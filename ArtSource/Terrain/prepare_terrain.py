"""从原作地形里切出一块区域 → UE Landscape 导入配方（高度 + 材质层权重 + 每个组件的材质实例）。
  python -I prepare_terrain.py --region crownless

数据来源：PakExport heightmap 的输出 <地图>_heightmap.json（每个 LandscapeComponent 的高度、各材质层权重、材质实例），
以及 terrain_overview.py 拼好的 <地图>_<组>.npy / .json（整张高度图和世界变换）。

区域以一个世界坐标为中心取 N×N 个组件，并对齐到原作的组件网格（每个组件 62 个四边形、2×2 子区块），所以：
  - 顶点间距、高度缩放、世界位置都与原作一致，之后按原作世界坐标摆放的静态物件能直接对上；
  - 我们的每个组件正好对应原作的一个组件，材质层权重和材质实例都能一一对应。

输出（原作数据，不提交）：
  C:/GamePakExtractor/Output/Terrain/<区域>.r16             16 位高度（原作原始值，32768 = 0），行 = Y
  C:/GamePakExtractor/Output/Terrain/<区域>_<层>.r8         各材质层权重（8 位），行 = Y；DataLayer 是原作的挖洞层
  ArtSource/Terrain/<区域>.terrain.json                     WuwaTerrainImport 的配方（尺寸、缩放、位置、关卡、层、组件材质）
"""
from __future__ import annotations

import argparse
import base64
import collections
import json
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
TERRAIN = Path('C:/GamePakExtractor/Output/Terrain')

REGIONS = {
    # 无冠者谷地：原作 TsEntity_BOSS_无冠者 出生点（关卡实体表 MapId 8，实体 310000014），坐标单位厘米
    'crownless': {
        'map': 'HuangLong',
        'group': 0,
        'center_cm': [-111319.64, 73209.74],
        'components': 16,
        'level': '/Game/Environment/HuangLong_Crownless/Maps/L_HuangLong_Crownless',
        'boss_cm': [-111319.64, 73209.74, 6911.12],
    },
}
COMPONENT_QUADS = 62
SUBSECTIONS = 2
# 原作地形里有少量离群顶点（和周围 8 个顶点的中值差几十米，形成尖刺），游戏里被岩石网格盖住看不到；
# 在网格导入之前先把它们替换成中值。只动差值超过这个阈值的顶点。
SPIKE_THRESHOLD_M = 20.0


def despike(heights: np.ndarray, z_scale: float) -> tuple[np.ndarray, int]:
    from numpy.lib.stride_tricks import sliding_window_view
    h = heights.astype(np.float64)
    padded = np.pad(h, 1, mode='edge')
    window = sliding_window_view(padded, (3, 3)).reshape(h.shape[0], h.shape[1], 9)
    median = np.median(np.delete(window, 4, axis=2), axis=2)
    threshold = SPIKE_THRESHOLD_M * 100 / z_scale * 128
    spikes = np.abs(h - median) > threshold
    out = heights.copy()
    out[spikes] = np.round(median[spikes]).astype(out.dtype)
    return out, int(spikes.sum())


