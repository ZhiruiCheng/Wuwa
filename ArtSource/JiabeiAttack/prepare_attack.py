"""Build an attack recipe (<Stage>.json) for WuwaAttackEffectSetup, without touching UE assets.

Usage: python -I prepare_attack.py [--character jiabei|changli] <Stage>   (run prepare_evidence.py first)
Jiabei (default): Attack01..04 / Charge, recipe in this folder, -Character=jiabei.
Changli: Attack01..05, recipe in Changli/, -Character=changli. Paths per character are in characters.py.

Shader / mesh-motion / sprite-emission reconstruction is ChangliSlash/prepare_attack.py's, imported unchanged;
what differs is only Jiabei-specific bookkeeping:
- system / material names are stage-independent (NS_Reference_Jiabei_<NS suffix>), since several stages share
  the same original system (Dg_02_1 plays in the first three attacks);
- groups and their children come from Evidence/AttackFxReference.json instead of a hand-written CONFIG;
- Render/Common default textures that are not exported get generated solid placeholders (named in the recipe);
- layers the builder cannot do (ribbon, distortion) and non-Niagara children (decal, light, post process,
  ghost, material controller) are listed in skippedLayers / skippedChildren.
Lifetimes, intensities and sprite counts are not in the cooked export and stay authored reconstruction.
"""
from __future__ import annotations

import json
import math
import re
import shutil
import struct
import subprocess
import sys
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'ChangliSlash'))
import prepare_reference as ref  # noqa: E402
import prepare_attack as changli  # noqa: E402
sys.path.append(str(HERE))  # 放在最后：同名的 prepare_attack 仍取 ChangliSlash 那份
import vm_decode  # noqa: E402
import characters  # noqa: E402

CHARACTER, ARGS = characters.select()
EXPORTS = Path('C:/GamePakExtractor/Output/Exports')
PAKEXPORT = Path('C:/GamePakExtractor/Tools/PakExport/bin/out/PakExport.exe')
PREFIX = f"NS_Reference_{CHARACTER['title']}_"

# Render/Common 默认贴图没有导出图片：生成纯色占位（不是原图）。
PLACEHOLDERS = {
    'T_DefaultColorWhite_D': (255, 255, 255, 255), 'T_DefaultColorWhite_Linear_D': (255, 255, 255, 255),
    'T_DefaultColorBlack_D': (0, 0, 0, 255), 'T_DefaultBlack': (0, 0, 0, 255), 'T_DefaultGrayscaleBlack_G': (0, 0, 0, 255),
    'T_DefaultNormal_N': (128, 128, 255, 255),
    'T_Default_Matcap_Metal': (128, 128, 128, 255), 'T_OceanWave_Volume': (128, 128, 128, 255), 'T_Noise_Stormy': (128, 128, 128, 255),
}


def system_name(ns: str) -> str:
    # 原作长离的系统名本身带角色名（NS_Fx_Changli_R1a01_N_Dg），去掉重复的那一段
    return PREFIX + ns.replace('NS_Fx_', '', 1).removeprefix(CHARACTER['title'] + '_')


# ---- 嘉贝莉娜的重建修正（都是重建选择，不是原作数据）----
# 原作颜色曲线是 HDR（刀光 40,18,6.5 → 15,19,40；火花 10,1.85,0.6），库洛的色调映射下保留色相发光，
# UE 默认色调映射会直接压成纯白。按最大分量等比压到 COLOR_LIMIT 以内，保留色相。
COLOR_LIMIT = 2.5
SOFT_DOT = 'T_Generated_SoftDot'


def compress_colors(layer: dict) -> dict:
    curves = []
    for curve in layer.get('curves', []):
        binding = curve['semanticBinding']
        if ('.Color.ColorCurve' in binding or '.Scale RGBA.' in binding) and curve.get('samples'):
            rows = []
            for row in curve['samples']:
                peak = max(row[:3])
                k = COLOR_LIMIT / peak if peak > COLOR_LIMIT else 1.0
                rows.append([v * k for v in row[:3]] + list(row[3:]))
            curve = {**curve, 'samples': rows, 'hueKeptCompression': COLOR_LIMIT}
        curves.append(curve)
    return {**layer, 'curves': curves}


def texture_alpha_constant(texture_path: str) -> bool:
    """贴图的 alpha 全满（打包贴图：RGB 各存一张遮罩）。"""
    from PIL import Image
    png = EXPORTS / (texture_path.replace('/Game/', 'Client/Content/') + '.png')
    if not png.exists():
        return False
    image = Image.open(png)
    return 'A' not in image.getbands() or image.getchannel('A').getextrema()[0] >= 250


def adjusted_material(material: dict) -> tuple[dict, list[str]]:
    """打包贴图：Base_AlphaSwitch 选的是 RGB 通道、贴图 alpha 全满时，颜色也取所选通道的灰度
    （Base_bUseChannelAsEmissive=1，没覆盖 EmissiveSwitch 时用 AlphaSwitch 同一个通道），不再把其他通道的遮罩当颜色。"""
    known = material['knownEffectiveOverrides']
    alpha_switch = known['vectors'].get('Base_AlphaSwitch', {}).get('value', [0, 0, 0, 1])
    base = known['textures'].get('Base', {}).get('value')
    if not base or alpha_switch[3] != 0 or not texture_alpha_constant(base):
        return material, []
    vectors = dict(known['vectors'])
    scalars = dict(known['scalars'])
    scalars['Base_bUseChannelAsEmissive'] = {'value': 1, 'source': 'Jiabei reconstruction: packed channel texture'}
    if 'Base_EmissiveSwitch' not in vectors:
        vectors['Base_EmissiveSwitch'] = {'value': alpha_switch, 'source': 'Jiabei reconstruction: same channel as Base_AlphaSwitch'}
    return ({**material, 'knownEffectiveOverrides': {**known, 'vectors': vectors, 'scalars': scalars}},
            ['packed channel texture: colour from the Base_AlphaSwitch channel'])


# ---- 人工调整（JiabeiTuning.json，按系统 / 层，所有段共用；不是原作数据）----
# {"systems": {"R1a_Kaiqiang_01": {"scale": 1.0, "layers": {"Glow001": {"drop": true, "intensity": 2, "tint": [r,g,b],
#   "lifetime": 0.4, "emission": {...部分覆盖 sprite_emission 的字段...}, "meshScale": 0.5, "meshRotate": [x,y,z 度]}}}}}
# 系统名不带 NS_Reference_Jiabei_ 前缀；层名是配方里的层名（重名的带 _R2）。
TUNING_FILE = CHARACTER['tuning']


def load_tuning() -> dict:
    if not TUNING_FILE.exists():
        return {}
    return json.loads(TUNING_FILE.read_text(encoding='utf-8')).get('systems', {})


def load_texture_srgb() -> dict:
    """{"textureSrgb": {"T_Xxx": true}}：逐张覆盖贴图的 sRGB 判定。"""
    if not TUNING_FILE.exists():
        return {}
    return json.loads(TUNING_FILE.read_text(encoding='utf-8')).get('textureSrgb', {})


def load_decal_tuning() -> dict:
    """{"decals": {"Decal_11_02": {"fadeOut": 0.466}}}：贴花名是配方里的名字。"""
    if not TUNING_FILE.exists():
        return {}
    return json.loads(TUNING_FILE.read_text(encoding='utf-8')).get('decals', {})


MASTER_DEFAULTS = json.loads((HERE / 'Evidence/MasterDefaults.json').read_text(encoding='utf-8'))
SIMPLE_EFFECT_MP = '/Game/Aki/Render/Shaders/Effect/EffectMaterial/MeshParticle/M_SimpleEffect_MP'
ADVANCED_EFFECT_MP = '/Game/Aki/Render/Shaders/Effect/EffectMaterial/MeshParticle/M_AdvancedEffect_MP'
SPRITE_MASTERS = '/Game/Aki/Render/Shaders/Effect/EffectMaterial/Sprite/'
# master -> advanced features. Sprite masters have the same pixel pipeline (M_SimpleEffect_S checked against
# the compiled shader map of MI_Mask_Ring_001_S; M_AdvancedEffect_S assumed to match its MP sibling).
# 丝带母材质 M_SimpleEffect_R 的参数和 M_SimpleEffect_MP 同名同组（少了网格 WPO 一组，多了 EdgeLight / ZFade / DepthFade），
# 颜色 / 透明度按同一条管线重建；它自己的 shader map 没有逐条核对（重建选择）。
RIBBON_SIMPLE_EFFECT = '/Game/Aki/Render/Shaders/Effect/EffectMaterial/Ribbon/M_SimpleEffect_R'
REBUILT_MASTERS = {SIMPLE_EFFECT_MP: False, ADVANCED_EFFECT_MP: True,
                   SPRITE_MASTERS + 'M_SimpleEffect_S': False, SPRITE_MASTERS + 'M_AdvancedEffect_S': True,
                   RIBBON_SIMPLE_EFFECT: False}
# Masked effect materials clip at this value in the compiled shader (MI_Mask_Ring_001_S: alpha - 0.666633 < 0 -> discard).
MASKED_CLIP = 0.666633


def material_root(material: dict) -> str:
    return material['chainLeafToRoot'][-1]['asset']


def blend_mode(material: dict) -> str:
    for entry in material['chainLeafToRoot']:
        mode = (entry.get('basePropertyOverrides') or {}).get('BlendMode')
        if mode:
            return mode
    return 'BLEND_Translucent'


def mparam(material: dict, kind: str, name: str):
    """MI chain value, falling back to the master's default (CachedExpressionData)."""
    fallback = MASTER_DEFAULTS.get(material_root(material), {}).get(kind, {}).get(name)
    if fallback is None:
        fallback = [0.0, 0.0, 0.0, 0.0] if kind == 'vectors' else 0.0
    return ref.known(material, kind, name, fallback)


def is_rebuilt(material: dict) -> bool:
    return material_root(material) in REBUILT_MASTERS or material_root(material) in (DEPRECATED_MP, SUBUV_MP, BASE_REF)


def rebuilt_body(material: dict, bindings: dict, shader: str) -> str | None:
    root = material_root(material)
    if not is_rebuilt(material):
        return None
    head = shader[:shader.index('float2 baseUv')]
    if root == DEPRECATED_MP:
        return head + deprecated_body(material, head)
    if root == BASE_REF:
        return head + simple_effect_body(normalized_base_ref(material), bindings, head)
    return head + simple_effect_body(material, bindings, head, REBUILT_MASTERS.get(root, False))


def has_curve(var: str, prefix: str) -> bool:
    """着色器开头的动态参数变量是否来自原作曲线（LUT）。没有曲线时开头会写成 float dynNpM=0.0 的默认值，
    那不是原作数据：真实值是字节码里写的常量（VMDMP 占位符，由 fill_dmp_placeholders 填）。"""
    return re.search(r'\b' + var + r'\s*=\s*lerp\(', prefix) is not None

def simple_effect_body(material: dict, bindings: dict, prefix: str, advanced: bool = False) -> str:
    """M_SimpleEffect_MP rebuilt from the game's compiled pixel shader (ShaderArchive-Client_Chunk-PCD3D_SM5,
    shader map of MI_Trans_Daoguang_80573_MP1, disassembled with d3dcompiler). The order of operations is the
    original; which material parameter feeds each constant slot is read from the role of that slot in the code
    (parameter names are stripped from the cooked uniform-expression table).
    M_AdvancedEffect_MP (advanced=True; shader map of MI_Trans_Liudong_230079_MP) has the same pipeline plus Base_bClampY
    (clamp V only), Opacity_CenterPower (alpha *= pow(|N.V| or its inverse, power)) and the soft depth fade
    (UseDepthFade: alpha *= saturate((SceneDepth-PixelDepth)/DepthFade_FadeDistance)).
    Not rebuilt: screen-space UV, second UV channel, dissolve edge colour, inside/outside tint, rim colour, camera fades."""
    f = ref.f
    sw = lambda name: bool((material['knownEffectiveOverrides']['staticSwitches'].get(name) or {}).get('value', False))
    sc = lambda name: float(mparam(material, 'scalars', name))
    vc = lambda name: [float(x) for x in mparam(material, 'vectors', name)]

    def uv_block(var: str, prefix_name: str, rotation: str, pan: str, clamp: float, offset_name: str | None, clamp_y: float = 0.0):
        uv = vc(prefix_name + '_UV')
        offset = vc(offset_name)[:2] if offset_name else [0.0, 0.0]
        turns = sc(rotation)
        c, s = math.cos(turns * math.tau), math.sin(turns * math.tau)
        return (f'float2 {var}=UV-0.5;\n'
                f'{var}=float2(dot({var},float2({f(c)},{f(-s)})),dot({var},float2({f(s)},{f(c)})))+0.5;\n'
                f'{var}={var}*float2({f(uv[0])},{f(uv[1])})+({pan})*float2({f(uv[2])},{f(uv[3])})+float2({f(offset[0])},{f(offset[1])});\n'
                f'{var}=lerp({var},saturate({var}),{f(clamp)});\n'
                + (f'{var}.y=lerp({var}.y,saturate({var}.y),{f(clamp_y)});\n' if clamp_y else ''))

    # 动态材质参数：有曲线的用 LUT 变量（dynNpM），没有的先放占位符，由 apply_vm_mesh_values 按字节码里的常量替换
    def dyn(var, placeholder):
        return var if has_curve(var, prefix) else placeholder
    code = ('// M_SimpleEffect_MP: operation order from the compiled game shader; see simple_effect_body().\n'
            'float t=a*LIFETIME_SECONDS;\n'
            f'float4 dmp=float4({dyn("dyn0p1", "VMDMP_x")},{dyn("dyn0p2", "VMDMP_y")},{dyn("dyn0p3", "VMDMP_z")},VMDMP_w);\n'
            f'float3 dmp1=float3({dyn("dyn1p1", "VMDMP1_x")},{dyn("dyn1p2", "VMDMP1_y")},{dyn("dyn1p3", "VMDMP1_z")});\n')
    # Second: colour ramp, optionally desaturated (Second_Contorl.x) and scaled (Second_Contorl.y)
    if sw('UseSecondTex'):
        clamp = sc('Second_bClamp')
        contorl = vc('Second_Contorl')
        code += uv_block('uvS', 'Second', 'Second_Rotation', f'lerp(t+dmp.w,dmp.w,{f(clamp)})', clamp, 'Second_UV_Offset')
        code += ('float3 sec=Texture2DSample(TexSecond,TexSecondSampler,uvS).rgb;\n'
                 f'sec=lerp(sec,dot(sec,float3(0.3,0.59,0.11)).xxx,{f(contorl[0])})*{f(contorl[1])};\n')
    else:
        code += 'float3 sec=float3(1.0,1.0,1.0);\n'
    # Base: shape, Refine = pow(c,R.x)*R.y blended with c*R.z by R.w, then multiply / screen with the ramp
    clamp = sc('Base_bClamp')
    refine, emissive_switch, alpha_switch = vc('Base_Refine'), vc('Base_EmissiveSwitch'), vc('Base_AlphaSwitch')
    tint = vc('Base_ColorTint')
    clamp_y = sc('Base_bClampY') if advanced and sw('UseClampY') else 0.0
    code += uv_block('uvB', 'Base', 'Base_Rotation', f'lerp(t+dmp.x,dmp.x,{f(max(clamp, clamp_y))})', clamp, 'Base_UV_Offset', clamp_y)
    code += ('float4 base=Texture2DSample(TexBase,TexBaseSampler,uvB);\n'
             f'float3 c=lerp(base.rgb,dot(base,float4({",".join(f(x) for x in emissive_switch)})).xxx,{f(sc("Base_bUseChannelAsEmissive"))});\n'
             f'float3 p=step(1e-6,c)*pow(max(c,1e-6),{f(refine[0])})*{f(refine[1])};\n'
             f'float3 c3=lerp(p,c*{f(refine[2])},{f(refine[3])})+{f(sc("Base_Emissive_Add"))};\n'
             'float3 mulC=c3*sec;\nfloat3 scrC=sec+c3-sec*c3;\n'
             f'float3 col=lerp(mulC,scrC,{f(sc("Base_bAddSecond"))})*float3({f(tint[0])},{f(tint[1])},{f(tint[2])});\n')
    # Mask (alpha factor) and Dissolve (hard erosion driven by DynamicMaterialParameter1.x)
    if sw('UseMaskTex'):
        clamp = sc('Mask_bClamp')
        code += uv_block('uvM', 'Mask', 'Mask_Rotation', f'lerp(t+dmp.y,dmp.y,{f(clamp)})', clamp, 'Mask_UV_Offset')
        code += (f'float m=dot(Texture2DSample(TexMask,TexMaskSampler,uvM),float4({",".join(f(x) for x in vc("Mask_AlphaSwitch"))}));\n'
                 f'm=lerp(m,1.0-m,{f(sc("Mask_Invert"))})*{f(sc("Mask_Alpha_Multiply"))};\n')
    else:
        code += 'float m=1.0;\n'  # Mask_Alpha_Multiply scales the mask texture only (compiled sprite shader without mask)
    if sw('UseDissolve'):
        clamp = sc('Dissolve_bClamp')
        code += uv_block('uvD', 'Dissolve', 'DIssolve_Rotation', f'lerp(t+dmp1.z,dmp1.z+{f(sc("DIssolve_TimeOffset"))},{f(clamp)})', clamp, None)
        code += (f'uvD+=float2({f(sc("Dissolve_UOffset"))},{f(sc("Dissolve_VOffset"))});\n'
                 f'float d=dot(Texture2DSample(TexDissolve,TexDissolveSampler,uvD),float4({",".join(f(x) for x in vc("Dissolve_ChannelSwitch"))}));\n'
                 f'float field=lerp(d,m,{f(sc("Dissolve_MaskLerp"))});\n'
                 f'field=lerp(field,1.0-field,{f(sc("Dissolve_Invert"))});\n'
                 f'float S={f(sc("Dissolve_Smooth"))};\n'
                 f'float erosion=saturate(S*(field-1.0)+dmp1.x*{f(sc("Dissolve_Dissolve"))}*(S+1.0));\n')
    else:
        code += 'float erosion=1.0;\n'
    if advanced:
        code += (f'float nv=abs(NdotV);\nnv=lerp(nv,1.0-nv,{f(sc("Opacity_CenterMaskInvert"))});\n'
                 f'erosion*=pow(max(nv,1e-4),{f(sc("Opacity_CenterPower"))});\n')
        if sw('UseDepthFade'):
            code += (f'float df=saturate(DepthDelta/{f(max(sc("DepthFade_FadeDistance"), 1e-3))});\n'
                     f'erosion*=lerp(df,1.0-df,{f(sc("Use_InvertDepthFade"))});\n')
    code += (f'float shapeA=dot(base,float4({",".join(f(x) for x in alpha_switch)}))*{f(sc("Base_Alpha_Multiply"))};\n'
             'float alpha=saturate(erosion*m*shapeA*max(curveColor.a,0.0)*max(curveAlpha,0.0)*saturate(VertexAlpha));\n'
             'float3 color=col*max(curveColor.rgb,0.0)*max(Intensity,0.0);\n'
             'return float4(color,alpha);\n')
    return code


