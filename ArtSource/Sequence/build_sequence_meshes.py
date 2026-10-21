"""序列里直接摆的原作静态网格 → 项目资产（贴图设置、重建材质、网格挂材质）。先导入网格和贴图，再在 UE 里跑本脚本：
  UnrealEditor-Cmd Wuwa.uproject -run=ImportAssets -source="<SOURCE>/SM_VortexCloud.fbx" -dest=<FOLDER> -nosourcecontrol -replaceexisting
  UnrealEditor-Cmd Wuwa.uproject -run=ImportAssets -source="<SOURCE>/T_VortexCloud_Color.png" -dest=<FOLDER> -nosourcecontrol -replaceexisting（Flow 同）
  UnrealEditor-Cmd Wuwa.uproject -run=pythonscript -script="C:/UEProjects/Wuwa/ArtSource/Sequence/build_sequence_meshes.py"
（commandlet 里用 Python 的 AssetImportTask 导入会去同步内容浏览器，没有 Slate 直接断言崩溃，所以导入走 ImportAssets）

现在只有嘉贝大招特写 SC_Jiabeilina_Burst01 的漩涡云 SM_VortexCloud（原作 Render/RuntimeBP/GI/VortexCloud）：
  - 网格：Tools/Blender/uemodel_static_to_fbx.py 从 .uemodel 转的 FBX（厘米），导入不建材质；
  - 贴图：原作 color_02_Test_V03（漩涡颜色，sRGB）→ T_VortexCloud_Color、color_02_Test_V3_FlowMap（流动方向，线性）→ T_VortexCloud_Flow；
  - 材质：原作 MI_VortexCloud_001_MP（父材质 M_VortexCloud_MP，半透明 + 无光照）是编译后的，没有节点图，
    按参数重建：流动贴图扰动（FlowMap Speed_bak 0.36、DistortIntansity_bak 1.0，经典两相位交替采样），
    颜色 = 贴图 × Brightness。贴图 alpha 中心黑、外圈白，按 AlphaMode 决定当不透明度怎么用（对照视频选的，见 MATERIAL）。
原件放在 ArtSource/Sequence/SC_Jiabeilina_Burst01/（原作数据，不提交）。重跑会覆盖重建材质，网格 / 贴图已存在则沿用。
"""
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, 'SC_Jiabeilina_Burst01')
FOLDER = '/Game/Characters/SequenceCamera/2X/2_7/Jiabeilina/VortexCloud'

# 重建着色器：UV 上叠流动贴图的方向，两个相位交替采样消除跳变（相位差半个周期）
SHADER = """
float2 Flow = (Texture2DSample(FlowTex, FlowTexSampler, UV).rg * 2.0 - 1.0) * FlowStrength;
float Phase0 = frac(Time * FlowSpeed);
float Phase1 = frac(Time * FlowSpeed + 0.5);
float4 A = Texture2DSample(BaseTex, BaseTexSampler, UV - Flow * Phase0);
float4 B = Texture2DSample(BaseTex, BaseTexSampler, UV - Flow * Phase1);
float4 C = lerp(A, B, abs(1.0 - 2.0 * Phase0));
float Alpha = AlphaMode > 0.5 ? C.a : (AlphaMode < -0.5 ? 1.0 - C.a : 1.0);
return float4(C.rgb * Brightness, saturate(Alpha * Opacity));
"""

MATERIAL = {
    'name': 'M_VortexCloud',
    'instance': 'MI_VortexCloud_001',
    # 原作 MI_VortexCloud_001_MP 的参数（流动速度 / 强度）+ 重建选择（Brightness / Opacity / AlphaMode / FlowStrength 比例）
    'scalars': {'FlowSpeed': 0.359524, 'FlowStrength': 0.15, 'Brightness': 1.0, 'Opacity': 1.0, 'AlphaMode': 0.0},
}


def configure_texture(asset_name, srgb):
    texture = unreal.load_asset(f'{FOLDER}/{asset_name}')
    if not texture:
        raise RuntimeError(f'{FOLDER}/{asset_name} missing: import it with -run=ImportAssets first')
    texture.set_editor_property('srgb', srgb)
    if not srgb:
        texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
    return texture


def build_material(base, flow):
    lib = unreal.MaterialEditingLibrary
    path = f"{FOLDER}/{MATERIAL['name']}"
    material = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else \
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(MATERIAL['name'], FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(material)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('two_sided', True)
    custom = lib.create_material_expression(material, unreal.MaterialExpressionCustom, -300, 0)
    custom.set_editor_property('code', SHADER)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    names = ['UV', 'Time', 'BaseTex', 'FlowTex'] + list(MATERIAL['scalars'])
    inputs = []
    for n in names:
        entry = unreal.CustomInput()
        entry.set_editor_property('input_name', n)
        inputs.append(entry)
    custom.set_editor_property('inputs', inputs)
    uv = lib.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -800, -200)
    lib.connect_material_expressions(uv, '', custom, 'UV')
    time = lib.create_material_expression(material, unreal.MaterialExpressionTime, -800, -100)
    lib.connect_material_expressions(time, '', custom, 'Time')
    for i, (name, texture) in enumerate((('BaseTex', base), ('FlowTex', flow))):
        node = lib.create_material_expression(material, unreal.MaterialExpressionTextureObjectParameter, -800, 0 + i * 120)
        node.set_editor_property('parameter_name', name)
        node.set_editor_property('texture', texture)
        if texture is flow:
            node.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        lib.connect_material_expressions(node, '', custom, name)
    for i, (name, value) in enumerate(MATERIAL['scalars'].items()):
        node = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -800, 260 + i * 80)
        node.set_editor_property('parameter_name', name)
        node.set_editor_property('default_value', value)
        lib.connect_material_expressions(node, '', custom, name)
    rgb = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, 0, -60)
    rgb.set_editor_property('r', True); rgb.set_editor_property('g', True); rgb.set_editor_property('b', True); rgb.set_editor_property('a', False)
    alpha = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, 0, 60)
    for c in ('r', 'g', 'b'):
        alpha.set_editor_property(c, False)
    alpha.set_editor_property('a', True)
    lib.connect_material_expressions(custom, '', rgb, '')
    lib.connect_material_expressions(custom, '', alpha, '')
    lib.connect_material_property(rgb, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    instance_path = f"{FOLDER}/{MATERIAL['instance']}"
    instance = unreal.load_asset(instance_path) if unreal.EditorAssetLibrary.does_asset_exist(instance_path) else \
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(MATERIAL['instance'], FOLDER, unreal.MaterialInstanceConstant,
                                                                 unreal.MaterialInstanceConstantFactoryNew())
    lib.set_material_instance_parent(instance, material)
    unreal.EditorAssetLibrary.save_loaded_asset(instance, False)
    return material, instance


def main():
    base = configure_texture('T_VortexCloud_Color', True)
    flow = configure_texture('T_VortexCloud_Flow', False)
    mesh = unreal.load_asset(f'{FOLDER}/SM_VortexCloud')
    if not mesh:
        raise RuntimeError(f'{FOLDER}/SM_VortexCloud missing: import it with -run=ImportAssets first')
    material, instance = build_material(base, flow)
    mesh.set_material(0, instance)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
    unreal.log(f'VortexCloud: mesh {mesh.get_path_name()} bounds {mesh.get_bounds().box_extent}, material {instance.get_path_name()}')


main()