def main() -> int:
    sys.stdout.reconfigure(encoding='utf-8')
    parser = argparse.ArgumentParser()
    parser.add_argument('--region', required=True, choices=sorted(REGIONS))
    region_name = parser.parse_args().region
    region = REGIONS[region_name]

    stem = f"{region['map']}_{region['group']}"
    grid = np.load(TERRAIN / f'{stem}.npy')
    meta = json.loads((TERRAIN / f'{stem}.json').read_text(encoding='utf-8'))
    ox, oy, oz = meta['origin']
    sx, sy, sz = meta['scale']
    bx, by = meta['sectionBase0']

    # 区域左上角对齐到原作组件网格：SectionBase（= bx + col）是 62 的倍数
    n = region['components']
    quads = n * COMPONENT_QUADS
    base_x = int(round((region['center_cm'][0] - ox) / sx)) - quads // 2
    base_y = int(round((region['center_cm'][1] - oy) / sy)) - quads // 2
    base_x -= base_x % COMPONENT_QUADS
    base_y -= base_y % COMPONENT_QUADS
    col, row = base_x - bx, base_y - by
    if col < 0 or row < 0 or col + quads + 1 > grid.shape[1] or row + quads + 1 > grid.shape[0]:
        raise SystemExit(f'region {region_name} leaves the stitched terrain')
    patch = grid[row:row + quads + 1, col:col + quads + 1]
    missing = int((patch < 0).sum())
    if missing:
        # 少量缺口用整块的中位数补（拼接时没有组件覆盖的位置）
        patch = patch.copy()
        patch[patch < 0] = int(np.median(patch[patch >= 0]))
    heights, spikes = despike(patch.astype('<u2'), sz)
    (TERRAIN / f'{region_name}.r16').write_bytes(heights.tobytes())

    # 材质层权重和组件材质：直接从 heightmap 输出里取落在区域内的原作组件（对齐后一一对应）
    size = quads + 1
    layers: dict[str, np.ndarray] = {}
    materials = [[None] * n for _ in range(n)]
    found = 0
    for comp in json.loads((TERRAIN / f"{region['map']}_heightmap.json").read_text(encoding='utf-8')):
        cx = (comp['sectionBaseX'] - base_x) // COMPONENT_QUADS
        cy = (comp['sectionBaseY'] - base_y) // COMPONENT_QUADS
        if (comp['sectionBaseX'] - base_x) % COMPONENT_QUADS or (comp['sectionBaseY'] - base_y) % COMPONENT_QUADS:
            continue
        if not (0 <= cx < n and 0 <= cy < n):
            continue
        found += 1
        k = comp['sizeQuads'] + 1
        x0, y0 = cx * COMPONENT_QUADS, cy * COMPONENT_QUADS
        for name, data in comp['layers'].items():
            weights = np.frombuffer(base64.b64decode(data), dtype=np.uint8).reshape(k, k)
            target = layers.setdefault(name, np.zeros((size, size), np.uint8))
            target[y0:y0 + k, x0:x0 + k] = np.maximum(target[y0:y0 + k, x0:x0 + k], weights)
        materials[cy][cx] = (comp.get('material') or '').split('.')[0] or None
    if found != n * n:
        raise SystemExit(f'expected {n * n} original components in the region, found {found}')
    layer_files = {}
    for name, weights in sorted(layers.items(), key=lambda kv: (len(kv[0]), kv[0])):
        path = TERRAIN / f'{region_name}_{name}.r8'
        path.write_bytes(weights.tobytes())
        layer_files[name] = str(path).replace('\\', '/')
    material_counts = collections.Counter(m for line in materials for m in line)

    location = [ox + base_x * sx, oy + base_y * sy, oz]
    height_m = (heights.astype(np.float64) - 32768) / 128 * sz / 100 + oz / 100
    recipe = {
        'region': region_name,
        'level': region['level'],
        'heightmap': str(TERRAIN / f'{region_name}.r16').replace('\\', '/'),
        'size': size,
        'componentQuads': COMPONENT_QUADS,
        'subsections': SUBSECTIONS,
        'location': location,
        'scale': [sx, sy, sz],
        'boss': region['boss_cm'],
        'source': f"{stem} sectionBase ({base_x}, {base_y}), {n}x{n} components",
        'missingVertices': missing,
        'despikedVertices': spikes,
        'height_m': [round(float(height_m.min()), 1), round(float(height_m.max()), 1)],
        # 层名 → 8 位权重文件；DataLayer 是原作的挖洞层（游戏里洞被网格盖住，网格导入之前先不用）
        'layers': layer_files,
        # 组件材质：原作材质实例的路径（行 = Y），以及各自的组件数
        'componentMaterials': materials,
        'materialCounts': dict(material_counts.most_common()),
    }
    out = HERE / f'{region_name}.terrain.json'
    out.write_text(json.dumps(recipe, indent=1, ensure_ascii=False), encoding='utf-8')
    print(out)
    print(json.dumps({k: v for k, v in recipe.items() if k != 'componentMaterials'}, ensure_ascii=False, indent=1))
    return 0


if __name__ == '__main__':
    sys.exit(main())