DEPRECATED_MP = '/Game/Aki/Render/Shaders/Effect/EffectMaterial/Deprecated/M_SimpleEffect_Deprecated'
SUBUV_MP = '/Game/Aki/Render/Shaders/Effect/EffectMaterial/MeshParticle/M_SubUV_MP'
BASE_REF = '/Game/Aki/Effect/Materials/Mat/M_SimpleEffect_Base_Ref'
# M_SimpleEffect_Base_Ref 用带空格的参数名；按含义映射到 M_SimpleEffect_MP 的名字后共用同一管线（未逐条对过它的着色器）
BASE_REF_NAMES = {'Base UV': 'Base_UV', 'Base Refine': 'Base_Refine', 'Base Emissive Add': 'Base_Emissive_Add',
                  'Second UV': 'Second_UV', 'Second Contorl': 'Second_Contorl', 'Dissolve UV': 'Dissolve_UV',
                  'Dissolve': 'Dissolve_Dissolve', 'Smooth': 'Dissolve_Smooth', 'Noise UV': 'Noise_UV', 'Noise Strength': 'Noise_Strength',
                  'Use Second Tex': 'UseSecondTex', 'Use Mask Tex': 'UseMaskTex', 'Use Dissolve': 'UseDissolve', 'Use Noise': 'UseNoise'}


def normalized_base_ref(material: dict) -> dict:
    o = material['knownEffectiveOverrides']
    renamed = {kind: {BASE_REF_NAMES.get(k, k): v for k, v in o[kind].items()} for kind in o}
    if (o['staticSwitches'].get('Base Clamp') or {}).get('value'):
        renamed['scalars']['Base_bClamp'] = {'value': 1.0}
    # 编译后的像素着色器（MI_Trans_Liudong_80487_MP）透明度取底图 R 通道 × Base Alpha Multiply，没有通道开关；
    # M_SimpleEffect_MP 的默认是 A。T_Smoke_001 的 A 全是 1、形状在 RGB 里，按 A 取会变成实心的灰环。
    renamed['vectors'].setdefault('Base_AlphaSwitch', {'value': [1.0, 0.0, 0.0, 0.0], 'source': 'compiled M_SimpleEffect_Base_Ref pixel shader'})
    chain = [dict(e) for e in material['chainLeafToRoot']]
    chain[-1]['asset'] = SIMPLE_EFFECT_MP  # 默认值按 M_SimpleEffect_MP 取
    return {**material, 'knownEffectiveOverrides': renamed, 'chainLeafToRoot': chain}


def deprecated_body(material: dict, prefix: str) -> str:
    """M_SimpleEffect_Deprecated（编译后 MI_Trans_Noise_30024_001_MP 的像素着色器）：UV 在顶点着色器里算
    （这里按 Base_UV 平铺 / 平移 + Base_UV_Offset），主贴图单通道经 Refine 后乘 Base_ColorTint 与粒子颜色；
    溶解阈值来自 DynamicMaterialParameter.x；alpha = 通道 × Base_Alpha_Multiply × 侵蚀 × 粒子 alpha。"""
    f = ref.f
    sw = lambda name: bool((material['knownEffectiveOverrides']['staticSwitches'].get(name) or {}).get('value', False))
    sc = lambda name: float(mparam(material, 'scalars', name))
    vc = lambda name: [float(x) for x in mparam(material, 'vectors', name)]
    uv, off, refine, tint = vc('Base_UV'), vc('Base_UV_Offset'), vc('Base_Refine'), vc('Base_ColorTint')
    dmpx = 'dyn0p1' if has_curve('dyn0p1', prefix) else 'VMDMPD_x'  # 没有曲线时用字节码常量；未写动态参数时着色器给 1
    code = ('// M_SimpleEffect_Deprecated: operation order from the compiled game shader; see deprecated_body().\n'
            'float t=a*LIFETIME_SECONDS;\n'
            f'float2 uvB=UV*float2({f(uv[0])},{f(uv[1])})+t*float2({f(uv[2])},{f(uv[3])})+float2({f(off[0])},{f(off[1])});\n'
            'float v=Texture2DSample(TexBase,TexBaseSampler,uvB).a;\n'  # 编译着色器取的是 .w（形状在 alpha 里）
            f'float p=step(1e-6,v)*pow(max(v,1e-6),{f(refine[0])})*{f(refine[1])};\n'
            f'float3 col=(lerp(p,v*{f(refine[2])},{f(refine[3])})+{f(sc("Base_Emissive_Add"))})*float3({f(tint[0])},{f(tint[1])},{f(tint[2])});\n')
    if sw('UseDissolve'):
        duv = vc('Dissolve_UV')
        code += (f'float2 uvD=UV*float2({f(duv[0])},{f(duv[1])})+t*float2({f(duv[2])},{f(duv[3])});\n'
                 f'float field=dot(Texture2DSample(TexDissolve,TexDissolveSampler,uvD),float4({",".join(f(x) for x in vc("Dissolve_ChannelSwitch"))}));\n'
                 f'field=lerp(field,1.0-field,{f(sc("Dissolve_Invert"))});\n'
                 f'float S={f(sc("Dissolve_Smooth"))};\n'
                 f'float erosion=saturate(S*(field-1.0)+{dmpx}*{f(sc("Dissolve_Dissolve"))}*(S+1.0));\n')
    else:
        code += 'float erosion=1.0;\n'
    code += (f'float alpha=saturate(v*{f(sc("Base_Alpha_Multiply"))}*erosion*max(curveColor.a,0.0)*max(curveAlpha,0.0)*saturate(VertexAlpha));\n'
             'float3 color=col*max(curveColor.rgb,0.0)*max(Intensity,0.0);\n'
             'return float4(color,alpha);\n')
    return code


UV_CENTERED = re.compile(r'(\w+Uv) = \(UV-0\.5\)\*float2\(([^)]*)\);')
SECOND_WEAK = 'lerp(float3(1.0,1.0,1.0),max(second.rgb,0.0),0.28)'


def jiabei_shader_rules(shader: str, bindings: dict, notes: list[str]) -> str:
    """对照 4K 视频后改的两条重建规则（仍是推断，不是原作材质图）：
    1. UV 按 UE 材质常见写法 UV*Tiling（再绕 0.5 旋转、加 Offset），不再以 0.5 为中心缩放。刀光圆环带 U 是径向、
       V 是环向；Mask_80019 的 Tiling 0.348 / Offset 0.486 这样才落在菱形中部，得到视频里外缘亮、两头收尖的粗月牙；
       以中心缩放时采到的是菱形边上的黑区，只剩几条细弧。
    2. Second 绑的是 T_Color_* 渐变图时，它就是颜色来源（这类层的颜色曲线多为白色），整份相乘而不是 28% 混入。"""
    shader, count = UV_CENTERED.subn(lambda m: f'{m.group(1)} = UV*float2({m.group(2)})-0.5;', shader)
    if count:
        notes.append('UV = UV*Tiling, rotated about 0.5, plus Offset (UE convention) instead of scaling about the centre')
    if bindings.get('TexSecond', '').startswith('T_Color_'):
        assert shader.count(SECOND_WEAK) == 1
        shader = shader.replace(SECOND_WEAK, 'max(second.rgb,0.0)')
        notes.append(f'Second {bindings["TexSecond"]} is a colour ramp: multiplied in fully')
    return shader


_NS_EXPORTS: dict[str, list] = {}


def ns_export(asset: str):
    if asset not in _NS_EXPORTS:
        path = EXPORTS / (asset.replace('/Game/', 'Client/Content/') + '.json')
        _NS_EXPORTS[asset] = json.loads(path.read_text(encoding='utf-8')) if path.exists() else None
    return _NS_EXPORTS[asset]


def vm_mesh_values(asset: str, handle: str) -> dict | None:
    """原作发射器编译后字节码里烘焙的真实值（vm_decode.py）：寿命、网格缩放、初始朝向、自转角速度、透明度倍数。"""
    data = ns_export(asset)
    if data is None:
        return None
    spawn, update = vm_decode.decode(data, handle, 'spawn'), vm_decode.decode(data, handle, 'update')
    if spawn is None:
        return None
    # 插值出生（InterpSpawnStartDt）的发射器在出生脚本里按"出生时刻在这一帧里的位置"在上一帧值和这一帧值之间插值
    # （lerp(上一帧, 这一帧, 出生时刻 × InverseDeltaTime)，Engine[4] = InverseDeltaTime）。没给这些值时比例是 0，取到的是
    # 读不到的上一帧值（读作 0），透明度会变成 0。取帧末（比例 1、剩余时间 0）= 稳定状态下的这一帧值。
    dt = 1e-4
    alpha_scale = {handle + '.AlphaScale': 1.0, handle + '.InterpSpawnStartDt': dt, 'Engine.DeltaTime': dt, 'Engine[4]': 1.0 / dt}
    # 曲线一律读 1（旋转速率读 0）：得到的是乘在曲线外面的常数部分；曲线本身由 mesh_wpo / 着色器的 LUT 负责
    unit = {'Rotation Rate.FloatCurve': 0.0}
    rest = vm_decode.evaluate(spawn, constants=alpha_scale, curves=unit, default_curve=1.0)
    alpha = rest
    if update is not None:
        # 有的发射器在更新脚本里才算出 Scale / 颜色（Initial.Scale × 曲线 × ScaleXY 之类）：喂出生结果跑一帧
        stepped = vm_decode.evaluate(update, inputs=dict(rest), constants={**alpha_scale, 'Engine.DeltaTime': 1e-4},
                                     curves=unit, default_curve=1.0)
        for key in ('Scale.x', 'Scale.y', 'Scale.z', 'Color.x', 'Color.y', 'Color.z', 'Color.w'):
            if key in stepped:
                rest[key] = stepped[key]
    values = {
        'lifetime': float(rest.get('Lifetime', 0.0)),
        'scale': [float(rest.get('Scale.' + c, 1.0)) for c in 'xyz'],
        'orientation': [float(rest.get('MeshOrientation.' + c, 0.0)) for c in 'xyzw'],
        'position': [float(rest.get('Position.' + c, 0.0)) for c in 'xyz'],
        'alphaMultiplier': float(alpha.get('Color.w', 1.0)),
        # 粒子颜色 = Initial.Color × 颜色曲线（曲线读 1 时就是 Initial.Color）；着色器里只有曲线那一项
        'colorMultiplier': [float(rest.get('Color.' + c, 1.0)) for c in 'xyz'],
        'dmp': {f'VMDMP{"" if i == 0 else "1"}_{c}': float(rest[f'DynamicMaterialParameter{"" if i == 0 else "1"}.{c}'])
                for i in (0, 1) for c in 'xyzw' if f'DynamicMaterialParameter{"" if i == 0 else "1"}.{c}' in rest},
    }
    if update is not None:
        dt = 1e-3
        identity = {'MeshOrientation.x': 0.0, 'MeshOrientation.y': 0.0, 'MeshOrientation.z': 0.0, 'MeshOrientation.w': 1.0, 'Lifetime': 1.0}
        turned = vm_decode.evaluate(update, inputs=identity, constants={'Engine.DeltaTime': dt}, curves={'Rotation Rate.FloatCurve': 1.0})
        x, y, z, w = (float(turned.get('MeshOrientation.' + c, 0.0)) for c in 'xyzw')
        length = math.sqrt(x * x + y * y + z * z)
        if length > 1e-9:
            values['spinPerRate'] = 2 * math.atan2(length, w) / dt  # rad/s per unit of the Rotation Rate curve
            values['spinAxis'] = [x / length, y / length, z / length]
    return values


_BURSTS: dict[str, dict] = {}
_BURST_STARTS: dict[str, dict] = {}   # 系统 → {发射器: 第一次瞬时爆发的时刻}


def flipbook_code(asset: str, layer: dict) -> str:
    """原作渲染器的 SubImageSize（如 2×2 翻页图集）：按出生脚本写的 SubImageIndex 选帧再采样。
    更新脚本只是照抄 SubImageIndex 时为出生时随机定帧（材质里用每粒子随机数），否则按寿命顺序播放。"""
    renderer = layer['renderers'][0] if layer.get('renderers') else {}
    props = (renderer or {}).get('properties') or renderer or {}
    size = props.get('SubImageSize') or {}
    cols, rows = int(size.get('X', 1)), int(size.get('Y', 1))
    if cols * rows <= 1:
        return ''
    data = ns_export(asset)
    update = vm_decode.decode(data, layer.get('vmHandle', layer['handle']), 'update') if data else None
    animated = bool(update) and str(update.writes.get('SubImageIndex', 'Particles.SubImageIndex')) != 'Particles.SubImageIndex'
    pick = 'a' if animated else 'saturate(ParticleRandom)'
    return (f'// Flipbook {cols}x{rows} (renderer SubImageSize); frame {"over life" if animated else "random per particle at spawn"} as in the original scripts\n'
            f'float sfr=floor({pick}*{cols * rows}.0*0.9999);\n'
            f'UV=(UV+float2(fmod(sfr,{cols}.0),floor(sfr/{cols}.0)))/float2({cols}.0,{rows}.0);\n')


def sprite_speed_scale(update, layer: dict) -> dict | None:
    """“按速度缩放 Sprite 尺寸”模块：SpriteSize.axis = Initial × lerp(最小缩放曲线(年龄), 最大值, saturate(|速度| / N))。
    识别字节码里这一结构，返回 {'binding': 最小缩放曲线, 'max': [x, y], 'divisor': N}；不是这种结构返回 None。"""
    by_name = {n: (e, d) for n, e, d in update.lines}
    result = {'max': []}
    for axis in 'xy':
        # 沿乘法链（Initial × … × Owner.Scale）找 lerp(曲线, 常数, 系数)
        lerp, stack, seen = None, [str(update.writes.get(f'SpriteSize.{axis}', '')).split('.')[0]], set()
        while stack and lerp is None:
            v = stack.pop()
            if v in seen or v not in by_name:
                continue
            seen.add(v)
            expr = by_name[v][0]
            lerp = re.fullmatch(r'lerp\((v\d+), ([-0-9.e]+|v\d+), (v\d+)\)', expr)
            if lerp is None and expr.startswith('mul('):
                stack += re.findall(r'v\d+', expr)
        curve = re.match(r'(.+?)::Sample\w*Curve\(', by_name.get(lerp.group(1), ('', []))[0]) if lerp else None
        if not curve:
            return None
        # 插值系数链：clamp(div(clamp(|v|, ...), N), 0, 1)
        divisor, stack, seen = None, [lerp.group(3)], set()
        while stack and divisor is None:
            v = stack.pop()
            if v in seen or v not in by_name:
                continue
            seen.add(v)
            e, deps = by_name[v]
            d = re.fullmatch(r'div\(v\d+, ([-0-9.e]+)\)', e)
            if d:
                divisor = float(d.group(1))
            stack += [str(x).split('.')[0] for x in deps]
        binding = f'{layer.get("vmHandle", layer["handle"])}.{curve.group(1)}'
        if divisor is None or not any(c['semanticBinding'] == binding and c.get('samples') for c in layer['curves']):
            return None
        result['binding'] = binding
        maximum = lerp.group(2)
        if maximum.startswith('v'):
            # 最大值是每粒子随机（mad(随机, A, B)，即 B~A+B）：重建取平均 B + A/2
            ranged = re.fullmatch(r'mad\(v\d+, ([-0-9.e]+), ([-0-9.e]+)\)', by_name.get(maximum, ('', []))[0])
            if not ranged:
                return None
            result['randomMax'] = True
            maximum = float(ranged.group(2)) + 0.5 * float(ranged.group(1))
        result['max'].append(float(maximum))
        result['divisor'] = divisor
    return result


def sprite_speed_size_wpo(layer: dict, info: dict) -> str:
    """按速度缩放 Sprite 尺寸（原作模块，见 sprite_speed_scale）：宽 / 长分别在最小缩放曲线与最大值之间按速率插值。
    速度朝向的 Sprite 长边沿速度方向：把面片顶点相对粒子中心的偏移拆成沿 / 垂直速度两部分分别缩放。"""
    curve = next(c for c in layer['curves'] if c['semanticBinding'] == info['binding'])
    rows = [[r[0]] for r in changli.domain_rows(curve)]
    code = (f'// Sprite size by speed from the original update script: Initial x lerp({info["binding"]}, max, saturate(speed/{info["divisor"]:g}))\n'
            f'float a=saturate(Age);\nfloat cp=a*{ref.f(changli.SAMPLES - 1)};\nint ci=min((int)floor(cp),{changli.SAMPLES - 2});\nfloat cf=cp-(float)ci;\n')
    code += ref.curve_hlsl('minScale', rows)
    code += (f'float k=saturate(Speed/{ref.f(info["divisor"])});\n'
             f'float fx=lerp(minScale,{ref.f(info["max"][0])},k);\n'
             f'float fy=lerp(minScale,{ref.f(info["max"][1])},k);\n'
             'float3 d=P-C;\n'
             'float3 dir=normalize(Dir+float3(0,0,1e-6));\n'
             'float3 along=dot(d,dir)*dir;\n'
             'return (d-along)*(fx-1.0)+along*(fy-1.0);\n')
    return code


def sprite_size_curve(update, layer: dict) -> str | None:
    """更新脚本里 SpriteSize 依赖的曲线（如 'Value.Scale Factor.FloatCurve'）对应的导出曲线绑定名。"""
    by_name = {n: (e, d) for n, e, d in update.lines}
    owners, stack, seen = set(), [update.writes.get('SpriteSize.x', '')], set()
    while stack:
        v = str(stack.pop()).split('.')[0]
        if v in seen or v not in by_name:
            continue
        seen.add(v)
        expr, deps = by_name[v]
        m = re.match(r'(.+?)::Sample\w*Curve\(', expr)
        if m:
            owners.add(m.group(1))
        stack += deps
    for owner in owners:
        binding = f'{layer.get("vmHandle", layer["handle"])}.{owner}'
        if any(c['semanticBinding'] == binding and c.get('samples') for c in layer['curves']):
            return binding
    return None


def sprite_size_wpo(layer: dict, binding: str) -> str:
    """Sprite 面片按尺寸曲线绕粒子中心缩放（世界空间）。Age 为归一化年龄，曲线按导出域重采样。"""
    curve = next(c for c in layer['curves'] if c['semanticBinding'] == binding)
    rows = [[r[0]] for r in changli.domain_rows(curve)]
    code = ('// Sprite size over life from the original update script: SpriteSize = Initial x ' + binding + '\n'
            f'float a=saturate(Age);\nfloat cp=a*{ref.f(changli.SAMPLES - 1)};\nint ci=min((int)floor(cp),{changli.SAMPLES - 2});\nfloat cf=cp-(float)ci;\n')
    code += ref.curve_hlsl('sizeScale', rows)
    code += 'return (P-C)*(max(sizeScale,0.0)-1.0);\n'
    return code


