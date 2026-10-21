"""区域内原作静态物件（岩石、植被、建筑……）→ 导出网格/贴图 + WuwaPropImport 的配方。
  python -I prepare_props.py --region crownless [--radius 500] [--only SM_Com_Roc_58AX,SM_Com_Tre_33AS] [--limit N] [--skip-export]

输入：PakExport placements 对区域相关单元的输出（<区域>_placements_raw.json）。只用标准格式的摆放：
  - component：StaticMeshActor 的组件（世界变换 = 组件相对变换沿 AttachParent 组合）；
  - fastgeo：FastGeo 容器里的静态网格组件（自带世界变换）。
  植被实例（instanced / fastgeo-instanced）的实例数据是库洛改过的格式（16 个 float 看起来是包围盒 + 缩放，没有旋转），
  含义无法确认，不用（草以后用 Landscape Grass 铺）。
只保留落在地形范围内的摆放；--radius（米）再限制到 Boss 点周围的圆内（水平距离）。贴花、光雾效果网格（_Dec_ / _Lig_ / Fog）不导入。

导出：PakExport export --format gltf（网格 .glb、材质 .json、贴图 .png，按原作包内路径放在 Output/Exports 下）。
配方 ArtSource/Terrain/<区域>.props.json（原作数据，不提交）：
  meshes：原作网格 → { glb, asset（项目路径，沿用原作目录结构）, materials（槽位 → 原作材质实例）, collision, masked }
  materials：原作材质实例 → { asset, diffuse, normal, masked, twoSided }
  textures：原作贴图 → { png, asset, normal }
  placements：[ 网格, 位置(3), 四元数(4), 缩放(3) ]
项目路径：Client/Content/Aki/<路径> → /Game/Environment/WuwaAssets/<路径>（不同区域共用的资产只导一份）。
"""
from __future__ import annotations

import argparse
import collections
import json
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
AKI = Path('C:/GamePakExtractor/Output/Exports/Client/Content/Aki')
EXPORTS = Path('C:/GamePakExtractor/Output/Exports')
TERRAIN = Path('C:/GamePakExtractor/Output/Terrain')
PAKEXPORT = Path('C:/GamePakExtractor/Tools/PakExport/bin/out/PakExport.exe')
ASSET_ROOT = '/Game/Environment/WuwaAssets'
SKIP_TOKENS = ('_Dec_', '_Lig_', 'Fog', '_Wat_', 'Decal')
# 小于这个尺寸（厘米，包围盒最长边）的物件不加碰撞（草、花、小石子），其余用复杂碰撞（攀爬需要贴合表面）
COLLISION_MIN_SIZE = 150.0


def project_path(pak_path: str) -> str:
    """Client/Content/Aki/Scene/.../SM_X(.SM_X) → /Game/Environment/WuwaAssets/Scene/.../SM_X"""
    p = pak_path.split('.')[0]
    if p.startswith('/Game/Aki/'):
        p = 'Client/Content/Aki/' + p[len('/Game/Aki/'):]
    for prefix, sub in (('Client/Content/Aki/', ''), ('Engine/Content/', 'Engine/'), ('Client/Content/', 'Client/')):
        if prefix in p:
            return f'{ASSET_ROOT}/{sub}{p.split(prefix, 1)[1]}'
    return f'{ASSET_ROOT}/Other/{p.lstrip("/")}'


def glb_info(path: Path) -> dict:
    data = path.read_bytes()
    n = struct.unpack_from('<I', data, 12)[0]
    gltf = json.loads(data[20:20 + n])
    materials = [m.get('name') for m in gltf.get('materials', [])]
    lo = [1e18] * 3
    hi = [-1e18] * 3
    for mesh in gltf['meshes']:
        for prim in mesh['primitives']:
            acc = gltf['accessors'][prim['attributes']['POSITION']]
            for i in range(3):
                lo[i] = min(lo[i], acc['min'][i])
                hi[i] = max(hi[i], acc['max'][i])
    return {'materials': materials, 'size': max(h - l for h, l in zip(hi, lo))}


