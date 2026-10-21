"""角色仿鸣潮材质：从原作导出整理出配方（目前是描边部分）。
  python -I prepare_toon.py jiabei
输出 <角色>.toon.json（不提交，原作数据），由 WuwaCharacterToonSetup 命令行读取。

描边按原作 M_CharacterOutline 编译后的着色器（MI_R2T1CalbrenaMd10011Cloth_OL / Face_OL 的 shader map）：
  宽度 = lerp(VertexGMinWidthFactor, 1, 顶点色 G) × OutlineClipWidth，按裁剪空间算并做宽高比 / FOV 补偿，
  不小于约 1 像素（OutlineHoldDistance 控制过渡）；顶点色 B × InnerOutlineMulti × InnerOutlineOffset × 0.01 往远离相机方向推；
  方向 = UV1 里八面体编码的切线空间平滑法线。
  颜色：UseMainTex 时 = 漫反射 × OutlineColorTint，否则 = OutlineColor。
参数值按原作材质实例链取：部位 _OL 实例 → 公共父实例（MI_CharacterOL_Default / MI_HairOL_Default）→ 母材质默认值。
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
EXPORTS = Path(r'C:\GamePakExtractor\Output\Exports')

CHARACTERS = {
    'jiabei': {
        'name': 'jiabei',
        'model': 'Client/Content/Aki/Character/Role/FemaleXL/Jiabeilina/R2T1CalbrenaMd10011/Model',
        'prefix': 'MI_R2T1CalbrenaMd10011',
        # 原作网格的材质槽（R2T1CalbrenaMd10011.uemodel 的顺序），项目网格的槽名相同
        'slots': ['Bangs', 'Hair', 'Face', 'Up', 'Down', 'Cloth', 'Eye'],
        'idUpperUp': True,  # 原作上衣 ID 贴图名是大写 UP_ID
    },
    'changli': {
        'name': 'changli',
        'model': 'Client/Content/Aki/Character/Role/FemaleXL/ChangLi/R2T1ChangLiMd10011/Model',
        'prefix': 'MI_R2T1ChangLiMd10011',
        'slots': ['Bangs', 'Hair', 'Face', 'Up', 'Down', 'Cloth', 'Eye'],
        'idUpperUp': False,
    },
    # 芙露德莉丝（Boss MB1FuludelisiMd00401 用的就是可玩角色的模型）：比上面两位多一个 Alpha 槽。
    # 项目里没有她的漫反射，配方直接带原作 PNG（diffuseFromExports），由命令行导入到 Mesh/Toon/Textures。
    'fuludelisi': {
        'name': 'fuludelisi',
        'model': 'Client/Content/Aki/Character/Role/FemaleZ/Fuludelisi/R2T1FuludelisiMd10011/Model',
        'prefix': 'MI_R2T1FuludelisiMd10011',
        'slots': ['Bangs', 'Hair', 'Face', 'Up', 'Down', 'Cloth', 'Alpha', 'Eye'],
        'idUpperUp': False,
        'diffuseFromExports': True,
    },
}
OUTLINE_SCALARS = ('OutlineClipWidth', 'VertexGMinWidthFactor', 'OutlineHoldDistance', 'InnerOutlineMulti', 'InnerOutlineOffset')
OUTLINE_VECTORS = ('OutlineColorTint', 'OutlineColor')
OUTLINE_SWITCHES = ('UseMainTex', 'UseVertexColorB_InnerOutline', 'UseVertexGreen_OutlineWidth')


def load(package: str):
    return json.loads((EXPORTS / (package + '.json')).read_text(encoding='utf-8'))


def package_references(package: str) -> list[str]:
    """.uasset 名字表里的 /Game 路径（FModel 的材质 JSON 不带父材质和贴图参数）。"""
    data = (EXPORTS / (package + '.uasset')).read_bytes()
    return [m.decode() for m in re.findall(rb'/Game/[A-Za-z0-9_/]+', data)]


def referenced_textures(package: str) -> list[str]:
    """材质实例引用的贴图名。新导出（Parameters 格式）有 .uasset，从名字表里找；旧导出（Properties 格式）直接读贴图参数。"""
    if (EXPORTS / (package + '.uasset')).exists():
        return [p.rsplit('/', 1)[1] for p in package_references(package) if p.rsplit('/', 1)[1].startswith('T_')]
    obj = next(o for o in load(package) if o['Type'] == 'MaterialInstanceConstant')
    names = []
    for e in obj['Properties'].get('TextureParameterValues', []):
        value = e.get('ParameterValue') or {}
        name = value.get('ObjectPath', '').rsplit('/', 1)[-1].split('.', 1)[0]
        if name.startswith('T_'):
            names.append(name)
    return names


def to_package(game_path: str) -> str:
    return game_path.replace('/Game/', 'Client/Content/', 1)


def chain_values(package: str) -> dict:
    """部位实例（FModel Parameters 格式）→ 父实例（Properties 格式）→ 母材质（CachedExpressionData），子级优先。"""
    values = {'scalars': {}, 'vectors': {}, 'switches': {}, 'chain': [package]}
    rgba = lambda c: [float(c[k]) for k in 'RGBA']
    d = load(package)
    if isinstance(d, dict):
        # 新导出：部位实例是 Parameters 格式，父材质从 .uasset 名字表找
        params = d['Parameters']
        values['scalars'].update({k: float(v) for k, v in params.get('Scalars', {}).items()})
        values['vectors'].update({k: rgba(v) for k, v in params.get('Colors', {}).items()})
        values['switches'].update({k: bool(v) for k, v in params.get('Switches', {}).items()})
        parent = next(p for p in package_references(package) if '/MaterialInstance/' in p or '/Shaders/' in p)
        package = to_package(parent)
    else:
        # 旧导出：部位实例本身就是 Properties 格式，和父实例一样处理
        values['chain'].pop()
    while package:
        values['chain'].append(package)
        obj = next(o for o in load(package) if o['Type'] in ('MaterialInstanceConstant', 'Material'))
        p = obj['Properties']
        if obj['Type'] == 'Material':
            P = p['CachedExpressionData']['Parameters']
            for info, v in zip(P['RuntimeEntries']['ParameterInfos'], P['ScalarValues']):
                values['scalars'].setdefault(info['Name'], float(v))
            for info, v in zip(P['RuntimeEntries[1]']['ParameterInfos'], P['VectorValues']):
                values['vectors'].setdefault(info['Name'], rgba(v))
            break
        for e in p.get('ScalarParameterValues', []):
            values['scalars'].setdefault(e['ParameterInfo']['Name'], float(e['ParameterValue']))
        for e in p.get('VectorParameterValues', []):
            values['vectors'].setdefault(e['ParameterInfo']['Name'], rgba(e['ParameterValue']))
        for e in (p.get('StaticParameters') or {}).get('StaticSwitchParameters', []):
            values['switches'].setdefault(e['ParameterInfo']['Name'], bool(e['Value']))
        parent = p.get('Parent')
        package = parent['ObjectPath'].rsplit('.', 1)[0] if parent else None
    return values


def outline(character: dict, slot: str) -> dict | None:
    package = f"{character['model']}/{character['prefix']}{slot}_OL"
    if not (EXPORTS / (package + '.json')).exists():
        return None
    v = chain_values(package)
    textures = referenced_textures(package)
    switches = {k: v['switches'].get(k, False) for k in OUTLINE_SWITCHES}
    if not switches['UseVertexGreen_OutlineWidth']:
        raise ValueError(f'{package}: UseVertexGreen_OutlineWidth off — only the compiled permutation with it on was reconstructed')
    scalars = {k: v['scalars'][k] for k in OUTLINE_SCALARS}
    if not switches['UseVertexColorB_InnerOutline']:
        # 推断（这个排列的着色器没解码）：开关关掉就是去掉"按顶点色 B 往里推"那一项，等价于 InnerOutlineMulti = 0。
        scalars['InnerOutlineMulti'] = 0.0
    diffuse = textures[0] if switches['UseMainTex'] and textures else None
    substitute = None
    if diffuse and diffuse.endswith('_LD') and not texture(diffuse, TEXTURE_SOURCES[character['name']]['mod'], True):
        # 描边专用贴图 _LD 没有像素（流式资源）。白芷的 _LD 和 _D 都在：_LD = 半分辨率的 _D × 0.6（sRGB 值，上下装
        # 10%~90% 分位 0.57~0.61），所以用 _D 代替，再乘 0.6 的线性等价值 0.6^2.2。这是推断，不是这个角色的原数据。
        substitute = diffuse
        diffuse = diffuse[:-3] + '_D'
        scalars['MainTexScale'] = round(0.6 ** 2.2, 4)
    result = {
        'source': package, 'chain': v['chain'],
        'useMainTex': switches['UseMainTex'],
        'diffuse': diffuse,
        'mainTexSubstitute': substitute,
        'innerOutlineSwitch': switches['UseVertexColorB_InnerOutline'],
        'scalars': scalars,
        'vectors': {k: v['vectors'][k] for k in OUTLINE_VECTORS},
    }
    # 项目里还没有这张图时，命令行按这个导入（diffuseFromExports 的角色；其他角色的配方不变）
    if diffuse and character.get('diffuseFromExports'):
        result['diffuseTexture'] = texture(diffuse, TEXTURE_SOURCES[character['name']]['mod'], True)
    return result


# 身体卡通材质用到的原作参数（明暗公式是重建的，见 reconstructionChoices）。
BODY_SCALARS = ('ShadowProcess', 'ShadowWidth', 'SolidShadowSaturation', 'NormalStrength', 'SDFSmoothness', 'RampPosition',
                'StockingIntensity', 'StockingRangeMin', 'StockingRangeMax', 'StockingLightRangeMin', 'StockingLightRangeMax')
BODY_VECTORS = ('SkinShadowColor1', 'BaseColorTint', 'StockingColor', 'StockingEdgeColor', 'StockingLightColor')
GAME_ROOT = Path(r'C:\GamePakExtractor')
# 原作贴图的像素：新版本包里角色贴图改为流式资源，导出里只有 JSON。
# 嘉贝莉娜的 PNG 来自公开的官方模型包；长离的贴图在旧版本包里，FModel 导出的 PNG 就在模型目录（通道已核对，和嘉贝莉娜一致）。
TEXTURE_SOURCES = {
    'jiabei': {
        'mod': '嘉贝莉娜/嘉贝莉娜（修模绑骨后）/AONMPB',
        'tex': '嘉贝莉娜/嘉贝莉娜（修模绑骨后）/tex',
        'common': 'Output/Exports/Client/Content/Aki/Character/Role/CommonTexture',
    },
    'changli': {
        'mod': 'Output/Exports/Client/Content/Aki/Character/Role/FemaleXL/ChangLi/R2T1ChangLiMd10011/Model',
        'tex': 'Output/Exports/Client/Content/Aki/Character/Role/FemaleXL/ChangLi/R2T1ChangLiMd10011/Model',
        'common': 'Output/Exports/Client/Content/Aki/Character/Role/CommonTexture',
    },
    # 芙露德莉丝的贴图在当前包里有像素（PakExport 导出 2048 PNG）
    'fuludelisi': {
        'mod': 'Output/Exports/Client/Content/Aki/Character/Role/FemaleZ/Fuludelisi/R2T1FuludelisiMd10011/Model',
        'tex': 'Output/Exports/Client/Content/Aki/Character/Role/FemaleZ/Fuludelisi/R2T1FuludelisiMd10011/Model',
        'common': 'Output/Exports/Client/Content/Aki/Character/Role/CommonTexture',
    },
}


def texture(name: str, folder: str, srgb: bool) -> dict | None:
    for ext in ('.png', '.PNG'):
        if (GAME_ROOT / folder / (name + ext)).exists():
            return {'name': name, 'file': f'{folder}/{name}{ext}', 'srgb': srgb}
    return None


def body_textures(name: str, slot: str, prefix_tex: str, switches: dict) -> dict:
    src = TEXTURE_SOURCES[name]
    part = 'UP' if slot == 'Up' and CHARACTERS[name].get('idUpperUp') else slot
    t = {
        'normal': texture(f'{prefix_tex}{slot}_N', src['mod'], False),
        'id': texture(f'{prefix_tex}{part}_ID', src['mod'], False),
        'highlight': texture(f'{prefix_tex}{slot}_HM', src['mod'], False),
        'ramp': texture(f'{prefix_tex}_Skin', src['tex'], True),
        'sdf': texture('T_FemaleMFace_SDF', src['common'], False) if switches.get('UseSDFShadow') else None,
        'diffuse': texture(f'{prefix_tex}{slot}_D', src['mod'], True) if CHARACTERS[name].get('diffuseFromExports') else None,
    }
    return {k: v for k, v in t.items() if v}


def body(character: dict, slot: str) -> dict | None:
    package = f"{character['model']}/{character['prefix']}{slot}"
    if not (EXPORTS / (package + '.json')).exists():
        return None
    v = chain_values(package)
    tex_prefix = character['prefix'].replace('MI_', 'T_')
    return {'source': package, 'chain': v['chain'],
            'scalars': {k: v['scalars'][k] for k in BODY_SCALARS if k in v['scalars']},
            'vectors': {k: v['vectors'][k] for k in BODY_VECTORS if k in v['vectors']},
            'switches': v['switches'],
            'textures': body_textures(character['name'], slot, tex_prefix, v['switches'])}


# 特效道具（长离第四段的伞 DA_Fx_Mesh_San）。原作材质 JSON 是 FModel 的材质摘要格式（Textures / Colors / Scalars / Switches），
# 不带父材质；贴图在新版本包里是流式资源，PNG 来自用户提供的配件包（通道和导出的 MI 引用名一致）。
PROPS = {
    'changli_san': {
        'model': 'Client/Content/Aki/Character/Weapon/Prop/Role/ChangLiProp/EchanglisanMd10011/Model',
        'material': 'MI_EchanglisanMd10011',
        'tex': '长离配件1_by_asasa_06ab3f3782c1377c8880b07c4731ad6d/伞',
        # 推断：摘要格式不带父材质，描边按角色描边的公共父实例取缺的参数（OutlineClipWidth 等）
        'outlineParent': 'Client/Content/Aki/Character/MaterialInstance/Outline/MI_CharacterOL_Default',
    },
    # 芙露德莉丝的大剑（Boss MB1FuludelisiMd00401 挂在 WeaponProp02）。贴图在当前包里有像素（PakExport 导出）。
    # 网格三个槽：本体（卡通 + 描边）、_Jian 剑刃（原作是 MatCap + 流水贴图的特殊材质）、_Shui 水纹（遮罩流水特效）。
    'fuludelisi_dajian': {
        'model': 'Client/Content/Aki/Character/Weapon/Prop/Role/FuludelisiProp/EFuludelisiDajianMd10011/Model',
        'material': 'MI_EFuludelisiDajianMd10011',
        'tex': 'Output/Exports/Client/Content/Aki/Character/Weapon/Prop/Role/FuludelisiProp/EFuludelisiDajianMd10011/Model',
        'outlineParent': 'Client/Content/Aki/Character/MaterialInstance/Outline/MI_CharacterOL_Default',
        # 推断：其余槽的原作母材质没解码，先用角色卡通材质显示一张本色贴图（槽 → 原作材质里的贴图参数）
        'extraSlots': {'MI_EFuludelisiDajianMd10011_Jian': 'Main_Tex', 'MI_EFuludelisiDajianMd10011_Shui': None},
        'extraTextures': {'MI_EFuludelisiDajianMd10011_Shui': 'T_EFuludelisiDajianShuiMd10011_D'},
    },
}


def summary_parts(mi: dict) -> tuple[dict, dict]:
    """FModel 材质摘要：旧格式顶层就是 Textures / Scalars / Colors / Switches，贴图值是 {ObjectPath}；
    新格式参数包在 Parameters 里，贴图值直接是路径字符串。返回 (贴图名表, 参数表)。"""
    params = mi.get('Parameters', mi)
    names = {k: (v['ObjectPath'] if isinstance(v, dict) else v).rsplit('/', 1)[1].split('.')[0] for k, v in mi.get('Textures', {}).items()}
    return names, params


def prop_recipe(name: str) -> dict:
    prop = PROPS[name]
    rgba = lambda c: [float(c[k]) for k in 'RGBA']
    body_pkg = f"{prop['model']}/{prop['material']}"
    outline_pkg = body_pkg + '_OL'
    mi, ol = load(body_pkg), load(outline_pkg)
    parent = chain_values(prop['outlineParent'])
    mi_textures, mi_params = summary_parts(mi)
    ol_textures, ol_params = summary_parts(ol)
    scalars = {k: v for k, v in parent['scalars'].items() if k in OUTLINE_SCALARS}
    scalars.update({k: float(v) for k, v in ol_params.get('Scalars', {}).items() if k in OUTLINE_SCALARS})
    vectors = {k: v for k, v in parent['vectors'].items() if k in OUTLINE_VECTORS}
    vectors.update({k: rgba(v) for k, v in ol_params.get('Colors', {}).items() if k in OUTLINE_VECTORS})
    switches = {k: parent['switches'].get(k, False) for k in OUTLINE_SWITCHES}
    switches.update({k: bool(v) for k, v in ol_params.get('Switches', {}).items() if k in OUTLINE_SWITCHES})
    if not switches['UseVertexColorB_InnerOutline']:
        scalars['InnerOutlineMulti'] = 0.0
    diffuse = mi_textures['MainTex']
    normal = mi_textures['Normal_Roughness_Metallic']
    # 描边实例带贴图且开了 UseMainTex 时颜色 = 漫反射 × OutlineColorTint（大剑）；伞的描边实例没有贴图，用 OutlineColor
    outline_tex = ol_textures.get('MainTex') if switches['UseMainTex'] else None
    slot = {
        'outline': {
            'source': outline_pkg, 'chain': [outline_pkg, prop['outlineParent'] + ' (inferred parent)'],
            'useMainTex': bool(outline_tex), 'diffuse': outline_tex, 'mainTexSubstitute': None,
            'innerOutlineSwitch': switches['UseVertexColorB_InnerOutline'],
            'scalars': scalars, 'vectors': vectors,
        },
        'body': {
            'source': body_pkg, 'chain': [body_pkg],
            # 原作是道具的 PBR 卡通母材质（UsePBR / Normal_Roughness_Metallic，未解码），这里用角色卡通材质显示本色
            'scalars': {}, 'vectors': {}, 'switches': {k: bool(v) for k, v in mi_params.get('Switches', {}).items()},
            # 法线贴图 RG 只有 126~128（基本是平的，B/A 是粗糙度 / 金属度），不接
            'textures': {'diffuse': texture(diffuse, prop['tex'], True)},
            'skippedTextures': [normal],
        },
    }
    if outline_tex:
        slot['outline']['diffuseTexture'] = texture(outline_tex, prop['tex'], True)
    slots = {prop['material']: slot}
    for extra, param in prop.get('extraSlots', {}).items():
        package = f"{prop['model']}/{extra}"
        tex_name = summary_parts(load(package))[0].get(param) if param else prop['extraTextures'][extra]
        slots[extra] = {'outline': None, 'body': {
            'source': package, 'chain': [package], 'scalars': {}, 'vectors': {}, 'switches': {},
            'textures': {'diffuse': texture(tex_name, prop['tex'], True)},
            'inferred': 'original master not decoded; shows one diffuse texture with the character toon body'}}
    # 原作网格没有顶点色（FModel 导出和配件包的 psk / fbx 都没有颜色层）；没有颜色的蒙皮网格在 UE 里读到白色，
    # 即描边宽度 G = 1、内推 B = 1，和原作运行时一致（推断）。命令行据此放行顶点色检查。
    return {'schemaVersion': 1, 'character': name, 'slots': slots, 'meshHasNoVertexColors': True,
            'reconstructionChoices': [
                'Prop material: the original master (UsePBR, Normal_Roughness_Metallic) is not decoded; the character toon body shows the diffuse.',
                'Outline parent: the summary-format export has no parent; MI_CharacterOL_Default fills the missing outline scalars (inference).',
            ]}


def main(name: str) -> int:
    if name in PROPS:
        recipe = prop_recipe(name)
        (HERE / f'{name}.toon.json').write_text(json.dumps(recipe, ensure_ascii=False, indent=1), encoding='utf-8')
        print(json.dumps(recipe['slots'], ensure_ascii=False)[:1500])
        return 0
    character = CHARACTERS[name]
    slots = {}
    for slot in character['slots']:
        slots[f"{character['prefix']}{slot}"] = {'outline': outline(character, slot), 'body': body(character, slot)}
    recipe = {'schemaVersion': 1, 'character': name, 'slots': slots,
              'reconstructionChoices': [
                  'Outline object-scale term: the original multiplies the width by the primitive (component) scale. The project '
                  'material uses the component scale the same way (MeshScale 1); 100 (the 2.0 root-bone scale) was tried and inflated the mesh.',
                  'Outline view constant cb1[280].y (Kuro view uniform, meaning unknown) is taken as 1 (material parameter PixelScale).',
                  'Outline with UseVertexColorB_InnerOutline off (e.g. Changli Down_OL): that permutation was not decoded; '
                  'the switch is taken to remove the vertex-colour-B inner push, i.e. InnerOutlineMulti = 0 (inference).',
                  'Outline lighting: the original writes the colour into the GBuffer for Kuro\'s deferred toon pass; here it is unlit emissive.',
                  'Body light/shadow: the original lighting runs in Kuro\'s deferred toon pass (not decoded). The project material uses '
                  'half-Lambert of the main light, smoothstep(ShadowProcess -/+ ShadowWidth/2), shadow = base x ShadowTint with '
                  'SolidShadowSaturation; ShadowTint is a project default, not original data. Normal / ID maps wait for original texture exports.',
              ]}
    out = HERE / f'{name}.toon.json'
    out.write_text(json.dumps(recipe, ensure_ascii=False, indent=1), encoding='utf-8')
    print(json.dumps({k: (s['outline'] or {}).get('scalars') for k, s in slots.items()}, ensure_ascii=False))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else 'jiabei'))