def di_curve_functions(data: list, script: dict, handle: str) -> dict:
    """脚本里每个曲线数据接口（CachedDefaultDataInterfaces 名字 → LUT）的采样函数；名字带 / 不带 Emitter. 与发射器名前缀都能查到。
    采样与 Niagara 的 LUT 一致：u = saturate((x - LUTMinTime) * LUTInvTimeRange) * (N - 1)，相邻两点线性插值。"""
    out = {}
    for binding in script['Properties'].get('CachedDefaultDataInterfaces', []):
        ref_name = binding['DataInterface']['ObjectName']
        # 'NiagaraDataInterfaceCurve'<系统>:<发射器>.<脚本>.<对象名>''：同名对象在出生 / 更新脚本里各有一份，按完整的外层（发射器.脚本）匹配
        object_name = ref_name.split('.')[-1].rstrip("'")
        owner_script = ref_name.split(':', 1)[-1].rsplit('.', 1)[0]
        di = next((o for o in data if o.get('Name') == object_name
                   and str((o.get('Outer') or {}).get('ObjectName', '')).rstrip("'").endswith(':' + owner_script)), None)
        if di is None or 'ShaderLUT' not in di.get('Properties', {}):
            continue
        p = di['Properties']
        lut = p['ShaderLUT']
        comps = 4 if 'Color' in di['Type'] else 1
        n = int(p.get('LUTNumSamplesMinusOne', len(lut) // comps - 1))
        tmin, inv = float(p.get('LUTMinTime', 0.0)), float(p.get('LUTInvTimeRange', 1.0))

        def sample(x, lut=lut, n=n, comps=comps, tmin=tmin, inv=inv):
            u = min(max((x - tmin) * inv, 0.0), 1.0) * n
            i = min(int(u), max(n - 1, 0))
            f = u - i
            values = [lut[i * comps + c] + (lut[min(i + 1, n) * comps + c] - lut[i * comps + c]) * f for c in range(comps)]
            return values if comps > 1 else values[0]
        name = binding['Name']
        for key in (name, name.replace('Emitter.', ''), name.replace(handle + '.', '', 1)):
            out[key] = sample
    return out


def emitter_births(data, system: dict, handle: str, horizon: float = 2.0) -> list[float] | None:
    """逐帧（1/60 秒）跑原作系统更新脚本（状态带到下一帧），返回每个粒子的出生年龄：数量取脚本自己的
    <发射器>.SpawnRate.SpawnOutputInfo.Count（含速率曲线与脚本里的倍率），发射器离开 Active（ExecutionState 0）后不再生成。
    例：长离第二段 yumao001 的发射器状态模块循环 0.15 秒、只跑一次，之后状态变为 3，原作只在开头 0.15 秒里生成。
    引擎输入：各发射器的生成数量缩放 In1[k] 取 1（yumao001 按 1 时与速率曲线积分 ×1.2 一致），其余（含粒子数）取 0；
    所以离开 Active 后是 Inactive（已有粒子活完）还是 Complete（清空）无法区分，表里的粒子按各自寿命活完（推断）。"""
    decoder = vm_decode.Decoder(system)
    decoder.setup_constants(system, interpolated=False, system=True)
    decoder.run()
    # 本发射器的生成数量缩放：它的速率曲线采样之后第一个 mul(v, In1[k])，按 1；其余引擎输入（含别的发射器的缩放）按 0
    exprs = [expr for _, expr, _ in decoder.lines]
    sample = next((i for i, e in enumerate(exprs) if e.startswith(f'{handle}.SpawnRate.FloatCurve::SampleCurve')), None)
    if sample is None:
        return None
    scale = next((m.group(1) for e in exprs[sample:] for m in [re.match(r'^mul\(v\d+, (In1\[\d+\])\)$', e)] if m), None)
    curves = di_curve_functions(data, system, handle)
    dt = 1 / 60

    def simulate(extra: dict) -> list[dict]:
        state, frames = {}, []
        for _ in range(int(horizon / dt)):
            out = vm_decode.evaluate(decoder, inputs={**state, **extra}, constants={'Engine.DeltaTime': dt}, curves=curves,
                                     default_curve=1.0, other_inputs=0.0)
            state.update({k: v for k, v in out.items() if not k.startswith('Out')})
            frames.append(out)
        return frames

    # 解码器里整数输入和浮点输入同名（In1[k]），缩放槽置 1 会同时改到某个整数状态输入：
    # 状态用全 0 输入那一遍判断，数量用缩放槽置 1 那一遍取
    states = simulate({})
    counts = simulate({scale: 1.0}) if scale else states
    births = []
    for frame, (st, ct) in enumerate(zip(states, counts)):
        execution, count = st.get(f'{handle}.ExecutionState'), ct.get(f'{handle}.SpawnRate.SpawnOutputInfo.Count')
        if execution is None or count is None:
            return None
        if int(execution) != 0:
            break
        births += [float(st.get(f'{handle}.Age', (frame + 1) * dt))] * int(count)
    return births


def vm_spawn_table(asset: str, handle: str) -> list[dict] | None:
    """原作按发射器时间持续生成的层（系统脚本里有 <发射器>.SpawnRate.FloatCurve，出生脚本按发射器年龄查尺寸 / 位置 / 颜色等曲线）：
    按生成速率曲线积分出每个粒子的出生时刻，在每个出生时刻用真实曲线跑一遍出生脚本，得到逐粒子参数表。
    出生脚本里 lerp(上一帧年龄, 这一帧年龄, 插值比例) 的两项都取出生时刻；随机数按粒子序号取确定性的低差异序列。"""
    data = ns_export(asset)
    if data is None:
        return None
    system = vm_decode.find_script(data, '', 'system_update')
    if system is None:
        return None
    rate = di_curve_functions(data, system, handle).get(f'{handle}.SpawnRate.FloatCurve')
    spawn_script = vm_decode.find_script(data, handle, 'spawn')
    spawn = vm_decode.decode(data, handle, 'spawn')
    if rate is None or spawn is None:
        return None
    # 发射器年龄：出生脚本里第一处 lerp(Engine[a], Engine[b], v) —— 上一帧 / 这一帧的发射器年龄
    ages = next((re.findall(r'Engine\[\d+\]', expr)[:2] for _, expr, _ in spawn.lines
                 if expr.startswith('lerp(Engine[') and expr.count('Engine[') >= 2), None)
    if not ages:
        return None
    curves = di_curve_functions(data, spawn_script, handle)
    # 出生时刻优先用原作系统脚本逐帧模拟（含倍率、发射器只在 Active 期间生成）；模拟不出来时退回按速率曲线积分
    births = emitter_births(data, system, handle)
    if births is None:
        dt, t, remainder, births = 1 / 60, 0.0, 0.0, []
        while t < 2.0 and len(births) < 64:
            remainder += max(float(rate(t + dt)), 0.0) * dt
            while remainder >= 1.0:
                births.append(t + dt)
                remainder -= 1.0
            t += dt
    births = births[:64]
    rows = []
    for index, born in enumerate(births):
        constants = {handle + '.AlphaScale': 1.0, handle + '.InterpSpawnStartDt': 1e-4, 'Engine.DeltaTime': 1e-4, 'Engine[4]': 1e4,
                     ages[0]: born, ages[1]: born, 'Engine[560]': 1.0, 'Engine[564]': 1.0, 'Engine[568]': 1.0}
        # 原作每个粒子的随机数不同：这里按序号取确定性的低差异序列（黄金比例），同一帧出生的粒子不会完全重叠
        out = vm_decode.evaluate(spawn, constants=constants, curves=curves, random=((index + 1) * 0.6180339887) % 1.0,
                                 exec_index=index, default_curve=1.0, other_inputs=1.0)
        g = lambda k: float(out.get(k, 0.0))
        row = {'delay': born, 'lifetime': g('Lifetime'), 'size': [g('SpriteSize.x'), g('SpriteSize.y')],
               'position': [g('Position.' + c) for c in 'xyz'], 'velocity': [g('Velocity.' + c) for c in 'xyz'],
               'color': [g('Color.' + c) for c in 'xyzw']}
        # 透明度为 0（循环结束后的出生时刻）或尺寸为 0 的粒子看不见，不放进表
        if row['color'][3] > 1e-3 and row['lifetime'] > 0 and abs(row['size'][0]) > 1e-3 and abs(row['size'][1]) > 1e-3:
            rows.append(row)
    return rows or None


def vm_sprite_emission(asset: str, layer: dict) -> dict | None:
    """Sprite 发射参数全部取自原作字节码：系统脚本的爆发数量；出生脚本（加一步更新）采样出的寿命、尺寸、
    位置分布、速度方向（圆锥 / 全向）与大小；更新脚本里的阻力。渲染器的对齐方式与锚点取自原作渲染器属性。"""
    data = ns_export(asset)
    handle = layer.get('vmHandle', layer['handle'])
    if data is None:
        return None
    spawn, update = vm_decode.decode(data, handle, 'spawn'), vm_decode.decode(data, handle, 'update')
    if spawn is None:
        return None
    if asset not in _BURSTS:
        _BURSTS[asset] = vm_decode.burst_counts(data)
    count = _BURSTS[asset].get(handle, (1, 1))[1]
    # Engine[560] / [564]：Owner.Scale 的 x / y（Sprite 尺寸乘组件缩放）。按 1 采样，若被引用则交给构建器乘 Owner.Scale
    constants = {handle + '.AlphaScale': 1.0, 'Engine[560]': 1.0, 'Engine[564]': 1.0, 'Engine[568]': 1.0}
    follows_scale = any('Engine[560]' in e or 'Engine[564]' in e for d in (spawn, update) if d for _, e, _ in d.lines)
    samples = []
    for r in (0.0, 0.2, 0.4, 0.6, 0.8, 0.999):
        for index in range(6):
            born = vm_decode.evaluate(spawn, constants=constants, random=r, exec_index=index, default_curve=1.0, other_inputs=1.0)
            if update is not None:
                step = vm_decode.evaluate(update, inputs=dict(born), constants={**constants, 'Engine.DeltaTime': 1e-4},
                                          random=r, exec_index=index, default_curve=1.0, other_inputs=1.0)
                for key in ('SpriteSize.x', 'SpriteSize.y'):
                    if abs(float(born.get(key, 0.0))) < 1e-6 and key in step:
                        born[key] = step[key]
            samples.append(born)
    get = lambda s, k: float(s.get(k, 0.0))
    life = [get(s, 'Lifetime') for s in samples]
    sx = [get(s, 'SpriteSize.x') for s in samples]
    sy = [get(s, 'SpriteSize.y') for s in samples]
    pos = [[get(s, 'Position.' + c) for c in 'xyz'] for s in samples]
    vel = [[get(s, 'Velocity.' + c) for c in 'xyz'] for s in samples]
    if max(life) <= 0 or max(abs(x) for x in sx) < 1e-3:
        return None
    offset = [sum(p[k] for p in pos) / len(pos) for k in range(3)]
    radii = [math.hypot(p[0] - offset[0], p[1] - offset[1]) for p in pos]
    heights = [p[2] - offset[2] for p in pos]
    speeds = [math.sqrt(sum(c * c for c in v)) for v in vel]
    emission = {'count': max(count, 1), 'lifetime': [min(life), max(life)],
                'sizeMin': [min(sx), min(sy)], 'sizeMax': [max(sx), max(sy)],
                'radius': [min(radii), max(radii)], 'height': max(heights) - min(heights),
                'speed': [min(speeds), max(speeds)], 'positionOffset': offset, 'sizeFollowsScale': follows_scale,
                'drag': 0.0, 'source': 'original bytecode (vm_decode) sampled over random values and particle index'}
    moving = [v for v, s in zip(vel, speeds) if s > 1e-3]
    if moving:
        mean = [sum(v[k] / max(math.sqrt(sum(c * c for c in v)), 1e-6) for v in moving) / len(moving) for k in range(3)]
        length = math.sqrt(sum(c * c for c in mean))
        if length < 0.3:
            emission['velocityMode'] = 2  # 方向分散到各处：全向
        else:
            axis = [c / length for c in mean]
            spread = max(math.degrees(math.acos(max(-1.0, min(1.0, sum(a * b for a, b in zip(axis, v)) / math.sqrt(sum(c * c for c in v))))))
                         for v in moving)
            emission.update({'velocityMode': 1, 'coneDirection': axis, 'coneAngle': spread})
        if update is not None:
            moved = vm_decode.evaluate(update, inputs={'Velocity.x': 100.0, 'Velocity.y': 0.0, 'Velocity.z': 0.0, 'Lifetime': 1.0,
                                                       'Mass': 1.0, 'Age': 0.0}, constants={**constants, 'Engine.DeltaTime': 0.01},
                                       default_curve=1.0, other_inputs=1.0)
            if 'Velocity.x' in moved:
                emission['drag'] = max(0.0, (1.0 - float(moved['Velocity.x']) / 100.0) / 0.01)
    emission['sizeCurve'] = sprite_size_curve(update, layer) if update is not None else None
    renderer = layer['renderers'][0] if layer.get('renderers') else {}
    props = renderer.get('properties') or renderer
    pivot = props.get('PivotInUVSpace') or {'X': 0.5, 'Y': 0.5}
    emission.update({'faceCamera': 'Custom' not in str(props.get('FacingMode', 'FaceCamera')),  # FaceCamera / FaceCameraPlane / FaceCameraPosition 都朝相机
                     'velocityAligned': str(props.get('Alignment', '')).endswith('VelocityAligned'),
                     'pivot': [pivot.get('X', 0.5), pivot.get('Y', 0.5)]})
    # 自定义对齐 / 朝向向量与 Sprite 旋转（度）：出生脚本写的常量（如长离大招画卷 Fire：对齐 (0,1,0)、朝向 (0,0,1)、旋转 180°）。
    # 不带这些时画卷按相机上方向摆、少转 180°，配合材质 UV 的翻转整张图上下颠倒（太阳跑到人物头顶上方）。
    def mean_vector(prefix):
        rows = [[get(s, f'{prefix}.{c}') for c in 'xyz'] for s in samples if f'{prefix}.x' in s]
        if not rows:
            return None
        v = [sum(r[k] for r in rows) / len(rows) for k in range(3)]
        n = math.sqrt(sum(c * c for c in v))
        return [round(c / n, 6) for c in v] if n > 1e-4 else None
    if str(props.get('Alignment', '')).endswith('CustomAlignment'):
        alignment = mean_vector('SpriteAlignment')
        if alignment:
            emission['customAlignment'] = alignment
    if 'Custom' in str(props.get('FacingMode', '')):
        facing = mean_vector('SpriteFacing')
        if facing:
            emission['spriteFacing'] = facing
    rotations = [get(s, 'SpriteRotation') for s in samples if 'SpriteRotation' in s]
    if rotations and max(rotations) - min(rotations) < 1e-3 and abs(rotations[0]) > 1e-3:
        emission['spriteRotation'] = round(rotations[0], 4)
    table = vm_spawn_table(asset, handle)
    if table:
        sizes = [r['size'] for r in table]
        emission.update({'table': table, 'count': len(table),
                         'lifetime': [min(r['lifetime'] for r in table), max(r['lifetime'] for r in table)],
                         'sizeMin': [min(s[0] for s in sizes), min(s[1] for s in sizes)],
                         'sizeMax': [max(s[0] for s in sizes), max(s[1] for s in sizes)],
                         'source': 'original bytecode: spawn-rate curve integrated, spawn script evaluated at each birth time with the real curve LUTs'})
    return emission


# 没写动态材质参数时引擎给的默认值（游戏着色器里 movc 的常量分支）：DMP=(x0,y0,z0,w1)，DMP1=(x1,y1,z0)
DMP_DEFAULTS = {'VMDMPD_x': 1.0, 'VMDMP_x': 0.0, 'VMDMP_y': 0.0, 'VMDMP_z': 0.0, 'VMDMP_w': 1.0, 'VMDMP1_x': 1.0, 'VMDMP1_y': 1.0, 'VMDMP1_z': 0.0}


def fill_dmp_placeholders(shader: str, vm: dict) -> str:
    dmp = vm.get('dmp', {})
    for key, default in DMP_DEFAULTS.items():
        value = dmp.get(key.replace('VMDMPD_', 'VMDMP_'), default)
        shader = re.sub(r'\b' + key + r'\b', ref.f(value), shader)
    return shader


def renderer_scale_variable(rp: dict | None) -> str:
    """网格渲染器的 ScaleBinding 绑定的粒子变量名（默认 Scale；Kaiqiang_01 Empty001 的第二个渲染器是 Size02）。"""
    name = (((rp or {}).get('ScaleBinding') or {}).get('ParamMapVariable') or {}).get('Name') or 'Particles.Scale'
    return name.split('.', 1)[1]


def bound_scale_values(asset: str, handle: str, variable: str) -> tuple[list[float], bool] | None:
    """非 Scale 的缩放绑定：字节码里该变量的常数部分（曲线读 1），以及它是否在更新脚本里乘了 Particles.Scale。"""
    data = ns_export(asset)
    spawn, update = vm_decode.decode(data, handle, 'spawn'), vm_decode.decode(data, handle, 'update')
    if spawn is None or f'{variable}.x' not in spawn.writes:
        return None
    rest = vm_decode.evaluate(spawn, curves={'Rotation Rate.FloatCurve': 0.0}, default_curve=1.0)
    uses_scale = False
    if update is not None and f'{variable}.x' in update.writes:
        rest = vm_decode.evaluate(update, inputs=dict(rest), constants={'Engine.DeltaTime': 1e-4},
                                  curves={'Rotation Rate.FloatCurve': 0.0}, default_curve=1.0)
        by_name = {n: (e, d) for n, e, d in update.lines}
        stack, seen = [str(update.writes[f'{variable}.{c}']).split('.')[0] for c in 'xyz'], set()
        while stack:
            v = stack.pop()
            if v in seen or v not in by_name:
                continue
            seen.add(v)
            expr, deps = by_name[v]
            uses_scale |= 'Particles.Scale.' in expr
            stack += [str(d).split('.')[0] for d in deps]
    return [float(rest.get(f'{variable}.{c}', 1.0)) for c in 'xyz'], uses_scale


def apply_vm_mesh_values(definition: dict, asset: str, layer: dict, rp: dict | None = None) -> None:
    handle = layer.get('vmHandle', layer['handle'])
    values = vm_mesh_values(asset, handle)
    if not values or values['lifetime'] <= 0:
        return
    variable = renderer_scale_variable(rp)
    bound_curve = None
    if variable != 'Scale':
        bound = bound_scale_values(asset, handle, variable)
        curve = changli.find(layer, f'.Particles.{variable}.VectorCurve')
        if bound is None or not bound[1] or curve is None:
            raise RuntimeError(f'{definition["id"]}: scale binding Particles.{variable} has an unsupported form')
        # 例：Size02 = Size02 曲线 × (Scale.x, 2.5·Scale.y, 2.5·Scale.z)，Scale = Initial.Scale × Scale Factor 曲线。
        # 常数部分替换粒子 Scale；Scale Factor 曲线已在 WPO 里，再乘上该变量自己的曲线。
        values = {**values, 'scale': bound[0]}
        bound_curve = curve
        definition.setdefault('jiabeiAdjustments', []).append(
            f'renderer ScaleBinding = Particles.{variable}: constant {[round(v, 4) for v in bound[0]]} × {variable} curve × Scale Factor curve (from bytecode)')
    definition['vm'] = values
    definition['lifetime'] = round(values['lifetime'], 4)
    wpo = definition['wpo']
    if not wpo:
        wpo = changli.mesh_wpo_stub() if hasattr(changli, 'mesh_wpo_stub') else (
            '// No motion curves.\nfloat theta=0.0;\nfloat3 scaled=P;\nfloat sn=sin(theta);\nfloat cs=cos(theta);\n'
            'float3 turned=float3(scaled.x*cs-scaled.y*sn,scaled.x*sn+scaled.y*cs,scaled.z);\nreturn turned-P;\n')
    # 自转：原作每帧绕父空间 Z 转 RotationRate(t)*spinPerRate*dt（vm 更新脚本）。重建按“圈/秒×2π×寿命”积分，换成真实系数
    spin = values.get('spinPerRate')
    if spin is not None and abs(values['spinAxis'][2]) > 0.999:
        wpo, n = re.subn(r'integratedRate\*[-0-9.e]+', f'integratedRate*{ref.f(values["lifetime"] * spin)}', wpo)
        if n:
            definition.setdefault('jiabeiAdjustments', []).append(f'spin = RotationRate * {spin:.4g} rad/s about Z (from bytecode)')
    # 缩放、初始朝向：Niagara 网格渲染 = 朝向(自转 ⊗ 初始) × (缩放 × 顶点)；自转在下面的 turned 里
    sx, sy, sz = values['scale']
    qx, qy, qz, qw = values['orientation']
    insert = (f'// From the original bytecode: particle Scale ({sx:.4g},{sy:.4g},{sz:.4g}), initial MeshOrientation quaternion.\n'
              f'scaled*=float3({ref.f(sx)},{ref.f(sy)},{ref.f(sz)});\n'
              f'{{float3 qv=float3({ref.f(qx)},{ref.f(qy)},{ref.f(qz)}); float qw={ref.f(qw)};\n'
              f' scaled=scaled+2.0*cross(qv,cross(qv,scaled)+qw*scaled);}}\n')
    if bound_curve is not None:
        assert 'int ci=' in wpo, definition['id']  # 曲线 LUT 依赖 WPO 开头的 ci / cf
        insert = (ref.curve_hlsl('boundScale', changli.domain_rows(bound_curve))
                  + 'scaled*=boundScale;\n' + insert)
    lines = wpo.rstrip('\n').split('\n')
    at = [i for i, line in enumerate(lines) if line.startswith('float3 scaled=')]
    assert len(at) == 1, definition['id']
    # 网格空间 PivotOffset：Niagara 把它和顶点一起乘粒子缩放（NiagaraMeshParticleUtils：Translation += Rot(MeshOffset × Scale)），
    # 所以缩放 / 朝向都绕粒子位置、作用在 (顶点 + Pivot) 上。WPO 里的缩放曲线不在粒子 Scale 上，这里把 Pivot 并进来，渲染器的 Pivot 清零。
    pivot = definition.get('pivot') or [0, 0, 0]
    if not definition['sprite'] and any(abs(c) > 1e-6 for c in pivot):
        shifted = 'float3(' + ','.join(f'P.{c}+{ref.f(v)}' for c, v in zip('xyz', pivot)) + ')'
        lines[at[0]] = f'float3 Q={shifted};\n' + lines[at[0]].replace('P.x*', 'Q.x*').replace('P.y*', 'Q.y*').replace('P.z*', 'Q.z*').replace('scaled=P;', 'scaled=Q;')
        assert 'Q' in lines[at[0]].split('\n')[1], definition['id']
        definition['sourcePivotOffset'] = pivot
        definition['pivot'] = [0, 0, 0]
        definition.setdefault('jiabeiAdjustments', []).append(
            f'mesh PivotOffset {pivot} scaled with the particle (folded into WPO, renderer pivot 0)')
    definition['wpo'] = '\n'.join(lines[:at[0] + 1]) + '\n' + insert + '\n'.join(lines[at[0] + 1:]) + '\n'
    px, py, pz = values['position']
    if abs(px) + abs(py) + abs(pz) > 1e-4:
        # 粒子初始位置（发射器局部空间）：顶点 = 位置 + 朝向 × (缩放 × 顶点)。位置给粒子本身（构建器的出生赋值），
        # WPO 只做朝向 / 缩放；以前并进 WPO 时粒子停在系统原点，挂相机的系统粒子就落在相机上而整层不渲染。
        definition['particlePosition'] = [round(px, 4), round(py, 4), round(pz, 4)]
    apply_vm_color(definition, values)
    definition.setdefault('jiabeiAdjustments', []).append(
        f'lifetime {values["lifetime"]:.3g}s, scale {values["scale"]}, orientation {[round(q, 4) for q in values["orientation"]]}, '
        f'alpha x{values["alphaMultiplier"]:.3g} (from bytecode)')


def apply_vm_color(definition: dict, values: dict) -> None:
    """粒子颜色的常数部分（字节码里的 Initial.Color 等，曲线读 1）：alpha 乘到 curveAlpha，RGB 乘到最终颜色。"""
    def patch(anchor: str, replacement: str, label: str) -> None:
        if definition['shader'].count(anchor) == 1:
            definition['shader'] = definition['shader'].replace(anchor, replacement)
            definition.setdefault('jiabeiAdjustments', []).append(label)
        else:
            definition.setdefault('jiabeiAdjustments', []).append(f'NOT applied ({anchor} not found): {label}')
    multiplier = values['alphaMultiplier']
    if abs(multiplier - 1.0) > 1e-4:
        patch('max(curveAlpha,0.0)', f'max(curveAlpha*{ref.f(multiplier)},0.0)', f'particle alpha x{multiplier:.4g} (from bytecode)')
    rgb = values.get('colorMultiplier') or [1.0, 1.0, 1.0]
    if any(abs(c - 1.0) > 1e-4 for c in rgb):
        patch('max(curveColor.rgb,0.0)', f'max(curveColor.rgb*float3({",".join(ref.f(c) for c in rgb)}),0.0)',
              f'particle colour x{[round(c, 4) for c in rgb]} (from bytecode)')


def apply_layer_tuning(definition: dict, tuning: dict) -> None:
    if 'intensity' in tuning:
        definition['intensity'] = tuning['intensity']
    if 'lifetime' in tuning and not definition['sprite']:
        definition['lifetime'] = tuning['lifetime']
    if 'meshScale' in tuning and not definition['sprite']:
        # 原作粒子的初始网格缩放在 cooked 导出里没有；在 WPO 里按网格原点整体缩放
        s = float(tuning['meshScale'])
        wpo = definition.get('wpo', '')
        if not wpo:
            definition['wpo'] = f'// JiabeiTuning meshScale\nfloat3 scaled=P;\nreturn scaled*{ref.f(s)}-P;\n'
        else:
            assert wpo.count('return turned-P;') == 1, definition['id']
            definition['wpo'] = wpo.replace('return turned-P;', f'return turned*{ref.f(s)}-P;')
    if 'meshRotate' in tuning and not definition['sprite']:
        # 原作网格粒子的初始朝向同样不在 cooked 导出里。顺序：原轴上的 Scale Factor → 这里的固定朝向（按 X、Y、Z
        # 依次转，度）→ 重建的绕 Z 自转，即自转轴是摆正之后的竖直轴
        wpo = definition.get('wpo', '') or 'float3 scaled=P;\nreturn scaled-P;\n'
        lines = wpo.rstrip('\n').split('\n')
        at = [i for i, line in enumerate(lines) if line.startswith('float3 scaled=')]
        assert len(at) == 1, definition['id']
        rotate = [f'// JiabeiTuning meshRotate {tuning["meshRotate"]}']
        for axis, degrees in zip('xyz', tuning['meshRotate']):
            if not degrees:
                continue
            u, v = {'x': ('y', 'z'), 'y': ('z', 'x'), 'z': ('x', 'y')}[axis]
            rad = math.radians(float(degrees))
            c, s = ref.f(math.cos(rad)), ref.f(math.sin(rad))
            rotate.append(f'{{float pu=scaled.{u}, pv=scaled.{v}; scaled.{u}=pu*{c}-pv*{s}; scaled.{v}=pu*{s}+pv*{c};}}')
        definition['wpo'] = '\n'.join(lines[:at[0] + 1] + rotate + lines[at[0] + 1:]) + '\n'
    if 'emission' in tuning and definition['sprite']:
        definition['emission'] = {**definition['emission'], **tuning['emission'], 'source': 'JiabeiTuning.json'}
    if 'tint' in tuning:
        r, g, b = tuning['tint']
        anchor = 'float3 color=sourceColor*'
        assert definition['shader'].count(anchor) == 1, definition['id']
        definition['shader'] = definition['shader'].replace(anchor, f'float3 color=float3({ref.f(r)},{ref.f(g)},{ref.f(b)})*sourceColor*')
    definition['tuning'] = {k: v for k, v in tuning.items() if k != 'note'}


def make_soft_dot(path: Path, size: int = 64) -> None:
    """柔边白色圆点（alpha 径向渐变），代替缺失底图的纯白方块。"""
    from PIL import Image
    image = Image.new('RGBA', (size, size))
    c = (size - 1) / 2
    for y in range(size):
        for x in range(size):
            d = min(1.0, (((x - c) ** 2 + (y - c) ** 2) ** .5) / c)
            image.putpixel((x, y), (255, 255, 255, int(255 * (1 - d) ** 2)))
    image.save(path)


def make_solid(path: Path, rgba) -> None:
    def chunk(kind, payload):
        return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload) & 0xffffffff)
    row = b'\x00' + bytes(rgba) * 2
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 2, 2, 8, 6, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(row * 2)) + chunk(b'IEND', b'')
    path.write_bytes(png)