def classify(textures: dict[str, str]) -> tuple[str | None, str | None]:
    """原作材质的贴图参数名不统一（PM_Diffuse、Overall_Diff、Tex Base Color、Base Color Map、BaseTex……），
    先按贴图文件名后缀认（_D 漫反射，_N / _NB / _NM 法线），认不出再看参数名。"""
    def base(path: str) -> str:
        return (path.split('.')[0].split('/')[-1]).upper()

    def norm(name: str) -> str:
        return name.lower().replace(' ', '').replace('_', '')

    diffuse = next((p for n, p in textures.items() if base(p).endswith('_D') and 'base' in norm(n)), None)         or next((p for p in textures.values() if base(p).endswith('_D')), None)         or next((p for n, p in textures.items() if any(k in norm(n) for k in ('basecolor', 'basetex', 'diff', 'albedo'))), None)
    normal = next((p for n, p in textures.items() if base(p).endswith(('_N', '_NB', '_NM')) and 'base' in norm(n)), None)         or next((p for p in textures.values() if base(p).endswith(('_N', '_NB', '_NM'))), None)
    return diffuse, normal


# 植被主材质（M_Foliage_Bush、M_Bush_*、M_Foliage_Leaf_*……）没有色彩图贴图参数，颜色参数默认也是白色，贴图是灰度；
# 结合 RampColorScope / BaseColorAffectIntensity 等参数推测颜色取自脚下地面（运行时虚拟纹理）。
# 这里用世界色彩图近似"地面颜色"（推测，不是原作数据）。树干（M_*Trunk*）贴图本身有颜色，不加。
FOLIAGE_ROOTS = ('M_Foliage_Bush', 'M_Bush_', 'M_Foliage_Leaf', 'M_Grass', 'M_Foliage_Grass')


# 特效类主材质（瀑布、体积光、玻璃、冰、蛛网、无光照……）需要专门的着色器，用通用材质只会是一块实心面片：这些网格不导入
EFFECT_ROOTS = ('M_Waterfall', 'M_VolumetricLightShaft', 'M_Glass', 'M_StandardIce', 'M_SpiderWeb', 'M_UnLit', 'WorldGridMaterial', 'M_Ocean')


# 原作主材质 → 是否按世界色彩图着色（这些主材质的贴图参数里有 WorldColorMap = T_LandscapeColorMap_D，
# 已从 M_CustomStone 的参数表确认；BaseColorAffectIntensity 默认 1）。颜色参数按名字取（StoneColorTint、Color Tint……）。
COLOR_MAP_SHADERS_HINT = ('Stone', 'Foliage', 'Tree', 'Grass', 'Plant', 'Vegetation')
TINT_PARAMS = ('StoneColorTint', 'Color Tint', 'ColorTint', 'BaseColorTint', 'DiffuseTint')


def resolve_shaders(material_keys: list[str]) -> dict[str, dict]:
    """原作材质实例 → { root（根材质路径）, colorMap（根材质是否用 WorldColorMap）, tint }。沿 Parent 往上导出 JSON。"""
    info: dict[str, dict] = {}
    cache: dict[str, dict] = {}

    def props_of(key: str) -> dict | None:
        if key in cache:
            return cache[key]
        path = EXPORTS / f'{key}.json'
        if not path.exists():
            return None
        exports = json.loads(path.read_text(encoding='utf-8'))
        obj = next((o for o in exports if o['Type'] in ('Material',) or 'MaterialInstance' in o['Type']), None)
        cache[key] = obj
        return obj

    def export_json(keys: set[str]) -> None:
        missing = [k for k in keys if not (EXPORTS / f'{k}.json').exists() or 'Textures' in (EXPORTS / f'{k}.json').read_text(encoding='utf-8')[:20]]
        if not missing:
            return
        listing = TERRAIN / 'props_materials.lst'
        listing.write_text('\n'.join(f'{k}.uasset' for k in missing), encoding='utf-8')
        subprocess.run([str(PAKEXPORT), 'json', f'@{listing}'], capture_output=True)
        for k in missing:
            cache.pop(k, None)

    # 网格导出时 CUE4Parse 写的材质 .json 是摘要格式（没有 Parent），这里换成完整的属性导出
    frontier = set(material_keys)
    chains = {k: [k] for k in material_keys}
    for _ in range(8):
        export_json(frontier)
        nxt = set()
        for key in frontier:
            obj = props_of(key)
            parent = ((obj or {}).get('Properties', {}).get('Parent') or {}).get('ObjectPath')
            if parent:
                pkey = parent.split('.')[0]
                nxt.add(pkey)
                for chain in chains.values():
                    if chain[-1] == key:
                        chain.append(pkey)
        if not nxt:
            break
        frontier = nxt
    for key, chain in chains.items():
        root = chain[-1]
        root_obj = props_of(root) or {}
        textures = json.dumps(root_obj.get('Properties', {}).get('CachedExpressionData', {}).get('Parameters', {}).get('TextureValues', []))
        tint = None
        for k in chain:  # 实例链上最近的颜色参数优先
            for p in (props_of(k) or {}).get('Properties', {}).get('VectorParameterValues', []):
                if p['ParameterInfo']['Name'] in TINT_PARAMS and tint is None:
                    v = p['ParameterValue']
                    tint = [round(v['R'], 4), round(v['G'], 4), round(v['B'], 4)]
        info[key] = {'root': root, 'colorMap': 'T_LandscapeColorMap_D' in textures, 'tint': tint}
    return info


