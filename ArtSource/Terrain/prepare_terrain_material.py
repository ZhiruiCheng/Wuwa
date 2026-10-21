"""原作地形材质实例 → 项目地形材质的配方（每个材质实例 10 个图层的切片、平铺、颜色、粗糙度）。
  python -I prepare_terrain_material.py --region crownless

原作（Aki/Scene/Landscape/Mat/MI_Landscape_world*，父材质 M_Landscape）所有图层共用两张 Texture2DArray：
颜色 T4_Array_01_C、法线 T4_Array_01_N，每个图层 N 用参数 <N>SliceIndex 选切片，<N>LayerScale 是平铺尺寸（厘米），
<N>ColorTint 乘颜色，<N>MinRoughness / <N>MaxRoughness 是粗糙度范围。参数没写在实例里的取父实例 MI_Landscape 的值，
再没有就用下面的 DEFAULTS。挖洞版本（MI_LandscapeHole_*）的图层参数与同名普通版本一致，导入时合并。

原作地表的颜色大部分来自一张覆盖整个大世界的手绘色彩图：M_Landscape 的贴图参数 WorldLandscapeColorMap
（T_LandscapeBaseColor_D，2048²）。它按世界坐标平铺：UV = (世界 XY + 409600) / 819200（u = X、v = Y，覆盖原点周围
8.192 公里见方）。这个规则取自同一材质的 TaskColorMapUVScaleAndBias = (819200, 819200, 409600, 409600)，并用瑝珑
地形验证过（画出的山体、火山口、台地的边界和高度图对得上，见 Output/Terrain/colormap_overlay_basecolor.png）。
色彩图与图层颜色怎么混合在编译后的材质里看不到，这里用 图层颜色 × 色彩图 × 2，强度用原作的 BaseColorAffectIntensity（1）。
切片 7 是草地：细节在 G 通道、R/B 几乎是常数，颜色由色彩图给；这里取 G 通道生成灰度草地贴图（中灰，乘色彩图后接近原作色）。

输出 ArtSource/Terrain/<区域>.material.json（WuwaTerrainMaterialSetup 的配方）和生成的草地贴图 PNG（原作数据，不提交）。
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
AKI = Path('C:/GamePakExtractor/Output/Exports/Client/Content/Aki')
TERRAIN = Path('C:/GamePakExtractor/Output/Terrain')
PAKEXPORT = Path('C:/GamePakExtractor/Tools/PakExport/bin/out/PakExport.exe')
ARRAY_DIR = AKI / 'Scene/Landscape/Tex'
LAYERS = 10
DEFAULTS = {'SliceIndex': 0.0, 'LayerScale': 500.0, 'MinRoughness': 0.8, 'MaxRoughness': 0.8}
GRASS_SLICE = 7
GRASS_TINT = (1.0, 1.0, 1.0)
COLOR_MAP = AKI / 'Scene/Landscape/ColorMap/T_LandscapeBaseColor_D.png'
# 原作 TaskColorMapUVScaleAndBias，WorldLandscapeColorMap 用同一套（已验证）：UV = (XY + ZW) / XY
COLOR_MAP_UV_SCALE_BIAS = [819200.0, 819200.0, 409600.0, 409600.0]

# 项目里的资产位置：主材质和贴图所有区域共用；材质实例、图层信息按区域放
COMMON = '/Game/Environment/Common/Landscape'
REGION_ROOT = {'crownless': '/Game/Environment/HuangLong_Crownless'}


def ensure_json(package: str) -> dict:
    path = AKI / f'{package}.json'
    if not path.exists():
        subprocess.run([str(PAKEXPORT), 'json', f'Client/Content/Aki/{package}.uasset'], capture_output=True, check=True)
    exports = json.loads(path.read_text(encoding='utf-8'))
    return next(o for o in exports if 'MaterialInstance' in o['Type'])['Properties']


def ensure_slices() -> None:
    if not (ARRAY_DIR / 'T4_Array_01_C_LAYER0.png').exists() or not (ARRAY_DIR / 'T4_Array_01_N_LAYER0.png').exists():
        subprocess.run([str(PAKEXPORT), 'export', 'Client/Content/Aki/Scene/Landscape/Tex/T4_Array_01_C.uasset',
                        'Client/Content/Aki/Scene/Landscape/Tex/T4_Array_01_N.uasset'], capture_output=True, check=True)


def parameters(props: dict) -> dict:
    out = {}
    for p in props.get('ScalarParameterValues', []):
        out[p['ParameterInfo']['Name']] = p['ParameterValue']
    for p in props.get('VectorParameterValues', []):
        v = p['ParameterValue']
        out[p['ParameterInfo']['Name']] = [v['R'], v['G'], v['B']]
    return out


def layer_params(instance: dict, parent: dict, n: int) -> dict:
    def get(name, default):
        key = f'{n}{name}'
        return instance.get(key, parent.get(key, default))
    slice_index = int(get('SliceIndex', DEFAULTS['SliceIndex']))
    lo, hi = get('MinRoughness', DEFAULTS['MinRoughness']), get('MaxRoughness', DEFAULTS['MaxRoughness'])
    # 原作在 Min..Max 之间按贴图插值（颜色切片的 alpha 大多在 0.7~0.9）；这里取 lerp(Min, Max, 0.85)。
    # Min = Max = 0 的图层当作没设（照搬成 0 会让地面像湿的一样反光），用父实例的默认 DEFAULTS
    if lo <= 0 and hi <= 0:
        roughness = DEFAULTS['MinRoughness']
    elif hi <= lo:
        roughness = lo
    else:
        roughness = lo + (hi - lo) * 0.85
    return {
        'slice': slice_index,
        'scale': float(get('LayerScale', DEFAULTS['LayerScale'])),
        'tint': [round(c, 4) for c in get('ColorTint', [1.0, 1.0, 1.0])],
        'roughness': round(min(max(roughness, 0.0), 1.0), 3),
    }


def make_grass() -> Path:
    src = np.asarray(Image.open(ARRAY_DIR / f'T4_Array_01_C_LAYER{GRASS_SLICE}.png').convert('RGBA'), dtype=np.float64)
    g = src[..., 1] / 255.0
    rgb = np.stack([g * GRASS_TINT[0], g * GRASS_TINT[1], g * GRASS_TINT[2]], axis=-1)
    # 归一到线性 0.21（sRGB 约 0.5，中灰），乘 色彩图 × 2 后颜色接近色彩图本身
    rgb = rgb / max(rgb.mean() / 0.21, 1e-6)
    out = TERRAIN / 'T_Landscape_Grass_D.png'
    Image.fromarray((np.clip(rgb, 0, 1) ** (1 / 2.2) * 255).astype(np.uint8)).save(out)
    return out


def main() -> int:
    sys.stdout.reconfigure(encoding='utf-8')
    parser = argparse.ArgumentParser()
    parser.add_argument('--region', required=True, choices=sorted(REGION_ROOT))
    region = parser.parse_args().region
    terrain = json.loads((HERE / f'{region}.terrain.json').read_text(encoding='utf-8'))
    ensure_slices()

    parent = parameters(ensure_json('Render/MaterialInstance/Landscape/MI_Landscape'))
    instances, aliases, used_slices = {}, {}, set()
    for path in terrain['materialCounts']:
        name = path.split('/')[-1]
        base = name.replace('MI_LandscapeHole_', 'MI_Landscape_')
        aliases[name] = base
        if base in instances:
            continue
        props = parameters(ensure_json(f'Scene/Landscape/Mat/{base}'))
        layers = {str(n): layer_params(props, parent, n) for n in range(1, LAYERS + 1)}
        instances[base] = {'layers': layers}
        used_slices |= {layer['slice'] for layer in layers.values()}

    grass = make_grass()
    textures = {}
    for s in sorted(used_slices):
        color = grass if s == GRASS_SLICE else ARRAY_DIR / f'T4_Array_01_C_LAYER{s}.png'
        textures[f'T_Landscape_{s:02d}_D'] = {'file': str(color).replace('\\', '/'), 'normal': False}
        textures[f'T_Landscape_{s:02d}_N'] = {'file': str(ARRAY_DIR / f'T4_Array_01_N_LAYER{s}.png').replace('\\', '/'), 'normal': True}

    if not COLOR_MAP.exists():
        subprocess.run([str(PAKEXPORT), 'export', 'Client/Content/Aki/Scene/Landscape/ColorMap/T_LandscapeBaseColor_D.uasset'], capture_output=True, check=True)
    textures['T_LandscapeBaseColor_D'] = {'file': COLOR_MAP.as_posix(), 'normal': False, 'clamp': True}
    root = REGION_ROOT[region]
    recipe = {
        'master': f'{COMMON}/M_WuwaLandscape',
        'textureFolder': f'{COMMON}/Textures',
        'instanceFolder': f'{root}/Landscape/Materials',
        'layerInfoFolder': f'{root}/Landscape/LayerInfos',
        'layers': LAYERS,
        'colorMap': 'T_LandscapeBaseColor_D',
        'colorMapUVScaleBias': COLOR_MAP_UV_SCALE_BIAS,
        'colorMapIntensity': 1.0,
        'textures': textures,
        # 原作材质实例名 → 项目材质实例名 MI_<原名去掉 MI_>；挖洞版本指向普通版本
        'instances': {f"MI_{k.removeprefix('MI_')}": v for k, v in instances.items()},
        'aliases': {k: f"MI_{v.removeprefix('MI_')}" for k, v in aliases.items()},
        'notes': ['WorldLandscapeColorMap: layer color x color map x 2 (blend formula assumed; UV mapping verified)',
                  f'slice {GRASS_SLICE} replaced by its G channel as grey'],
    }
    out = HERE / f'{region}.material.json'
    out.write_text(json.dumps(recipe, indent=1, ensure_ascii=False), encoding='utf-8')
    print(out)
    for k, v in recipe['instances'].items():
        print(k, {n: (l['slice'], l['scale']) for n, l in v['layers'].items()})
    print(len(textures), 'textures')
    return 0


if __name__ == '__main__':
    sys.exit(main())