def texture_entry(name: str, package: str) -> dict:
    dest = HERE / 'Textures' / (name + '.png')
    png = EXPORTS / (package + '.png')
    if not png.exists():
        if name not in PLACEHOLDERS:
            raise FileNotFoundError(f'{png} (and no placeholder defined for {name})')
        make_solid(dest, PLACEHOLDERS[name])
        return {'name': name, 'file': 'Textures/' + dest.name, 'srgb': name.endswith('_D') and 'Linear' not in name,
                'substitute': True, 'substituteColor': list(PLACEHOLDERS[name])}
    if not dest.exists() or ref.sha(dest) != ref.sha(png):
        shutil.copy2(png, dest)
    srgb = None
    metadata = EXPORTS / (package + '.json')
    if metadata.exists():
        data = ref.read(metadata)
        objects = data if isinstance(data, list) else [data]
        texture = next((o for o in objects if o.get('Type') == 'Texture2D'), None)
        # UE 只序列化和默认值不同的属性：Texture2D 存在而没有 SRGB 字段 = 默认的 true
        srgb = texture.get('Properties', {}).get('SRGB', True) if texture is not None else None
    if srgb is None:
        # 新版本客户端包里大多数特效贴图是流式资源，导不出属性。导得出的 38 张（Color / Mask / Tile / Trail / Wenli /
        # LaunchScene）全部 SRGB 开、BC7，只有 Normal 目录的法线贴图关（TC_Normalmap、BC5）：没有属性时按这条规律判定。
        lower = package.lower()
        srgb = not ('/normal/' in lower or lower.endswith('_n') or 'flowmap' in lower)
    entry = {'name': name, 'file': 'Textures/' + dest.name, 'srgb': bool(srgb), 'source': str(png), 'sha256': ref.sha(dest)}
    # 导出里没有贴图属性 JSON 时 sRGB 只能按目录猜（只有 /Color/ 当 sRGB）；调整文件 textureSrgb 可以逐张覆盖
    override = load_texture_srgb().get(name)
    if override is not None and bool(override) != entry['srgb']:
        entry['srgb'] = bool(override)
        entry['srgbSource'] = 'tuning textureSrgb (not original data)'
    return entry


def apply_da_user_parameters(system: dict) -> dict:
    """特效 DA（EffectModelNiagara）的 FloatParameters：库洛运行时按名字写进 Niagara 的 User.<Key>
    （如 R1air_a03_Trail 的 Alpha = 1，系统默认是 0）。常量照用；曲线取 t = 0 的值（重建选择，运行时变化没有烘焙）。
    挂到缓存的导出数据上，vm_decode.user_parameter_defaults 会读到。"""
    values = {}
    for entry in system['dataAssetExplicitProperties'].get('FloatParameters') or []:
        field = entry.get('Value') or {}
        if field.get('bUseCurve'):
            keys = (field.get('Curve') or {}).get('EditorCurveData', {}).get('Keys') or []
            if keys:
                values[entry['Key']] = float(keys[0]['Value'])
        elif 'Constant' in field:
            values[entry['Key']] = float(field['Constant'])
    data = ns_export(system['asset'])
    if data is not None:
        data[:] = [o for o in data if o.get('Type') != 'WuwaUserParameterOverrides']
        if values:
            data.append({'Type': 'WuwaUserParameterOverrides', 'Values': values})
    return values


def ribbon_definition(asset: str, layer: dict, rp: dict) -> dict:
    """丝带层（NiagaraRibbonRendererProperties，如长离空中攻击 2/3 的武器拖尾 R1air_a03_Trail/NE_Fx_Base003）：
    原作字节码里的寿命、宽度（Initial.RibbonWidth × Ribbon Width Scale 曲线）、SpawnPerUnit（每厘米出生数，系统更新脚本里
    count = |发射器速度| × k × dt）。颜色 / 透明度 / 动态参数照常走着色器 LUT（丝带的粒子相对时间就是归一化年龄）。"""
    handle = layer.get('vmHandle', layer['handle'])
    data = ns_export(asset)
    spawn = vm_decode.decode(data, handle, 'spawn')
    dt = 1e-4
    born = vm_decode.evaluate(spawn, constants={handle + '.InterpSpawnStartDt': dt, 'Engine.DeltaTime': dt, 'Engine[4]': 1.0 / dt},
                              default_curve=1.0) if spawn else {}
    lifetime = float(born.get('Lifetime', 0.5))
    base_width = float(born.get('Initial.RibbonWidth', born.get('RibbonWidth', 10.0)))
    curve = changli.find(layer, '.Ribbon Width Scale.FloatCurve')
    scale = [float(row[0] if isinstance(row, list) else row) for row in (curve['samples'] if curve else [])] or [1.0]
    widths = [round(base_width * v, 4) for v in scale]
    # SpawnPerUnit：喂一个 100 cm/s 的发射器速度、dt = 1，数出生数
    system = vm_decode.find_script(data, '', 'system_update')
    per_unit = None
    if system is not None:
        dec = vm_decode.Decoder(system)
        dec.setup_constants(system, interpolated=False, system=True)
        out = vm_decode.evaluate(dec, inputs={'In1[4]': 100.0, 'In1[5]': 0.0, 'In1[6]': 0.0, 'In1[2]': 0.0},
                                 constants={'Engine.DeltaTime': 1.0}, default_curve=1.0, other_inputs=0.0)
        count = out.get(handle + '.SpawnPerUnit.SpawnOutputInfo.Count')
        if count is not None:
            per_unit = float(count) / 100.0 or None
        # 普通 SpawnRate：dt = 1 秒时的出生数就是每秒出生数
        rate_count = out.get(handle + '.SpawnRate.SpawnOutputInfo.Count')
        rate = float(rate_count) if rate_count else None
    else:
        rate = None
    uv = rp.get('UV0Settings') or {}
    return {'lifetime': round(lifetime, 4), 'widths': widths, 'baseWidth': base_width,
            'spawnPerUnit': per_unit, 'spawnRate': rate if not per_unit else None,
            'uvTilingLength': float(uv.get('TilingLength', 1.0) or 1.0),
            'uvDistribution': str(uv.get('DistributionMode', '')).rsplit('::', 1)[-1],
            'source': 'original bytecode: spawn script Lifetime / RibbonWidth, system update SpawnPerUnit count; width curve resampled on normalized age'}


VAT_FPS = 30.0  # Houdini 顶点动画默认帧率；原作寿命由发射器参数 LifetimeToParticle 传入，字节码里取不到


def vat_texture(name: str, package: str) -> dict:
    """顶点动画贴图（TC_HDR / PF_FloatRGBA）：从 uexp 里取原样的半精度像素写成 Textures/<名字>.bin（PNG / HDR 导出会丢 A 通道）。"""
    uexp = EXPORTS / (package + '.uexp')
    if not uexp.exists():
        subprocess.run([str(PAKEXPORT), 'raw', package + '.uexp'], check=True, capture_output=True)
    png = EXPORTS / (package + '.png')
    if not png.exists():
        subprocess.run([str(PAKEXPORT), 'export', package + '.uasset'], check=True, capture_output=True)
    from PIL import Image
    width, height = Image.open(png).size
    data = uexp.read_bytes()
    size = width * height * 8
    # 批量数据头：int32 元素数、int32 磁盘大小（都等于像素字节数）、int64 偏移，之后就是像素
    at = data.find(struct.pack('<ii', size, size))
    if at < 0 or at + 16 + size > len(data):
        raise ValueError(f'{uexp}: no {width}x{height} RGBA16F block')
    dest = HERE / 'Textures' / (name + '.bin')
    dest.write_bytes(data[at + 16:at + 16 + size])
    return {'name': name, 'file': 'Textures/' + dest.name, 'srgb': False, 'format': 'rgba16f', 'width': width, 'height': height,
            'source': str(uexp), 'sha256': ref.sha(dest)}