_INDEX: dict[str, Path] | None = None


def material_index() -> dict[str, Path]:
    """导出目录里所有 MI_* / M_* 的 JSON：文件名 → 路径（遍历一次，后面查表）。"""
    global _INDEX
    if _INDEX is None:
        import os
        _INDEX = {}
        for root, _dirs, files in os.walk(AKI):
            for f in files:
                if f.endswith('.json') and (f.startswith('MI_') or f.startswith('M_')):
                    _INDEX.setdefault(f[:-5], Path(root) / f)
    return _INDEX


def main() -> int:
    sys.stdout.reconfigure(encoding='utf-8')
    parser = argparse.ArgumentParser()
    parser.add_argument('--region', required=True)
    parser.add_argument('--only', default='')
    parser.add_argument('--limit', type=int, default=0)
    parser.add_argument('--skip-export', action='store_true')
    parser.add_argument('--radius', type=float, default=0, help='meters around the boss point (horizontal)')
    args = parser.parse_args()

    terrain = json.loads((HERE / f'{args.region}.terrain.json').read_text(encoding='utf-8'))
    x0, y0 = terrain['location'][:2]
    span = (terrain['size'] - 1) * terrain['scale'][0]
    raw = json.loads((TERRAIN / f'{args.region}_placements_raw.json').read_text(encoding='utf-8'))
    placements = [p for p in raw if p['kind'] in ('component', 'fastgeo') and p['mesh']
                  and x0 <= p['t'][0] <= x0 + span and y0 <= p['t'][1] <= y0 + span
                  and not any(tok in p['mesh'] for tok in SKIP_TOKENS)]
    if args.radius > 0:
        bx, by = terrain['boss'][:2]
        r2 = (args.radius * 100) ** 2
        placements = [p for p in placements if (p['t'][0] - bx) ** 2 + (p['t'][1] - by) ** 2 <= r2]
    counts = collections.Counter(p['mesh'].split('.')[0] for p in placements)
    only = {s.strip() for s in args.only.split(',') if s.strip()}
    meshes = [m for m, _ in counts.most_common() if not only or m.split('/')[-1] in only]
    if args.limit:
        meshes = meshes[:args.limit]
    print(f'{len(placements)} placements of {len(counts)} meshes; preparing {len(meshes)}')

    missing = [m for m in meshes if not (EXPORTS / f'{m}.glb').exists()]
    if missing and not args.skip_export:
        listing = TERRAIN / f'{args.region}_props_export.lst'
        listing.write_text('\n'.join(f'{m}.uasset' for m in missing), encoding='utf-8')
        print(f'exporting {len(missing)} meshes ...')
        result = subprocess.run([str(PAKEXPORT), 'export', f'@{listing}', '--format', 'gltf'],
                                capture_output=True, text=True, encoding='utf-8', errors='replace')
        print(result.stderr.strip().splitlines()[-1] if result.stderr.strip() else 'no output')

    # 第一遍：每个网格的槽位 → 原作材质实例（glb 里的材质名 → 导出目录里同名的材质 JSON）
    recipe_meshes, materials, textures = {}, {}, {}
    mesh_slots, mesh_info = {}, {}
    for m in meshes:
        glb = EXPORTS / f'{m}.glb'
        if not glb.exists():
            print('  missing export', m)
            continue
        info = glb_info(glb)
        slots = []
        for mat_name in info['materials']:
            # 材质 JSON 和网格不一定在同一个资产目录下：先在网格所在的资产包里找，再查整个导出目录的索引
            mat_json = next((p for p in (EXPORTS / m).parent.parent.glob(f'**/{mat_name}.json')), None) if mat_name else None
            if mat_json is None and mat_name:
                mat_json = material_index().get(mat_name)
            slots.append(mat_json.relative_to(EXPORTS).with_suffix('').as_posix() if mat_json else None)
        mesh_slots[m], mesh_info[m] = slots, info

    # 第二遍：材质实例的完整属性（父级链、贴图参数、混合模式、颜色参数、是否用世界色彩图）
    keys = sorted({k for slots in mesh_slots.values() for k in slots if k})
    print(f'resolving {len(keys)} materials ...')
    shaders = resolve_shaders(keys)
    for key in keys:
        chain_tex: dict[str, str] = {}
        blend = None
        k = key
        for _ in range(8):  # 实例链上最近的贴图 / 混合模式优先
            path = EXPORTS / f'{k}.json'
            if not path.exists():
                break
            obj = next((o for o in json.loads(path.read_text(encoding='utf-8')) if o['Type'] == 'Material' or 'MaterialInstance' in o['Type']), None)
            if obj is None:
                break
            props = obj.get('Properties', {})
            for t in props.get('TextureParameterValues', []):
                chain_tex.setdefault(t['ParameterInfo']['Name'], (t.get('ParameterValue') or {}).get('ObjectPath', ''))
            over = props.get('BasePropertyOverrides', {})
            if blend is None and over.get('bOverride_BlendMode'):
                blend = over.get('BlendMode')
            if blend is None and obj['Type'] == 'Material' and props.get('BlendMode'):
                blend = props.get('BlendMode')
            parent = (props.get('Parent') or {}).get('ObjectPath')
            if not parent:
                break
            k = parent.split('.')[0]
        # 引擎 / Render/Common 下的默认贴图（纯白、默认法线）不算
        chain_tex = {n: v for n, v in chain_tex.items() if v and '/Render/Common/' not in v and 'Engine/' not in v}
        diffuse, normal = classify(chain_tex)
        masked = bool(blend) and 'Masked' in str(blend)
        shader = shaders.get(key, {})
        materials[key] = {'asset': project_path(key), 'diffuse': diffuse and diffuse.split('.')[0],
                          'normal': normal and normal.split('.')[0], 'masked': masked, 'twoSided': masked,
                          'root': shader.get('root'), 'tint': shader.get('tint'),
                          'colorMap': shader.get('colorMap', False) or any(r in (shader.get('root') or '') for r in FOLIAGE_ROOTS)}
        for path, is_normal in ((diffuse, False), (normal, True)):
            if not path:
                continue
            tkey = path.split('.')[0]
            png = EXPORTS / f'{tkey}.png'
            if png.exists():
                textures[tkey] = {'png': png.as_posix(), 'asset': project_path(tkey), 'normal': is_normal}
            else:
                materials[key]['diffuse' if not is_normal else 'normal'] = None

    skipped_effects = 0
    for m, slots in mesh_slots.items():
        info = mesh_info[m]
        if any(any(r in (materials.get(sl, {}).get('root') or '') for r in EFFECT_ROOTS) for sl in slots if sl):
            skipped_effects += 1
            continue
        masked = any(materials.get(s, {}).get('masked') for s in slots if s)
        recipe_meshes[m] = {'glb': (EXPORTS / f'{m}.glb').as_posix(), 'asset': project_path(m), 'materials': slots,
                            'masked': masked, 'collision': (not masked) and info['size'] >= COLLISION_MIN_SIZE,
                            'size': round(info['size'], 1), 'count': counts[m]}
    roots = collections.Counter(v['root'].split('/')[-1] if v['root'] else None for v in materials.values())
    print('root materials:', roots.most_common(15))
    print('skipped effect meshes:', skipped_effects)
    print('color-mapped materials:', sum(1 for v in materials.values() if v['colorMap']), 'of', len(materials))

    kept = [p for p in placements if p['mesh'].split('.')[0] in recipe_meshes]
    recipe = {
        'region': args.region,
        'level': terrain['level'],
        'radius_m': args.radius,
        'meshes': recipe_meshes,
        'materials': materials,
        'textures': textures,
        'placements': [[p['mesh'].split('.')[0], *[round(v, 3) for v in p['t']], *[round(v, 6) for v in p['q']],
                        *[round(v, 4) for v in p['s']]] for p in kept],
    }
    out = HERE / f'{args.region}.props.json'
    out.write_text(json.dumps(recipe, ensure_ascii=False), encoding='utf-8')
    print(out, f'{len(recipe_meshes)} meshes, {len(materials)} materials, {len(textures)} textures, {len(kept)} placements')
    no_tex = [k for k, v in materials.items() if not v['diffuse']]
    if no_tex:
        print(f'  {len(no_tex)} materials without a recognised diffuse texture, e.g. {no_tex[:3]}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