def rigid_body_vat(layer: dict, textures: dict) -> bool:
    """原作 MI_PBREffect_RigidBodyVAT_MP（Houdini 刚体顶点动画）：碎块轴心 = (UV1.x, UV1.y, UV2.x)（模型空间厘米），UV2.y = 贴图列；
    LocationTex 每帧 RGB = 轴心位移、A = 缩放（UseLocTexAlphaAsScale），RotationTex = 四元数（xyzw）。帧位置 = 动态材质参数 2 的曲线 × (帧数 - 1)。
    母材质是 PBR 受光的岩石；这里按深色无光照岩石 + 边缘紫光重建（大招时场景压暗，视频里碎石近乎黑色）。"""
    material = layer.get('sourceMaterial', '')
    if '/RigidBody/' not in material:
        return False
    package = material.replace('/Game/', 'Client/Content/', 1)
    props = export_objects(package)[0].get('Properties', {})
    if 'RigidBodyVAT' not in (props.get('Parent') or {}).get('ObjectPath', ''):
        return False
    tex = {v['ParameterInfo']['Name']: v['ParameterValue']['ObjectPath'].rsplit('.', 1)[0] for v in props.get('TextureParameterValues', [])}
    scalars = {v['ParameterInfo']['Name']: v['ParameterValue'] for v in props.get('ScalarParameterValues', [])}
    vectors = {v['ParameterInfo']['Name']: v['ParameterValue'] for v in props.get('VectorParameterValues', [])}
    frames = int(scalars.get('VertexAnim_Frame', 100))
    names = {}
    for key in ('LocationTex', 'RotationTex'):
        name = tex[key].rsplit('/', 1)[1]
        textures.setdefault(name, vat_texture(name, tex[key]))
        names[key] = name
    rock = tex['RockDiffuseTex'].rsplit('/', 1)[1]
    textures.setdefault(rock, texture_entry(rock, tex['RockDiffuseTex']))
    tint = vectors.get('RockDiffuseColorTint', {'R': 1, 'G': 1, 'B': 1})
    # 帧曲线：发射器更新脚本里 Index 2 Param 1 的曲线（动态材质参数 2.y），按归一化年龄采样
    system = layer['sourceSystem'].replace('/Game/', 'Client/Content/', 1)
    emitter = layer['sourceEmitter']
    lut = next(o['Properties']['ShaderLUT'] for o in export_objects(system)
               if o.get('Type') == 'NiagaraDataInterfaceCurve' and f':{emitter}.UpdateScript' in o.get('Outer', {}).get('ObjectName', ''))
    n = len(lut)
    layer.update({
        'blend': 'masked', 'lifetime': round(frames / VAT_FPS, 4),
        'textures': {'TexBase': rock},
        'vat': {**names, 'frames': frames, 'frameCurveSamples': n, 'fps': VAT_FPS},
        'wpo': (
            '// Rigid-body VAT (original MI_PBREffect_RigidBodyVAT_MP data; formula = Houdini VAT 3 rigid convention)\n'
            f'const float frameLut[{n}] = {{{",".join(ref.f(x) for x in lut)}}};\n'
            f'float lp=saturate(Age)*{n - 1}.0; int li=min((int)floor(lp),{n - 2}); float lf=lp-(float)li;\n'
            f'float frame=lerp(frameLut[li],frameLut[li+1],lf)*{frames - 1}.0;\n'
            f'float f0=floor(frame); float ff=frame-f0; float f1=min(f0+1.0,{frames - 1}.0);\n'
            f'float2 c0=float2(UV2.y,(f0+0.5)/{frames}.0); float2 c1=float2(UV2.y,(f1+0.5)/{frames}.0);\n'
            'float4 l=lerp(Texture2DSampleLevel(LocationTex,LocationTexSampler,c0,0),Texture2DSampleLevel(LocationTex,LocationTexSampler,c1,0),ff);\n'
            'float4 q0=Texture2DSampleLevel(RotationTex,RotationTexSampler,c0,0);\n'
            'float4 q1=Texture2DSampleLevel(RotationTex,RotationTexSampler,c1,0);\n'
            'q1=dot(q0,q1)<0.0?-q1:q1;\n'
            'float4 q=normalize(lerp(q0,q1,ff));\n'
            'float3 pivot=float3(UV1.x,UV1.y,UV2.x);\n'
            'float3 v=(P-pivot)*l.w;\n'
            'float3 t=2.0*cross(q.xyz,v);\n'
            'float3 r=v+q.w*t+cross(q.xyz,t);\n'
            'return pivot+l.xyz+r-P;\n'),
        'shader': (
            '// Dark unlit rock (reconstruction of the lit PBR rock in the darkened burst scene)\n'
            'float4 rock=Texture2DSample(TexBase,TexBaseSampler,UV);\n'
            'float rim=pow(saturate(1.0-abs(NdotV)),3.0);\n'
            f'float3 c=rock.rgb*float3({ref.f(tint["R"])},{ref.f(tint["G"])},{ref.f(tint["B"])})*0.06+rim*float3(0.2,0.12,0.45);\n'
            'return float4(c,1.0);\n'),
    })
    layer.setdefault('jiabeiAdjustments', []).append(
        f'rigid-body VAT: {frames} frames over {frames / VAT_FPS:.2f}s (assumed {VAT_FPS:.0f} fps), frame curve from {emitter} Index 2 Param 1; '
        'dark unlit rock + purple rim instead of the lit PBR rock')
    return True


def build_systems(parameters: dict, textures: dict, meshes: set, skipped: list) -> list[dict]:
    materials = {m['leaf']: m for m in parameters['materials']}
    tuning_all = load_tuning()
    systems = []
    for system in parameters['systems']:
        ns = system['asset'].rsplit('/', 1)[1]
        name = system_name(ns)
        suffix = name[len('NS_Reference_'):]
        system_tuning = tuning_all.get(name[len(PREFIX):], {})
        layer_tuning = system_tuning.get('layers', {})
        user_overrides = apply_da_user_parameters(system)
        built = []
        used_names: set[str] = set()
        for layer in system['layers']:
            if not layer['enabled']:
                continue
            for renderer in layer.get('renderers', []):
                if renderer is None:
                    continue
                # 一个发射器可以挂多个渲染器（例如 Kaiqiang_01 的 Empty001 有两个网格渲染器）；构建器要求层名唯一
                handle = layer['handle']
                unique = handle
                n = 2
                while unique in used_names:
                    unique = f'{handle}_R{n}'
                    n += 1
                layer = {**layer, 'handle': unique, 'vmHandle': handle}  # 同一发射器的第二个渲染器：字节码 / 曲线仍按原发射器名查
                rp = renderer['properties']
                if renderer['type'] not in ('NiagaraSpriteRendererProperties', 'NiagaraMeshRendererProperties', 'NiagaraRibbonRendererProperties'):
                    skipped.append({'system': name, 'layer': layer['handle'], 'reason': f'{renderer["type"]} not supported by the builder'})
                    continue
                ribbon = renderer['type'] == 'NiagaraRibbonRendererProperties'
                # 丝带（武器拖尾）的着色器和 Sprite 一样按顶点 alpha / 归一化年龄取值，不走网格 WPO
                sprite = renderer['type'] == 'NiagaraSpriteRendererProperties' or ribbon
                matref = rp.get('Material') if sprite else (rp.get('OverrideMaterials') or [{}])[0].get('ExplicitMat')
                if not matref:
                    skipped.append({'system': name, 'layer': layer['handle'], 'reason': 'renderer has no material'})
                    continue
                matpath = matref['ObjectPath'].rsplit('.', 1)[0].replace('Client/Content/', '/Game/')
                material = materials.get(matpath)
                if material is None:
                    skipped.append({'system': name, 'layer': layer['handle'], 'reason': f'material {matpath} not in evidence'})
                    continue
                if changli.is_distortion(material):
                    skipped.append({'system': name, 'layer': layer['handle'], 'reason': 'distortion (refraction) material not reconstructed'})
                    continue
                mesh = None
                if not sprite:
                    mesh = rp['ParticleMesh']['ObjectPath'].rsplit('.', 1)[0].split('/')[-1]
                    if not (HERE / 'Geometry' / (mesh + '.json')).exists():
                        skipped.append({'system': name, 'layer': layer['handle'], 'reason': f'geometry {mesh} not converted'})
                        continue
                definition = {'name': layer['handle'], 'sprite': sprite and not ribbon, 'sortOrder': rp.get('SortOrderHint', 0),
                              'pivot': [rp.get('PivotOffset', {}).get(k, 0) for k in ('X', 'Y', 'Z')],
                              'id': f'Reference_{suffix}_{layer["handle"]}',
                              'sourceMaterial': matpath, 'sourceSystem': system['asset'], 'sourceEmitter': layer['instance']}
                # 发射器推后爆发（原作发射器状态的循环延迟 / 爆发时刻）：构建器在 spawnTime 才爆发
                if system['asset'] not in _BURST_STARTS:
                    ns_data = ns_export(system['asset'])
                    _BURST_STARTS[system['asset']] = vm_decode.burst_start_times(ns_data) if ns_data else {}
                spawn_time = _BURST_STARTS[system['asset']].get(layer.get('vmHandle', layer['handle']), 0.0)
                if spawn_time > 1e-3 and not ribbon:
                    definition['spawnTime'] = round(spawn_time, 4)
                original = material
                material, notes = adjusted_material(material)
                bindings = {}
                # M_Effect（如 MI_Add_Ring_20011 彩虹光环）的贴图参数叫 MainTex / SecondTex / FlowTex / MaskTex / DissolutionTex；
                # 母材质没重建，走通用着色器，只把贴图按用途对上（重建选择）。
                master = material['chainLeafToRoot'][-1]['asset'].rsplit('/', 1)[1]
                if master == 'M_Refraction_MP' and ref.known(material, 'textures', 'Base', ref.WHITE).endswith(ref.WHITE):
                    # 没有 Base 贴图的折射材质（如 MI_Laser_Glass 玻璃碎片）只扭曲背景，项目不做折射：按扭曲层跳过，否则会画成整片白色
                    skipped.append({'system': name, 'layer': layer['handle'], 'reason': 'refraction-only material (no Base texture) not reconstructed'})
                    continue
                if master == 'M_Refraction_MP' and not sprite:
                    # 原作 M_Refraction_MP 的网格粒子顶点着色器（shader map 3022cebe…，3_freq0）把面片按 UV 铺满屏幕：
                    # 视空间 ((UV-0.5)·2·Scale·tan(半视场)·D, 同上·高宽比, D)，preshader 默认 Scale=1、D=15、Fit=0（纯拉伸铺满）。
                    # 例：长离大招 BurstBw03 的黑白闪屏 T_EChangli_190001_Bw03 / Bw04，各 0.08 / 0.16 秒。
                    definition['screenQuad'] = {'scale': 1.0, 'distance': 15.0, 'fit': 0.0,
                                                'source': 'M_Refraction_MP vertex shader (shader map 3022cebe, preshader cb3[40].w / [41].x / [41].y)'}
                aliases = ({'Base': 'MainTex', 'Second': 'SecondTex', 'Noise': 'FlowTex', 'Mask': 'MaskTex', 'Dissolve': 'DissolutionTex'}
                           if master == 'M_Effect' else {})
                bound = material['knownEffectiveOverrides']['textures']
                if aliases:
                    notes.append('M_Effect texture names mapped onto the generic shader (MainTex→Base, MaskTex→Mask, ...)')
                for key in ('Base', 'Second', 'Noise', 'Mask', 'Dissolve'):
                    param = aliases.get(key, key)
                    texture = ref.known(material, 'textures', param, '/Game/Aki/Render/Common/' + ref.WHITE)
                    texture_name = texture.rsplit('/', 1)[1]
                    if key == 'Base' and param not in bound:
                        # 材质实例没绑底图（例如 M_SimpleEffect_Deprecated 的纯色粒子）：用柔边圆点，不用纯白方块
                        texture_name = SOFT_DOT
                        notes.append('no Base texture bound: generated soft dot instead of white')
                        if SOFT_DOT not in textures:
                            dest = HERE / 'Textures' / (SOFT_DOT + '.png')
                            make_soft_dot(dest)
                            textures[SOFT_DOT] = {'name': SOFT_DOT, 'file': 'Textures/' + dest.name, 'srgb': False,
                                                  'substitute': True, 'substituteDescription': 'generated 64x64 white soft dot'}
                    bindings['Tex' + key] = texture_name
                    if texture_name not in textures:
                        textures[texture_name] = texture_entry(texture_name, texture.replace('/Game/', 'Client/Content/'))
                definition['textures'] = bindings
                # 重建了母材质的层用原始 HDR 颜色曲线（如 An02 的 (20,9,3)）；压缩到 2.5 只是旧近似材质的补救
                rebuilt = is_rebuilt(material)
                definition['shader'], dynamic = changli.dynamic_shader(material, layer if rebuilt else compress_colors(layer), sprite)
                body = rebuilt_body(original, bindings, definition['shader'])
                if body:
                    definition['shader'] = body
                    definition['intensity'] = 1.0
                    notes.append(f'{material_root(original).rsplit(chr(47), 1)[1]} body rebuilt from the compiled game shader (operation order original)')
                else:
                    definition['shader'] = jiabei_shader_rules(definition['shader'], bindings, notes)
                # 重建选择：材质本身也会把亮度乘到很高（例如通道权重 (1,3,2)、Refine ×3、+1），库洛的色调映射保留色相，
                # UE 默认的会把高亮压成白色。最终输出按最大分量等比压到 COLOR_LIMIT 以内（和颜色曲线的压缩同一个规则）。
                # Tuning 里系统级 "colorLimit": null 关掉这条压缩（原作 HDR 颜色直接进色调映射）。
                limit = system_tuning.get('colorLimit', COLOR_LIMIT)
                final = 'return float4(color,alpha);'
                if limit is None:
                    notes.append('final colour cap off (tuning colorLimit null): raw HDR colour')
                elif definition['shader'].count(final) == 1:
                    definition['shader'] = definition['shader'].replace(
                        final, f'float peak=max(color.r,max(color.g,color.b));\ncolor*=peak>{ref.f(limit)}?{ref.f(limit)}/peak:1.0;\n' + final)
                    notes.append(f'final colour hue-kept compression to max component {limit}')
                noise_texture = bindings.get('TexNoise', '')
                if noise_texture.endswith(ref.WHITE.rsplit('/', 1)[-1]) or noise_texture.endswith('T_DefaultColorWhite_D'):
                    # 没绑噪声图（默认白图）时通用着色器的扰动 (rg-0.5)·强度 变成固定 +0.06 的 UV 平移：
                    # 整屏黑白帧在右边 / 下边 6% 处露出贴图重复的接缝（视频里没有）。没有噪声图就不扰动。
                    lines = [l for l in definition['shader'].split('\n') if not re.match(r'^\w+Uv \+= noise\*', l)]
                    if len(lines) != len(definition['shader'].split('\n')):
                        definition['shader'] = '\n'.join(lines)
                        notes.append('noise distortion dropped: TexNoise is the white default (would be a constant UV shift)')
                if 'screenQuad' in definition and definition['shader'].count('*fadeIn*fadeOut*breakup') == 1:
                    # 通用着色器的淡入 / 淡出 / 溶解是重建时补的（原作 M_Refraction_MP 没有）；0.08 秒的整屏黑白帧会因此大半时间半透明，
                    # 视频 268.2–268.35 是不透明的整屏画面
                    definition['shader'] = definition['shader'].replace('*fadeIn*fadeOut*breakup', '')
                    notes.append('screen quad: authored fade-in / fade-out / dissolve removed (opaque full-screen frame)')
                mode = blend_mode(original)
                definition['blend'] = {'BLEND_Additive': 'additive', 'BLEND_Masked': 'masked'}.get(mode, 'translucent')
                if definition['blend'] == 'masked':
                    definition['clip'] = MASKED_CLIP
                if notes:
                    definition['jiabeiAdjustments'] = notes
                if dynamic:
                    definition['dynamicParameterCurves'] = dynamic
                definition['reconstruction'] = ('Shader wiring, lifetime, intensity, fade/dissolve are authored (ChangliSlash reconstruction); '
                                                'colour/alpha LUTs resampled over their exported domain on normalizedAge.')
                if ribbon:
                    definition['intensity'] = 1.0 if is_rebuilt(material) else .8
                    definition['wpo'] = ''
                    definition['ribbon'] = ribbon_definition(system['asset'], layer, rp)
                    definition['lifetime'] = definition['ribbon']['lifetime']
                    values = vm_mesh_values(system['asset'], layer.get('vmHandle', layer['handle']))
                    if values:
                        apply_vm_color(definition, values)
                elif sprite:
                    definition['intensity'] = 1.0 if is_rebuilt(material) else .8
                    definition['wpo'] = ''
                    definition['emission'] = vm_sprite_emission(system['asset'], layer) or changli.sprite_emission(layer)
                    if definition['emission'].get('table'):
                        # 逐粒子表的层：颜色 / 透明度是出生时按发射器时间查的（写在粒子颜色里），不是按粒子年龄变化的曲线
                        shader = definition['shader']
                        color_line = re.search(r'float4 curveColor = lerp\(curveColorLut\[ci\], curveColorLut\[ci\+1\], cf\);', shader)
                        alpha_line = re.search(r'float curveAlpha = lerp\(curveAlphaLut\[ci\], curveAlphaLut\[ci\+1\], cf\);', shader)
                        if not color_line or not alpha_line:
                            raise RuntimeError(f"{definition['id']}: colour / alpha curve lines not found for the per-particle table")
                        shader = shader.replace(color_line.group(0), 'float4 curveColor = float4(VertexColor.rgb, 1.0);  // per-particle colour (table)')
                        shader = shader.replace(alpha_line.group(0), 'float curveAlpha = 1.0;  // per-particle alpha is in VertexAlpha (table)')
                        definition['shader'] = shader
                        definition.setdefault('jiabeiAdjustments', []).append(
                            f"per-particle table: {len(definition['emission']['table'])} particles from the spawn-rate curve, colour/alpha from particle colour")
                    sprite_values = vm_mesh_values(system['asset'], layer.get('vmHandle', layer['handle']))
                    if sprite_values:
                        apply_vm_color(definition, sprite_values)
                    sprite_update = vm_decode.decode(ns_export(system['asset']), layer.get('vmHandle', layer['handle']), 'update') \
                        if ns_export(system['asset']) else None
                    speed_scale = sprite_speed_scale(sprite_update, layer) if sprite_update else None
                    if speed_scale:
                        definition['wpo'] = sprite_speed_size_wpo(layer, speed_scale)
                        definition['emission'] = {**definition['emission'], 'sizeCurve': None, 'sizeBySpeed': speed_scale}
                    elif definition['emission'].get('sizeCurve'):
                        definition['wpo'] = sprite_size_wpo(layer, definition['emission']['sizeCurve'])
                else:
                    meshes.add(mesh)
                    definition['mesh'] = mesh
                    definition['lifetime'] = changli.LIFETIME[changli.kind(layer['handle'])]
                    if not is_rebuilt(material):  # 重建母材质时没有额外的强度倍数
                        definition['intensity'] = changli.INTENSITY[changli.kind(layer['handle'])]
                    definition['wpo'], motion = changli.mesh_wpo(layer, definition['lifetime'])
                    if motion:
                        definition['motionReconstruction'] = motion
                    apply_vm_mesh_values(definition, system['asset'], layer, rp)
                # UE 的 Panner 按秒走：偏移 = 速度 × 已存活秒数。ChangliSlash 重建里乘的是 a*0.15（a 为 0~1 的归一化年龄），
                # 0.5 秒寿命的层几乎不动，渐变图（T_Color_*）停在顶端那一种颜色上。按 a*寿命 换回秒。
                seconds = definition.get('lifetime') or 0.5
                definition['shader'], panners = re.subn(r'\*a\*0\.15?\b', f'*a*{ref.f(seconds)}', definition['shader'])
                if panners:
                    definition.setdefault('jiabeiAdjustments', []).append(f'panner speed applied per second (a*{seconds}) instead of a*0.15')
                flip = flipbook_code(system['asset'], layer) if sprite and not ribbon else ''
                if flip:
                    sh = definition['shader']
                    at = sh.find('// M_SimpleEffect')
                    at = at if at >= 0 else sh.find('float2 baseUv')
                    definition['shader'] = sh[:at] + flip + sh[at:]
                definition['shader'] = definition['shader'].replace('LIFETIME_SECONDS', ref.f(definition.get('lifetime') or 0.5))
                definition['shader'] = fill_dmp_placeholders(definition['shader'], definition.get('vm') or {})
                used_names.add(unique)
                tuned = layer_tuning.get(unique, {})
                if tuned.get('drop'):
                    skipped.append({'system': name, 'layer': unique, 'reason': 'dropped by JiabeiTuning.json: ' + tuned.get('note', '')})
                    continue
                apply_layer_tuning(definition, tuned)
                built.append(definition)
        entry = {'name': name, 'source': system['asset'], 'generate': True, 'layers': built}
        if system_tuning.get('scale'):
            entry['tuningScale'] = system_tuning['scale']
        location = system['dataAssetExplicitProperties'].get('Location', {}).get('Constant')
        if location:
            entry['modelLocation'] = [location.get(k, 0.0) for k in ('X', 'Y', 'Z')]
        systems.append(entry)
    return systems


DECAL_MASTER = 'Client/Content/Aki/Render/Shaders/Effect/EffectMaterial/MeshParticle/M_Decal_MP'


def export_objects(package: str) -> list:
    data = json.loads((EXPORTS / (package + '.json')).read_text(encoding='utf-8'))
    return data if isinstance(data, list) else [data]


def material_chain_values(package: str) -> dict:
    """材质实例链（叶 → 根）的有效参数，叶优先；根母材质 CachedExpressionData 的默认值垫底。"""
    values = {'scalars': {}, 'vectors': {}, 'textures': {}, 'switches': {}, 'componentMasks': {}, 'chain': []}
    rgba = lambda v: [float(v[c]) for c in 'RGBA']
    while package:
        obj = export_objects(package)[0]
        p = obj.get('Properties') or {}
        values['chain'].append(package)
        if obj['Type'] == 'Material':
            P = p['CachedExpressionData']['Parameters']
            for entry, kind, convert in (('RuntimeEntries', 'scalars', float), ('RuntimeEntries[1]', 'vectors', rgba),
                                         ('RuntimeEntries[2]', 'textures', lambda t: t['ObjectPath'].rsplit('.', 1)[0])):
                defaults = P[{'scalars': 'ScalarValues', 'vectors': 'VectorValues', 'textures': 'TextureValues'}[kind]]
                for info, value in zip(P[entry]['ParameterInfos'], defaults):
                    values[kind].setdefault(info['Name'], convert(value))
            break
        for e in p.get('ScalarParameterValues', []):
            values['scalars'].setdefault(e['ParameterInfo']['Name'], float(e['ParameterValue']))
        for e in p.get('VectorParameterValues', []):
            values['vectors'].setdefault(e['ParameterInfo']['Name'], rgba(e['ParameterValue']))
        for e in p.get('TextureParameterValues', []):
            values['textures'].setdefault(e['ParameterInfo']['Name'], e['ParameterValue']['ObjectPath'].rsplit('.', 1)[0])
        for e in (p.get('StaticParameters') or {}).get('StaticSwitchParameters', []):
            values['switches'].setdefault(e['ParameterInfo']['Name'], bool(e['Value']))
        for e in (p.get('StaticParameters') or {}).get('StaticComponentMaskParameters', []):
            values['componentMasks'].setdefault(e['ParameterInfo']['Name'], {c: bool(e.get(c)) for c in 'RGBA'})
        parent = p.get('Parent')
        package = parent['ObjectPath'].rsplit('.', 1)[0] if parent else None
    return values


def rich_curve_keys(curve: dict) -> list[dict]:
    return [{'time': k['Time'], 'value': k['Value'], 'interp': k.get('InterpMode', 'RCIM_Linear'),
             'arrive': k.get('ArriveTangent', 0.0), 'leave': k.get('LeaveTangent', 0.0)} for k in curve.get('Keys', [])]


def decal_definition(da_package: str, textures: dict) -> dict:
    """EffectModelDecal（原作 DA_Fx_Decal_*）→ 贴花材质 + 模型参数。材质按 M_Decal_MP 编译后的贴花像素着色器
    （MI_Decal_80005_MP 的 shader map，ShaderArchive-Client_Chunk-PCD3D_SM5）的运算顺序重建：
      颜色 = Emissive 贴图（可选通道去色）→ Emissive_Refine（pow / 乘 / lerp）× Emissive_ColorTint × Emissive_Intensity；
      透明 = Mask 通道 × Mask_Alpha_Multiply × Base 通道 × Base_Alpha_Multiply × 侵蚀 saturate(S(d-1)+D(S+1)) × Z 向淡出。
    UV 都是贴花投影 UV，先绕中心旋转（圈）再平铺。DA 里有曲线的材质参数做成材质参数，由 EffectModelDecal 每帧写入；
    其余参数烘焙为常量。未重建（非默认值时记入 unsupported）：噪声扰动、平移、屏幕 UV、反相、溶解遮罩混合、渐变映射。"""
    f = ref.f
    da = export_objects(da_package)[0]['Properties']
    material_package = da['DecalMaterialRef']['ObjectPath'].rsplit('.', 1)[0]
    m = material_chain_values(material_package)
    if m['chain'][-1] == DECAL_001_MASTER:
        return decal_001_definition(da_package, da, material_package, m, textures)
    if m['chain'][-1] != DECAL_MASTER:
        raise ValueError(f'{da_package}: decal master {m["chain"][-1]} is not M_Decal_MP')
    sc, vc, sw = m['scalars'].get, m['vectors'].get, m['switches'].get
    float_curves = {e['Key']: e['Value'] for e in da.get('MaterialFloatParameters', [])}
    color_curves = {e['Key']: e['Value'] for e in da.get('MaterialColorParameters', [])}
    unsupported = [n for n in ('Emissive_NoiseStrength', 'Mask_NoiseStrength', 'Dissolve_NoiseStrength', 'Base_NoiseStrength',
                               'Dissolve_MaskLerp', 'Dissolve_Invert', 'Mask_Invert',
                               'Dissolve_bScreenUV', 'Mask_bScreenUV', 'NiagaraShiftColorParam') if abs(sc(n, 0.0)) > 1e-6]
    unsupported += [n for n in ('Emissive_UV', 'Mask_UV', 'Dissolve_UV', 'Base_UV') if any(abs(x) > 1e-6 for x in vc(n, [1, 1, 0, 0])[2:])]
    # 材质参数（曲线驱动）用 HLSL 输入变量，其余写常量
    exposed_scalars = {name: sc(name, 0.0) for name in float_curves}
    exposed_vectors = {name: vc(name, [1, 1, 1, 1]) for name in color_curves}
    exposed_scalars.update({'ZFadingFactor': float(da.get('ZfadingFactor', 1.0)), 'ZFadingPower': float(da.get('ZfadingPower', 1.0))})
    s = lambda name: name if name in exposed_scalars else f(sc(name, 0.0))
    v4 = lambda name: name if name in exposed_vectors else 'float4(' + ','.join(f(x) for x in vc(name, [0, 0, 0, 0])) + ')'

    bindings = {}

    def bind(key: str) -> bool:
        package = m['textures'].get(key)
        if not package or package.endswith(('/T_DefaultColorWhite_D', '/DefaultTexture')):
            return False
        name = package.rsplit('/', 1)[1]
        if name not in textures:
            textures[name] = texture_entry(name, package)
        bindings['Tex' + key] = name
        return True

    def uv(var: str, prefix: str, rotation: str, clamp: str) -> str:
        tiling = vc(prefix + '_UV', [1, 1, 0, 0])
        turns = sc(rotation, 0.0)
        c, sn = math.cos(turns * math.tau), math.sin(turns * math.tau)
        return (f'float2 {var}=UV-0.5;\n'
                f'{var}=float2(dot({var},float2({f(c)},{f(-sn)})),dot({var},float2({f(sn)},{f(c)})))+0.5;\n'
                f'{var}={var}*float2({f(tiling[0])},{f(tiling[1])});\n'
                f'{var}=lerp({var},saturate({var}),{f(sc(clamp, 0.0))});\n')

    refine = vc('Emissive_Refine', [1, 1, 1, 0.5])
    code = '// M_Decal_MP: operation order from the compiled decal pixel shader; see prepare_attack.decal_definition().\n'
    if sw('UseEmissive', False) and bind('Emissive'):
        code += uv('uvE', 'Emissive', 'Emissive_Rotation', 'Emissive_bClamp')
        # 编译后的贴花像素着色器（MI_Decal_80005_MP）：采样后先 lerp(rgb, dot(采样, Emissive_ChannelSwitch), Emissive_bUseChannelAsEmissive)
        code += ('float4 te=Texture2DSample(TexEmissive,TexEmissiveSampler,uvE);\n'
                 f'float3 e=lerp(te.rgb,dot(te,{v4("Emissive_ChannelSwitch")}).xxx,{s("Emissive_bUseChannelAsEmissive")});\n'
                 f'float3 p=step(1e-6,e)*pow(max(e,1e-6),{f(refine[0])})*{f(refine[1])};\n'
                 f'float3 col=lerp(p,e*{f(refine[2])},{f(refine[3])})*{v4("Emissive_ColorTint")}.rgb*{s("Emissive_Intensity")};\n')
    else:
        code += 'float3 col=0;\n'
    if sw('UseMaskTex', False) and bind('Mask'):
        code += uv('uvM', 'Mask', 'Mask_Rotation', 'Mask_bClamp')
        code += f'float m=dot(Texture2DSample(TexMask,TexMaskSampler,uvM),{v4("Mask_AlphaSwitch")})*{s("Mask_Alpha_Multiply")};\n'
    else:
        code += f'float m={s("Mask_Alpha_Multiply")};\n'
    if sw('UseBase', False) and bind('Base'):
        code += uv('uvB', 'Base', 'Base_Rotation', 'Base_bClamp')
        code += f'float b=dot(Texture2DSample(TexBase,TexBaseSampler,uvB),{v4("Base_AlphaChannel")})*{s("Base_Alpha_Multiply")};\n'
    else:
        # 没绑底图时母材质用白图：通道值为 1
        code += f'float b={s("Base_Alpha_Multiply")};\n'
    if sw('UseDissolve', False) and bind('Dissolve'):
        code += uv('uvD', 'Dissolve', 'DIssolve_Rotation', 'Dissolve_bClamp')
        code += (f'float d=dot(Texture2DSample(TexDissolve,TexDissolveSampler,uvD),{v4("Dissolve_ChannelSwitch")});\n'
                 f'float S={s("Dissolve_Smooth")};\n'
                 f'float erosion=saturate(S*(d-1.0)+{s("Dissolve_Dissolve")}*(S+1.0));\n')
    else:
        code += 'float erosion=1.0;\n'
    # 库洛 DecalComponent.ZFadingFactor/Power 的引擎实现不在导出里：着色器是 saturate(F·(1-|z|))^P 再乘一个未知的组件常量。
    # 这里按 F 为淡出区宽度的倒数尺度归一：pow(saturate(1-|z|), P)（z 为投影方向，-1~1）。这是重建选择。
    code += ('float z=abs(DecalPos.z*2.0-1.0);\n'
             'float fade=pow(saturate(1.0-z),max(ZFadingPower,1e-3));\n'
             'return float4(max(col,0.0),saturate(m*b*erosion*fade));\n')

    def constant(curve_field: dict, default):
        return curve_field.get('Constant') if curve_field else default
    vec = lambda c, d: [c[k] for k in ('X', 'Y', 'Z')] if c else d
    model = {
        'rotation': vec(constant(da.get('Rotation'), None), [0, 0, 0]),
        'scale': vec(constant(da.get('Scale'), None), [1, 1, 1]),
        'location': vec(constant(da.get('Location'), None), [0, 0, 0]),
        'startTime': da.get('StartTime'),
        'floatCurves': {name: rich_curve_keys(c['Curve']['EditorCurveData']) if c.get('bUseCurve') else c.get('Constant', 0.0)
                        for name, c in float_curves.items()},
        'colorCurves': {name: ({ch: rich_curve_keys(c['Curve'].get('ColorCurves' + ('' if i == 0 else f'[{i}]'), {}))
                                for i, ch in enumerate('RGBA')} if c.get('bUseCurve') else [c['Constant'][k] for k in 'RGBA'])
                        for name, c in color_curves.items()},
    }
    for field in ('Rotation', 'Scale', 'Location'):
        if (da.get(field) or {}).get('bUseCurve'):
            unsupported.append(f'model {field} curve')
    name = 'Decal_' + da_package.rsplit('/', 1)[1].removeprefix('DA_Fx_Decal_')
    tuned = load_decal_tuning().get(name, {})
    if 'fadeOut' in tuned:
        # 人工调整：底图透明度从 1 线性降到 0（fadeOut 秒），贴花随之结束；替换原作的 Base_Alpha_Multiply 曲线。
        if 'Base_Alpha_Multiply' not in exposed_scalars:
            raise ValueError(f'{name}: fadeOut needs a Base_Alpha_Multiply curve in the original data asset')
        t = float(tuned['fadeOut'])
        key = lambda time, value: {'time': time, 'value': value, 'interp': 'RCIM_Linear', 'arrive': 0.0, 'leave': 0.0}
        model['floatCurves']['Base_Alpha_Multiply'] = [key(0.0, 1.0), key(t, 0.0)]
        model['startTime'] = t
        model['tuning'] = {k: v for k, v in tuned.items() if k != 'note'}
    return {'name': name, 'source': da_package, 'sourceMaterial': material_package,
            'material': {'id': f"Reference_{CHARACTER['title']}_{name}", 'textures': bindings, 'shader': code,
                         'scalars': exposed_scalars, 'vectors': exposed_vectors},
            'model': model, 'unsupported': unsupported}


DECAL_001_MASTER = 'Client/Content/Aki/Effect/Materials/Mat/M_Decal_001'


def decal_001_definition(da_package: str, da: dict, material_package: str, m: dict, textures: dict) -> dict:
    """M_Decal_001（如长离 E 落地裂纹 MI_Decal_30021）：按它的 shader map（Client_Chunk 4a22b4ac…，0_freq5）与 preshader 重建：
      主 UV = 绕中心旋转(UVRotationAngle 圈)；自发光 UV = 主 UV + 偏移，再绕中心放大 uv + UVscale·(uv-0.5)；
      自发光 = TexEmissive 通道(EmissiveRGBA) → e>0 ? pow(e, EmissiveRefine.x)·EmissiveRefine.y : 0 → × EmissiveColor × EmissiveIntensity；
      溶解 = saturate(Lerp·(d-1) + (1-Diss)·(Lerp+1))，d = TexDissolve.r（溶解 UV = UV·平铺 + 偏移，再旋转）；
      透明 = 溶解 × saturate(TexBase(主 UV).A - |1-Opacity|)。
    重建选择：只输出自发光（UE 贴花的自发光不经 alpha 混合底色）；底色 Color × TexBase 是近黑的受光底色，未重建；
    溶解描边色 Dissolve Edge Color 与第三向量 lerp（MI 里都为 0）未重建。
    槽位名：preshader 里只有默认值；按用途对上（UV 中心放大 = UVscale，emissive 路径的 (1,1,0,0) 向量 = EmissiveRefine，
    底图路径的 = BaseRefine，Opacity 默认 1）。"""
    f = ref.f
    sc, vc, mask = m['scalars'].get, m['vectors'].get, m.get('componentMasks', {})
    float_curves = {e['Key']: e['Value'] for e in da.get('MaterialFloatParameters', [])}
    color_curves = {e['Key']: e['Value'] for e in da.get('MaterialColorParameters', [])}
    known_scalars = {'UVscale', 'UVRotationAngle', 'U_Offset', 'V_Offset', 'EmissiveIntensity', 'Lerp', 'Diss', 'Opacity',
                     'DissolutionTex_U_Tiling', 'DissolutionTex_V_Tiling', 'DissolutionTex_U_Offset', 'DissolutionTex_V_Offset'}
    # 原作 DA 曲线按名字写材质参数；名字对不上（如 'Emissivelntensity'，小写 L）时原作里不起作用，这里同样不暴露
    exposed_scalars = {name: sc(name, 0.0) for name in float_curves if name in known_scalars}
    exposed_vectors = {name: vc(name, [1, 1, 1, 0]) for name in color_curves if name == 'EmissiveColor'}
    ignored = sorted(set(float_curves) - set(exposed_scalars)) + sorted(set(color_curves) - set(exposed_vectors))
    exposed_scalars.update({'ZFadingFactor': float(da.get('ZfadingFactor', 1.0)), 'ZFadingPower': float(da.get('ZfadingPower', 1.0))})
    s = lambda name, d=0.0: name if name in exposed_scalars else f(sc(name, d))
    v4 = lambda name, d: name if name in exposed_vectors else 'float4(' + ','.join(f(x) for x in vc(name, d)) + ')'
    bindings = {}

    def bind(key: str) -> bool:
        package = m['textures'].get(key)
        if not package or package.rsplit('/', 1)[1] in ('T_DefaultColorWhite_D', 'T_DefaultColorBlack_D', 'DefaultTexture'):
            return False
        name = package.rsplit('/', 1)[1]
        if name not in textures:
            textures[name] = texture_entry(name, package)
        bindings[key] = name
        return True

    def channel(field: str, default: str) -> str:
        bits = mask.get(field)
        return 'rgba'[[bits.get(c, False) for c in 'RGBA'].index(True)] if bits and any(bits.get(c) for c in 'RGBA') else default

    turns = sc('UVRotationAngle', 0.0)
    c, sn = math.cos(turns * math.tau), math.sin(turns * math.tau)
    dturns = sc('DissolutionTex_Rotation', 0.0)
    dc, dsn = math.cos(dturns * math.tau), math.sin(dturns * math.tau)
    refine, base_refine = vc('EmissiveRefine', [1, 1, 0, 0]), vc('BaseRefine', [1, 1, 0, 0])
    code = ('// M_Decal_001: operation order from the compiled pixel shader; see prepare_attack.decal_001_definition().\n'
            'float2 uvR=UV-0.5;\n'
            f'uvR=float2(dot(uvR,float2({f(c)},{f(-sn)})),dot(uvR,float2({f(sn)},{f(c)})))+0.5;\n'
            f'float2 uvE=uvR+float2({s("U_Offset")},{s("V_Offset")});\n'
            f'uvE=uvE+{s("UVscale")}*(uvE-0.5);\n')
    if m['switches'].get('UseEmissive', False) and bind('TexEmissive'):
        code += (f'float e=Texture2DSample(TexEmissive,TexEmissiveSampler,uvE).{channel("EmissiveRGBA", "r")};\n'
                 f'float3 col=step(1e-6,e)*pow(max(e,1e-6),{f(refine[0])})*{f(refine[1])}*{v4("EmissiveColor", [1, 1, 1, 0])}.rgb*{s("EmissiveIntensity", 1.0)};\n')
    else:
        code += 'float3 col=0;\n'
    if bind('TexDissolve'):
        code += (f'float2 uvD=UV*float2({s("DissolutionTex_U_Tiling", 1.0)},{s("DissolutionTex_V_Tiling", 1.0)})'
                 f'+float2({s("DissolutionTex_U_Offset")},{s("DissolutionTex_V_Offset")})-0.5;\n'
                 f'uvD=float2(dot(uvD,float2({f(dc)},{f(-dsn)})),dot(uvD,float2({f(dsn)},{f(dc)})))+0.5;\n'
                 'float d=Texture2DSample(TexDissolve,TexDissolveSampler,uvD).r;\n')
    else:
        code += 'float d=0.0;\n'  # 母材质默认黑图
    code += f'float L={s("Lerp", 1.0)};\nfloat dissolve=saturate(L*(d-1.0)+(1.0-{s("Diss")})*(L+1.0));\n'
    if bind('TexBase'):
        code += f'float ba=Texture2DSample(TexBase,TexBaseSampler,uvR).{channel("MaskRGBA", "a")};\n'
    else:
        code += 'float ba=1.0;\n'
    code += (f'float alpha=dissolve*saturate(ba-abs(1.0-{s("Opacity", 1.0)}));\n'
             'float z=abs(DecalPos.z*2.0-1.0);\n'
             'float fade=pow(saturate(1.0-z),max(ZFadingPower,1e-3));\n'
             'return float4(max(col,0.0),saturate(alpha*fade));\n')
    vec = lambda field, d: [field['Constant'][k] for k in ('X', 'Y', 'Z')] if field and 'Constant' in field else d
    model = {
        'rotation': vec(da.get('Rotation'), [0, 0, 0]), 'scale': vec(da.get('Scale'), [1, 1, 1]),
        'location': vec(da.get('Location'), [0, 0, 0]), 'startTime': da.get('StartTime'),
        'floatCurves': {name: float_curve_value(c, 0.0) for name, c in float_curves.items() if name in exposed_scalars},
        'colorCurves': {name: color_curve_value(c, [1, 1, 1, 0]) for name, c in color_curves.items() if name in exposed_vectors},
    }
    unsupported = [f'DA material parameter {n}: ' + ('drives the base colour, which is not reconstructed' if n in ('Color', 'BaseRefine')
                                                     else 'no material parameter of that name (no effect in the original either)') for n in ignored]
    unsupported += [f'model {field} curve' for field in ('Rotation', 'Scale', 'Location') if (da.get(field) or {}).get('bUseCurve')]
    if any(abs(x) > 1e-6 for x in vc('Dissolve Edge Color', [0, 0, 0, 0])[:3]):
        unsupported.append('Dissolve Edge Color')
    # 只解码过 UseEmissive + 单通道遮罩的排列（MI_Decal_30021）；其他静态开关会换 shader map，没核对过
    unsupported += [f'static switch {k} (shader map not decoded; ignored)' for k, on in m['switches'].items() if on and k != 'UseEmissive']
    unsupported += [f'{k} selects {sum(b.values())} channels (decoded permutation has one; first used)'
                    for k, b in mask.items() if sum(b.values()) > 1]
    name = 'Decal_' + da_package.rsplit('/', 1)[1].removeprefix('DA_Fx_Decal_')
    return {'name': name, 'source': da_package, 'sourceMaterial': material_package,
            'material': {'id': f"Reference_{CHARACTER['title']}_{name}", 'textures': bindings, 'shader': code,
                         'scalars': exposed_scalars, 'vectors': exposed_vectors},
            'model': model, 'unsupported': unsupported}


def float_curve_value(field: dict | None, default: float):
    """原作 FloatCurve 字段 → 配方值：有曲线时是关键帧列表，否则是常量。"""
    if not field:
        return default
    if field.get('bUseCurve'):
        return rich_curve_keys(field['Curve']['EditorCurveData'])
    return float(field.get('Constant', default))


def color_curve_value(field: dict | None, default: list[float]):
    if not field:
        return default
    if field.get('bUseCurve'):
        return {ch: rich_curve_keys(field['Curve'].get('ColorCurves' + ('' if i == 0 else f'[{i}]'), {})) for i, ch in enumerate('RGBA')}
    c = field.get('Constant', {})
    return [float(c.get(k, d)) for k, d in zip('RGBA', default)]


# EffectModelPostProcess 里本项目能执行的字段（UE 原生色差 / 暗角 + 重建的径向模糊）；其余字段记入 unsupported。
POST_FLOAT_FIELDS = ('BlurIntensity', 'SceneFringeIntensity', 'VignetteIntensity', 'RadialBlurHardness', 'RadialBlurRadius')


def post_process_definition(da_package: str) -> dict:
    """EffectModelPostProcess（原作 DA_Fx_Post_*）→ 配方：曲线与 Override 照抄。"""
    da = export_objects(da_package)[0]['Properties']
    known = set(POST_FLOAT_FIELDS) | {f + 'Override' for f in POST_FLOAT_FIELDS} | {'StartTime', 'EndTime', 'LoopTime'}
    model = {
        'startTime': da.get('StartTime'), 'endTime': da.get('EndTime', 0.0),
        'floatCurves': {f: float_curve_value(da[f], 0.0) for f in POST_FLOAT_FIELDS if f in da},
        'overrides': {f: bool(da.get(f + 'Override', False)) for f in POST_FLOAT_FIELDS if f + 'Override' in da},
    }
    return {'name': 'Post_' + da_package.rsplit('/', 1)[1].removeprefix('DA_Fx_Post_'), 'source': da_package,
            'model': model, 'unsupported': sorted(k for k in da if k not in known)}


def light_definition(da_package: str) -> dict:
    """EffectModelLight（原作 DA_Fx_Light_*）→ 配方：位置、强度、颜色、半径照抄；库洛角色光字段记入 unsupported。"""
    da = export_objects(da_package)[0]['Properties']
    location = da.get('Location')
    if isinstance(location, dict) and 'X' in location:
        location = {'Constant': location}
    if (location or {}).get('bUseCurve'):
        raise ValueError(f'{da_package}: Location curve is not supported')
    loc = (location or {}).get('Constant') or {}
    model = {
        'startTime': da.get('StartTime'), 'endTime': da.get('EndTime', 0.0),
        'location': [float(loc.get(k, 0.0)) for k in ('X', 'Y', 'Z')],
        'intensity': float_curve_value(da.get('Intensity'), 0.0),
        'color': color_curve_value(da.get('Color'), [1.0, 1.0, 1.0, 1.0]),
    }
    if 'Radius' in da:
        model['radius'] = float_curve_value(da['Radius'], 0.0)
    known = {'StartTime', 'EndTime', 'Location', 'Intensity', 'Color', 'Radius'}
    return {'name': 'Light_' + da_package.rsplit('/', 1)[1].removeprefix('DA_Fx_Light_'), 'source': da_package,
            'model': model, 'unsupported': sorted(k for k in da if k not in known)}


def skeletal_mesh_definition(da_package: str) -> dict:
    """EffectModelSkeletalMesh（原作 DA_Fx_Mesh_*，技能里带动画的道具，例如长离的伞）→ 配方。
    网格 / 动画要先转成 FBX 导入项目（Tools/Blender/uemodel_to_fbx.py + UE ImportAssets，见 fbx / importTo），
    项目资产名按 FBX 导入的规则：网格 <网格名>，动画 <网格名>_Anim（FBX 只有一条动作时）。"""
    da = export_objects(da_package)[0]['Properties']
    mesh_package = da['SkeletalMeshRef']['ObjectPath'].rsplit('.', 1)[0]
    anim_package = da['AnimationRef']['ObjectPath'].rsplit('.', 1)[0]
    mesh_name = mesh_package.rsplit('/', 1)[1]
    folder = f"/Game/Effects/{CHARACTER['title']}/Reference/Props/{mesh_name}"
    constant = lambda field, keys, default: [float(((da.get(field) or {}).get('Constant') or {}).get(k, default)) for k in keys]
    for field in ('Location', 'Rotation', 'Scale'):
        if (da.get(field) or {}).get('bUseCurve'):
            raise ValueError(f'{da_package}: {field} curve is not supported')
    model = {
        'startTime': da.get('StartTime', 1.0), 'loopTime': da.get('LoopTime', 0.0), 'endTime': da.get('EndTime', 0.0),
        'looping': bool(da.get('Looping', False)), 'hideFrames': int(da.get('HideFrames', 0)), 'castShadow': bool(da.get('CastShadow', False)),
        # 欧拉角按 Model 的约定（X=Roll、Y=Pitch、Z=Yaw），和 DA 的 Rotation 常量同一顺序
        'location': constant('Location', 'XYZ', 0.0), 'rotation': constant('Rotation', 'XYZ', 0.0),
        'scale': constant('Scale', 'XYZ', 1.0) if da.get('Scale') else [1.0, 1.0, 1.0],
    }
    notes = []
    if mesh_name == 'EchanglisanMd10011' and not any(model['rotation']):
        # 推断（不是这个 DA 的数据）：伞的网格竖轴是 Z，挂点 WeaponProp02 在第四段里是 X 轴朝上；收伞特效 San_End 在同一挂点上
        # 用的正是 Pitch -90（把网格 Z 转到挂点 X），视频里伞也是竖着举过头顶。原作把这个旋转加在了没导出的地方。
        model['rotation'] = [0.0, -90.0, 0.0]
        notes.append('rotation Pitch -90 inferred from DA_Fx_Group_San_end notify (same socket) and the video; not in DA_Fx_Mesh_San')
    known = {'SkeletalMeshRef', 'AnimationRef', 'StartTime', 'LoopTime', 'EndTime', 'Looping', 'HideFrames', 'CastShadow',
             'Location', 'Rotation', 'Scale', 'NativeClass'}
    return {'name': 'SkeletalMesh_' + da_package.rsplit('/', 1)[1].removeprefix('DA_Fx_Mesh_'), 'source': da_package,
            'sourceMesh': mesh_package, 'sourceAnimation': anim_package,
            'mesh': f'{folder}/{mesh_name}.{mesh_name}', 'animation': f'{folder}/{mesh_name}_Anim.{mesh_name}_Anim',
            'fbx': f'Changli/Props/{mesh_name}.fbx' if CHARACTER['name'] == 'changli' else f'Props/{mesh_name}.fbx',
            'importTo': folder, 'model': model, 'reconstructionNotes': notes, 'unsupported': sorted(k for k in da if k not in known)}


GHOST_MASTER = 'Client/Content/Aki/Render/Shaders/Effect/Ghost/M_GhostNew'
# 原作 M_GhostNew 编译后像素着色器里的 3x3 屏幕抖动表（立即常量缓冲），按 (x%3)*3 + y%3 取值。
GHOST_DITHER = (0.0, 0.777778, 0.333333, 0.666667, 0.555556, 0.222222, 0.444444, 0.111111, 0.888889)


def ghost_definition(da_package: str) -> dict:
    """EffectModelGhost（原作 DA_Fx_Ghost_*）→ 残影材质。按 MI_Ghost_Yangyang 自己的 shader map
    （ShaderArchive-Client_Chunk-PCD3D_SM5，UseDither=true 排列）的像素着色器与 preshader 重建：
      颜色 = Color.rgb + MainTex + RimColor.rgb × RimScale × pow(1 - saturate(N·V), RimWidth)（1-N·V ≤ 0 时边缘光为 0）；
      遮罩 = saturate(2 × (CustomPrimitiveData[0] × Color.a + DitherOffset) - 抖动表)，裁剪值 0.3333。
    preshader 证实的槽位：Color.rgb / RimColor.rgb / Color.a / 顶点偏移常量 0。cooked 数据去掉了参数名，
    默认值 1.0 的标量当作 RimScale、默认值 0 的标量当作 DitherOffset，是按语义选的（重建选择）。
    未重建：MainTex（MI 未设置，母材质默认黑色贴图，贡献为 0）、lerp 到第三个向量（权重是它的 .a，MI 里为 0）。"""
    da = export_objects(da_package)[0]['Properties']
    material_package = da['MaterialRef']['ObjectPath'].rsplit('.', 1)[0]
    f = ref.f
    m = material_chain_values(material_package)
    if m['chain'][-1] != GHOST_MASTER:
        raise ValueError(f'{da_package}: ghost master {m["chain"][-1]} is not M_GhostNew')
    if not m['switches'].get('UseDither', False):
        raise ValueError(f'{da_package}: only the UseDither permutation of M_GhostNew was reconstructed')
    sc, vc = m['scalars'].get, m['vectors'].get
    alpha = da.get('AlphaCurve') or {}
    # 混合模式：MI 链上第一个覆盖 BlendMode 的（叶优先）。MI_Ghost_Changli_140002 覆盖成 Additive，有自己的 shader map
    # （Client_Chunk 1df83a42…，0_freq5）：同样的颜色 / 抖动 alpha，输出 emissive = 颜色 × alpha、o0.w = 0（叠加）。
    for package in m['chain']:
        props = export_objects(package)[0].get('Properties', {})
        overrides = props.get('BasePropertyOverrides') or {}
        if overrides.get('bOverride_BlendMode') or package == m['chain'][-1]:
            mode = overrides.get('BlendMode') if overrides.get('bOverride_BlendMode') else props.get('BlendMode', 'BLEND_Opaque')
            break
    blend = {'BLEND_Masked': 'masked', 'BLEND_Additive': 'additive'}.get(mode)
    if blend is None:
        raise ValueError(f'{da_package}: ghost blend mode {mode} not reconstructed (only Masked / Additive shader maps decoded)')
    code = ('// M_GhostNew (UseDither): operation order from the compiled pixel shader; see prepare_attack.ghost_definition().\n'
            'float f=1.0-saturate(dot(normalize(N),normalize(V)));\n'
            'float3 rim=step(1e-6,f)*pow(max(f,1e-6),RimWidth)*RimScale*RimColor.rgb;\n'
            'float3 col=max(Color.rgb+rim,0.0);\n'
            'int2 px=int2(floor(Parameters.SvPosition.xy-ResolvedView.ViewRectMin.xy));\n'
            'const float dither[9]={' + ','.join(f(x) for x in GHOST_DITHER) + '};\n'
            'float d=dither[(abs(px.x)%3)*3+abs(px.y)%3];\n'
            'float a=saturate(2.0*(Fade*Color.a+DitherOffset)-d);\n'
            + ('return float4(col*a,a);\n' if blend == 'additive' else 'return float4(col,a);\n'))
    name = 'Ghost_' + da_package.rsplit('/', 1)[1].removeprefix('DA_Fx_Ghost_')
    return {'name': name, 'source': da_package, 'sourceMaterial': material_package,
            'material': {'id': f"Reference_{CHARACTER['title']}_{name}", 'shader': code, 'clip': 0.3333, 'blend': blend,
                         'scalars': {'RimWidth': sc('RimWidth', 2.0), 'RimScale': sc('RimScale', 1.0), 'DitherOffset': sc('DitherOffset', 0.0)},
                         'vectors': {'Color': vc('Color', [0, 0, 0, 1]), 'RimColor': vc('RimColor', [1, 1, 1, 1])}},
            'alphaCurve': float_curve_value(alpha, 1.0) if alpha.get('bUseCurve') or 'Constant' in alpha else None}


def ghost_windows(attack: dict, scale: float, first: float) -> tuple[list[dict], dict, list[dict]]:
    """原作蒙太奇里的 AnimNotifyStateGhost（角色残影窗口）：开始时间、时长、生成频率、单个寿命、残影 DA。
    残影 DA 没导出时跳过这个窗口，记在第三个返回值里（需要从 FModel 补导出）。"""
    windows, definitions, missing = [], {}, []
    for source in attack['sources']:
        if source['type'] != 'AnimMontage':
            continue
        objects = json.loads(Path(source['file']).read_text(encoding='utf-8'))
        states = {o['Name']: o for o in objects if o.get('Type') == 'AnimNotifyStateGhost_C'}
        montage = next(o for o in objects if o.get('Type') == 'AnimMontage')
        for n in montage['Properties'].get('Notifies', []):
            ref = (n.get('NotifyStateClass') or {}).get('ObjectName', '')
            state = states.get(ref.split(':', 1)[-1].rstrip("'")) if 'AnimNotifyStateGhost_C' in ref else None
            if not state or not n.get('bEnabled', True):
                continue
            p = state['Properties']
            da = p['EffectDataAssetRef']['AssetPathName'].split('.', 1)[0].replace('/Game/', 'Client/Content/')
            if da not in definitions and not (EXPORTS / (da + '.json')).exists():
                missing.append({'sourceMontage': source['source'], 'child': 'AnimNotifyStateGhost', 'reason': f'missing export {da}'})
                continue
            definition = definitions.get(da) or definitions.setdefault(da, ghost_definition(da))
            windows.append({'ghost': definition['name'], 'delay': (float(n['LinkValue']) - first) * scale,
                            'duration': float(n['Duration']) * scale, 'spawnRate': p.get('SpawnRate', 0.0),
                            'useSpawnRate': p.get('UseSpawnRate', True), 'spawnInterval': p.get('SpawnInterval', 0.0),
                            'ghostLifeTime': p.get('GhostLifeTime', 0.0), 'sourceMontage': source['source']})
    return windows, definitions, missing


# 原作 RenderUtil.GetSelectedChannel：枚举 0 → x=-1（用 RGB 全部），1~4 → R/G/B/A。
CONTROLLER_CHANNELS = {0: [-1, 0, 0, 0], 1: [1, 0, 0, 0], 2: [0, 1, 0, 0], 3: [0, 0, 1, 0], 4: [0, 0, 0, 1]}


def material_controller_definition(da_package: str) -> dict:
    """角色材质控制器（原作 PD_CharacterControllerData）→ 配方。取 DA 里的 KuroMaterialContainerDataCache（原作运行时
    实际读取的有效值）；只转边缘光，其余已开启的功能记入 unsupported。"""
    objects = export_objects(da_package)
    cache = next(o for o in objects if o.get('Type') == 'KuroMaterialContainerDataCache')['Properties']
    data = next(o for o in objects if o.get('Type', '').startswith('PD_CharacterControllerData'))['Properties']
    if data.get('DataType', 0) != 0:
        raise ValueError(f'{da_package}: only effect-type controllers (DataType 0) are supported')
    timing = cache.get('RangeTime', {})
    rim = cache.get('RimParam', {})

    def float_group(group: dict | None) -> dict:
        group = group or {}
        return {phase: float_curve_value(group.get(phase + 'Curve'), 0.0) for phase in ('Start', 'Loop', 'End')}

    def color_group(group: dict | None) -> dict:
        group = group or {}
        return {phase: color_curve_value(group.get(phase + 'Curve'), [0.0, 0.0, 0.0, 0.0]) for phase in ('Start', 'Loop', 'End')}

    model = {
        'startTime': timing.get('Start', 0.0), 'loopTime': timing.get('Loop', 0.0), 'endTime': timing.get('End', 0.0),
        'ignoreTimeDilation': bool(data.get('IgnoreTimeDilation', False)),
        'useRim': bool(cache.get('bUseRim', False)),
        'rimUseTex': bool(rim.get('bRimUseTex', False)),
        'rimChannel': CONTROLLER_CHANNELS.get(int(data.get('RimChannel', 0)) if str(data.get('RimChannel', 0)).isdigit() else 0, [0, 0, 0, 0]),
        'rimRange': float_group(rim.get('RimRange')), 'rimColor': color_group(rim.get('RimColor')),
        'rimIntensity': float_group(rim.get('RimIntensity')),
    }
    unsupported = [k for k in ('bUseOutline', 'bUseDissolve', 'bUseColor', 'bUseTextureSample', 'bUseMotionOffset',
                               'bUseDitherEffect', 'bUseCustomMaterialEffect') if cache.get(k)]
    return {'name': 'MC_' + da_package.rsplit('/', 1)[1].removeprefix('DA_Fx_'), 'source': da_package,
            'model': model, 'unsupported': unsupported}


def material_controller_events(attack: dict) -> list[tuple[float, str]]:
    """原作动画（蒙太奇与其序列）里的 AnimNotifyAddMaterialControllerData：(时间, 控制器 DA)。"""
    events = []
    for source in attack['sources']:
        objects = json.loads(Path(source['file']).read_text(encoding='utf-8'))
        notifies = {o['Name']: o for o in objects if o.get('Type') == 'AnimNotifyAddMaterialControllerData_C'}
        for o in objects:
            for n in (o.get('Properties') or {}).get('Notifies') or []:
                ref = (n.get('Notify') or {}).get('ObjectName', '')
                notify = notifies.get(ref.split(':', 1)[-1].rstrip("'")) if 'AnimNotifyAddMaterialControllerData_C' in ref else None
                if notify and n.get('bEnabled', True):
                    da = notify['Properties']['MaterialAssetData']['ObjectPath'].rsplit('.', 1)[0]
                    events.append((float(n['LinkValue']) + float(n.get('TriggerTimeOffset', 0.0)), da))
    return sorted(set(events))


def child_da_transform(child: dict) -> dict | None:
    """Group 子项 EffectModelNiagara DA 自己的常量 Location / Rotation / Scale（Niagara 组件相对 Group 的变换）。
    原作 EffectModelHelper.VectorToRotator：Pitch = Y、Yaw = Z、Roll = X。用曲线的分量不在这里（旋转曲线走 localYawKeys）。"""
    path = EXPORTS / ((child.get('dataAsset') or '') + '.json')
    if not child.get('dataAsset') or not path.exists():
        return None
    props = export_objects(child['dataAsset'])[0].get('Properties') or {}

    def constant(field: str, default: float) -> list[float]:
        value = props.get(field) or {}
        c = {} if value.get('bUseCurve') else (value.get('Constant') or {})
        return [float(c.get(k, default)) for k in 'XYZ']
    x, y, z = constant('Rotation', 0.0)
    return {'location': constant('Location', 0.0), 'rotation': [y, z, x], 'scale': constant('Scale', 1.0)}


def _rotator_quat(r: list[float]) -> tuple[float, float, float, float]:
    """UE FRotator(Pitch, Yaw, Roll) → FQuat (x, y, z, w)。"""
    sp, cp = math.sin(math.radians(r[0]) / 2), math.cos(math.radians(r[0]) / 2)
    sy, cy = math.sin(math.radians(r[1]) / 2), math.cos(math.radians(r[1]) / 2)
    sr, cr = math.sin(math.radians(r[2]) / 2), math.cos(math.radians(r[2]) / 2)
    return (cr * sp * sy - sr * cp * cy, -cr * sp * cy - sr * cp * sy, cr * cp * sy - sr * sp * cy, cr * cp * cy + sr * sp * sy)


def _quat_rotator(q: tuple[float, float, float, float]) -> list[float]:
    """UE FQuat::Rotator()（不处理万向节奇点外的特殊情况）。"""
    x, y, z, w = q
    test = z * x - w * y
    yaw = math.degrees(math.atan2(2 * (w * z + x * y), 1 - 2 * (y * y + z * z)))
    if test < -0.4999995:
        return [-90.0, yaw, -yaw - 2 * math.degrees(math.atan2(x, w))]
    if test > 0.4999995:
        return [90.0, yaw, yaw - 2 * math.degrees(math.atan2(x, w))]
    pitch = math.degrees(math.asin(2 * test))
    roll = math.degrees(math.atan2(-2 * (w * x + y * z), 1 - 2 * (x * x + y * y)))
    return [pitch, yaw, roll]


def _quat_mul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def _quat_rotate(q, v):
    qv, qw = q[:3], q[3]
    cross = lambda a, b: [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
    t = [2 * c for c in cross(qv, v)]
    u = cross(qv, t)
    return [v[i] + qw * t[i] + u[i] for i in range(3)]


def compose_child_transform(placement: dict, child: dict) -> None:
    """子项变换在 Group（通知）变换之下：世界 = 子项相对 × 通知相对 × 插槽。"""
    q_parent = _rotator_quat(placement['rotation'])
    s_parent = placement['scale']
    offset = _quat_rotate(q_parent, [s_parent[i] * child['location'][i] for i in range(3)])
    placement['location'] = [round(placement['location'][i] + offset[i], 4) for i in range(3)]
    placement['rotation'] = [round(v, 4) for v in _quat_rotator(_quat_mul(q_parent, _rotator_quat(child['rotation'])))]
    placement['scale'] = [round(s_parent[i] * child['scale'][i], 6) for i in range(3)]


def main(stage: str) -> int:
    reference = ref.read(CHARACTER['evidence'] / 'AttackFxReference.json')
    attack = next(a for a in reference['attacks'] if a['projectAttack'] == stage)
    parameters = ref.read(CHARACTER['evidence'] / f'LayerParameters_{stage}.json')
    (HERE / 'Textures').mkdir(exist_ok=True)
    textures, meshes, skipped = {}, set(), []
    systems = build_systems(parameters, textures, meshes, skipped)
    for system in systems:
        for layer in system['layers']:
            rigid_body_vat(layer, textures)
    buildable = {s['name'] for s in systems if s['layers']}
    tuning_scale = {s['name']: s.get('tuningScale', 1.0) for s in systems}
    das = {system_name(s['asset'].rsplit('/', 1)[1]): s['dataAssetExplicitProperties'] for s in parameters['systems']}
    # 同一个 Niagara 被多个 DA 引用（如长离 E 的 12 条 R1s01_Slash，各自 Location / Rotation / Scale 不同），或 DA 带旋转 / 缩放常量：
    # 系统级的 modelLocation 只能存一份、旋转缩放也没地方放，改为把每个子项 DA 的变换并进它自己的放置。
    child_transforms: dict[str, dict] = {}
    system_das: dict[str, set] = {}
    for children in attack['groups'].values():
        for child in children:
            if child.get('niagara') and child.get('dataAsset'):
                system_das.setdefault(system_name(child['niagara'].rsplit('/', 1)[1]), set()).add(child['dataAsset'])
                if child['dataAsset'] not in child_transforms:
                    child_transforms[child['dataAsset']] = child_da_transform(child)
    per_child = {name for name, found in system_das.items()
                 if len(found) > 1 or any((t := child_transforms.get(d)) and (any(abs(v) > 1e-4 for v in t['rotation'])
                                                                          or any(abs(v - 1) > 1e-4 for v in t['scale'])) for d in found)}
    scale = attack['timelineScale']

    events = [dict(e, state=False) for e in attack['sourceEvents'] if e['included']]
    events += [dict(e, authoredSeconds=e['authoredBeginSeconds'], state=True) for e in attack['sourceSequenceEffects'] if e['included']]
    first = min((e['authoredSeconds'] for e in events), default=0.0)  # 镜头序列阶段没有通知
    placements, unplaced, skipped_children = [], [], []
    decals: dict[str, dict] = {}
    # 非 Niagara 子项：类型 → (定义函数, 已生成的定义)
    others = {'EffectModelPostProcess_C': (post_process_definition, {}), 'EffectModelLight_C': (light_definition, {}),
              'EffectModelSkeletalMesh_C': (skeletal_mesh_definition, {})}
    for event in sorted(events, key=lambda e: e['authoredSeconds']):
        group = event['effectAsset'].rsplit('/', 1)[1].split('.', 1)[0]
        children = attack['groups'].get(group, [])
        if not children:
            # 通知直接引用单个 DA（贴花 / 后处理 / 灯光等，不是 Group）：按它的 NativeClass 当作只有一个子项
            package = event['effectAsset'].split('.', 1)[0].replace('/Game/', 'Client/Content/')
            native = ''
            if (EXPORTS / (package + '.json')).exists():
                native = ((export_objects(package)[0].get('Properties') or {}).get('NativeClass') or {}).get('ObjectName', '')
            found = re.search(r"'(EffectModel\w+_C)'", native)
            if found and found.group(1) != 'EffectModelGroup_C':
                children = [{'modelClass': found.group(1), 'delay': 0.0, 'dataAsset': package}]
            elif group.startswith('DA_Fx_Decal_'):
                children = [{'modelClass': 'EffectModelDecal_C', 'delay': 0.0, 'dataAsset': package}]
        placed = False
        for child in children:
            if child.get('modelClass') in ('EffectModelDecal_C', *others) and child.get('dataAsset'):
                if not (EXPORTS / (child['dataAsset'] + '.json')).exists():
                    skipped_children.append({'sourceEffect': group, 'child': child['dataAsset'], 'modelClass': child['modelClass'],
                                             'reason': f"missing export {child['dataAsset']}"})
                    continue
                if child['modelClass'] == 'EffectModelDecal_C':
                    definition = decals.get(child['dataAsset'])
                    if definition is None:
                        definition = decals[child['dataAsset']] = decal_definition(child['dataAsset'], textures)
                    key = 'decal'
                else:
                    build, built = others[child['modelClass']]
                    definition = built.get(child['dataAsset'])
                    if definition is None:
                        definition = build(child['dataAsset'])
                        # 骨骼网格道具要先用 Tools/Blender/uemodel_to_fbx.py 转成 FBX 并导入（commandlet 只加载，不导入）：还没转的先跳过
                        if definition.get('fbx') and not (HERE / definition['fbx']).exists():
                            skipped_children.append({'sourceEffect': group, 'child': child['dataAsset'], 'modelClass': child['modelClass'],
                                                     'reason': f"prop mesh not converted yet ({definition['fbx']})"})
                            continue
                        built[child['dataAsset']] = definition
                    key = 'model'
                    # 原作循环播放（LoopTime > 0）的子项挂在通知窗口（AnimNotifyStateEffect）上，窗口结束才停：
                    # 项目的通知只在开始时触发一次，把窗口时长写成它的播放时长（StartTime），循环段清零。
                    m = definition.get('model', {})
                    if event['state'] and (m.get('loopTime') or 0) > 0 and event.get('durationSeconds'):
                        m.update({'startTime': float(event['durationSeconds']) * scale, 'loopTime': 0.0,
                                  'lifetimeSource': f"notify window {event['notifyObject']} ({event['durationSeconds']:.3f}s)"})
                placements.append({
                    key: definition['name'],
                    'location': [event['location'][k] for k in ('X', 'Y', 'Z')] if event['location'] else [0, 0, 0],
                    'rotation': [event['rotation'][k] for k in ('Pitch', 'Yaw', 'Roll')] if event['rotation'] else [0, 0, 0],
                    'scale': [event['scale'][k] for k in ('X', 'Y', 'Z')] if event['scale'] else [1, 1, 1],
                    'delay': (event['authoredSeconds'] - first) * scale + child['delay'],
                    'attached': event.get('attachedExplicit') is not False,
                    'socket': event.get('socketName') or 'Root',
                    'sourceNotify': event['notifyObject'], 'sourceOwner': event['owner'],
                    'sourceSeconds': event['authoredSeconds'], 'sourceEffect': group,
                })
                placed = True
                continue
            if not child.get('niagara'):
                skipped_children.append({'sourceEffect': group, 'child': child.get('dataAsset') or child.get('missing'),
                                         'modelClass': child.get('modelClass'), 'reason': 'no executor for this model type yet'})
                continue
            name = system_name(child['niagara'].rsplit('/', 1)[1])
            if name not in buildable:
                skipped_children.append({'sourceEffect': group, 'child': name, 'reason': 'no buildable layer'})
                continue
            placement = {
                'system': name,
                'location': [event['location'][k] for k in ('X', 'Y', 'Z')] if event['location'] else [0, 0, 0],
                'rotation': [event['rotation'][k] for k in ('Pitch', 'Yaw', 'Roll')] if event['rotation'] else [0, 0, 0],
                'scale': [v * tuning_scale.get(name, 1.0) for v in ([event['scale'][k] for k in ('X', 'Y', 'Z')] if event['scale'] else [1, 1, 1])],
                'delay': (event['authoredSeconds'] - first) * scale + child['delay'],
                # 原作 AnimNotifyEffect.Attached 默认 true；显式 false 的不跟随角色。
                'attached': event.get('attachedExplicit') is not False,
                'socket': event.get('socketName') or 'Root',
                'sourceNotify': event['notifyObject'], 'sourceOwner': event['owner'],
                'sourceSeconds': event['authoredSeconds'], 'sourceEffect': group,
            }
            if event['state']:
                placement['sourceStateDurationSeconds'] = event['durationSeconds']
            if name in per_child and child_transforms.get(child.get('dataAsset')):
                compose_child_transform(placement, child_transforms[child['dataAsset']])
                placement['childTransformSource'] = child['dataAsset'].rsplit('/', 1)[1] + ' Location/Rotation/Scale constants (Pitch=Y, Yaw=Z, Roll=X)'
            rotation = das.get(name, {}).get('Rotation')
            if rotation and rotation.get('bUseCurve'):
                placement['localYawKeys'] = changli.yaw_keys(rotation, scale)
                placement['localYawSource'] = 'Original DA Rotation.Curve[2], timeline-scaled; axis mapping to local yaw is reconstruction.'
            placements.append(placement)
            placed = True
        if not placed:
            unplaced.append({'sourceNotify': event['notifyObject'], 'sourceEffect': group, 'reason': 'no buildable child effect'})

    for system in systems:
        if system['name'] in per_child:
            system.pop('modelLocation', None)  # 已并进每个放置
    ghost_placements, ghosts, missing_ghosts = ghost_windows(attack, scale, first)
    skipped_children += missing_ghosts
    controllers: dict[str, dict] = {}
    for seconds, da in material_controller_events(attack):
        if da not in controllers and not (EXPORTS / (da + '.json')).exists():
            skipped_children.append({'sourceEffect': da, 'child': 'AnimNotifyAddMaterialControllerData', 'reason': f'missing export {da}'})
            continue
        definition = controllers.get(da) or controllers.setdefault(da, material_controller_definition(da))
        # 原作通知挂在角色骨骼网格上，作用于整个角色；这里用 PlayEffect 通知（跟随网格）播放控制器 Model。
        placements.append({'model': definition['name'], 'location': [0, 0, 0], 'rotation': [0, 0, 0], 'scale': [1, 1, 1],
                           'delay': (seconds - first) * scale, 'attached': True, 'socket': 'Root',
                           'sourceNotify': 'AnimNotifyAddMaterialControllerData', 'sourceSeconds': seconds, 'sourceEffect': da})
    recipe = {
        'schemaVersion': 1, 'character': CHARACTER['name'], 'stage': stage, 'montage': attack['projectMontage'],
        # 镜头序列阶段：只建系统，不往蒙太奇装通知（commandlet 的 solo 路径）
        **({'solo': True, 'sequenceSystems': attack['sequenceSystems']} if attack.get('sequenceSystems') and not events else {}),
        # 子弹特效阶段：同样只建 Model 组不放通知，放置由 ArtSource/Bullet/prepare_bullets.py 写进子弹表
        **({'solo': True, 'bulletEffects': attack['bulletEffects']} if attack.get('bulletEffects') else {}),
        'description': f"{CHARACTER['cn']} {stage}："f'网格 / 贴图为导出原件，材质与 Niagara 为重建（ChangliSlash 同一套重建）。由 ArtSource/JiabeiAttack/prepare_attack.py 生成。',
        'notifyAuthoredSeconds': first * scale,
        'textures': sorted(textures.values(), key=lambda t: t['name']), 'meshes': sorted(meshes),
        'systems': systems, 'decals': list(decals.values()),
        'postProcesses': list(others['EffectModelPostProcess_C'][1].values()), 'lights': list(others['EffectModelLight_C'][1].values()),
        'skeletalMeshes': list(others['EffectModelSkeletalMesh_C'][1].values()),
        'ghosts': list(ghosts.values()), 'ghostWindows': ghost_placements,
        'materialControllers': list(controllers.values()),
        'placements': placements,
        'skippedLayers': skipped, 'skippedChildren': skipped_children, 'unplacedEvents': unplaced,
        'reconstructionChoices': [
            'Shader / mesh WPO / sprite emission reconstruction is ChangliSlash/prepare_attack.py, imported unchanged.',
            'Lifetime / intensity per mesh layer by name analogy with Changli; not in the cooked export. Tune with <Stage>Tuning.json.',
            'Render/Common default textures missing from the export use generated solid placeholders (substitute=true).',
            'timelineScale = 1: the project montages play the original 30 fps sequences at their original length.',
        ],
    }
    (CHARACTER['out'] / f'{stage}.json').write_text(json.dumps(recipe, ensure_ascii=False, indent=2), encoding='utf-8')
    layers = sum(len(s['layers']) for s in systems)
    print(json.dumps({'recipe': f'{stage}.json', 'systems': len(systems), 'layers': layers, 'textures': len(textures),
                      'placeholders': [t['name'] for t in textures.values() if t.get('substitute')], 'meshes': len(meshes),
                      'placements': len(placements), 'skippedLayers': len(skipped),
                      'skippedChildren': sorted({c.get('modelClass') or c['reason'] for c in skipped_children}),
                      'unplaced': len(unplaced)}, ensure_ascii=False))
    return 0


if __name__ == '__main__':
    sys.exit(main(ARGS[0]))
