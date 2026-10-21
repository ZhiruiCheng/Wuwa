"""原作地形高度 → 拼成整张高度图 + 总览图（山体阴影 + 高程着色 + 候选区域）。
  python -I terrain_overview.py C:/GamePakExtractor/Output/Terrain/HuangLong_heightmap.json

输入是 PakExport heightmap 的输出（每个 LandscapeComponent 一条）。同一张地图可能有多块地形（不同原点/缩放），
按 (原点, 缩放) 分组，每组单独拼。输出到输入文件旁边：
  <名字>_<组>.npy        int32 原始高度（-1 = 没有数据），行 = Y，列 = X（地形顶点坐标，原点在 SectionBase 最小处）
  <名字>_<组>.json       该组的世界变换和网格范围
  <名字>_overview.png    总览图：高程着色 + 山体阴影，标出起伏最大的若干块候选区域（编号见 *_candidates.json）
"""
from __future__ import annotations

import base64
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

CANDIDATE_SIZE_M = 1500   # 候选区域边长（米）
CANDIDATES = 8


def load_groups(path: Path) -> dict:
    groups: dict = {}
    for c in json.loads(path.read_text(encoding='utf-8')):
        scale = c['proxyScale']
        loc = c['proxyLocation']
        off = c['sectionOffset']
        # 地形原点 = 代理位置 - SectionOffset × 缩放（同一块地形的所有代理算出来一样）
        origin = (round(loc['X'] - off['X'] * scale['X']), round(loc['Y'] - off['Y'] * scale['Y']), round(loc['Z']))
        key = (origin, round(scale['X'], 3), round(scale['Y'], 3), round(scale['Z'], 3))
        groups.setdefault(key, []).append(c)
    return groups


def stitch(components: list) -> tuple[np.ndarray, int, int]:
    xs = [c['sectionBaseX'] for c in components]
    ys = [c['sectionBaseY'] for c in components]
    size = components[0]['sizeQuads']
    x0, y0 = min(xs), min(ys)
    w = max(xs) - x0 + size + 1
    h = max(ys) - y0 + size + 1
    grid = np.full((h, w), -1, dtype=np.int32)
    for c in components:
        n = c['sizeQuads'] + 1
        data = np.frombuffer(base64.b64decode(c['heights']), dtype='<u2').reshape(n, n)
        bx, by = c['sectionBaseX'] - x0, c['sectionBaseY'] - y0
        grid[by:by + n, bx:bx + n] = data
    return grid, x0, y0


def shade(height_m: np.ndarray, valid: np.ndarray, cell_m: float) -> np.ndarray:
    gy, gx = np.gradient(np.where(valid, height_m, np.nanmean(height_m[valid])), cell_m)
    # 光从左上方照来
    lx, ly, lz = -0.6, -0.6, 0.53
    norm = np.sqrt(gx * gx + gy * gy + 1)
    return np.clip((-gx * lx - gy * ly + lz) / norm, 0, 1)


def colorize(height_m: np.ndarray, valid: np.ndarray) -> np.ndarray:
    lo, hi = np.percentile(height_m[valid], [1, 99])
    t = np.clip((height_m - lo) / max(hi - lo, 1), 0, 1)
    stops = np.array([[40, 90, 60], [110, 150, 80], [190, 170, 110], [150, 120, 100], [240, 240, 240]], dtype=float)
    pos = t * (len(stops) - 1)
    i = np.clip(pos.astype(int), 0, len(stops) - 2)
    f = (pos - i)[..., None]
    return stops[i] * (1 - f) + stops[i + 1] * f


def candidates(height_m: np.ndarray, valid: np.ndarray, cell_m: float) -> list:
    """按块统计起伏：块内高差（P95 - P5）与坡度均值，要求整块都有数据。"""
    win = int(CANDIDATE_SIZE_M / cell_m)
    step = win // 2
    gy, gx = np.gradient(np.where(valid, height_m, 0), cell_m)
    slope = np.degrees(np.arctan(np.sqrt(gx * gx + gy * gy)))
    scored = []
    for y in range(0, height_m.shape[0] - win, step):
        for x in range(0, height_m.shape[1] - win, step):
            v = valid[y:y + win, x:x + win]
            if v.mean() < 0.999:
                continue
            hh = height_m[y:y + win, x:x + win]
            relief = float(np.percentile(hh, 95) - np.percentile(hh, 5))
            s = slope[y:y + win, x:x + win]
            steep = float((s > 35).mean())      # 适合攀爬的陡坡占比
            gentle = float((s < 15).mean())     # 适合战斗的平缓地占比
            scored.append({'x': x, 'y': y, 'size': win, 'relief_m': round(relief, 1), 'steep': round(steep, 3),
                           'gentle': round(gentle, 3), 'score': relief * (0.5 + steep) * (0.3 + gentle)})
    scored.sort(key=lambda s: -s['score'])
    picked = []
    for s in scored:
        if all(abs(s['x'] - p['x']) >= win or abs(s['y'] - p['y']) >= win for p in picked):
            picked.append(s)
        if len(picked) >= CANDIDATES:
            break
    return picked


def main() -> int:
    sys.stdout.reconfigure(encoding='utf-8')
    src = Path(sys.argv[1])
    stem = src.stem.replace('_heightmap', '')
    groups = load_groups(src)
    print(f'{len(groups)} landscape group(s)')
    best = max(groups.items(), key=lambda kv: len(kv[1]))
    for gi, ((origin, sx, sy, sz), comps) in enumerate(sorted(groups.items(), key=lambda kv: -len(kv[1]))):
        grid, x0, y0 = stitch(comps)
        valid = grid >= 0
        height_m = (grid.astype(np.float64) - 32768) / 128 * sz / 100 + origin[2] / 100
        meta = {'origin': origin, 'scale': [sx, sy, sz], 'sectionBase0': [x0, y0], 'shape': list(grid.shape),
                'components': len(comps), 'cell_m': sx / 100,
                'height_m': [round(float(height_m[valid].min()), 1), round(float(height_m[valid].max()), 1)]}
        np.save(src.with_name(f'{stem}_{gi}.npy'), grid)
        src.with_name(f'{stem}_{gi}.json').write_text(json.dumps(meta, indent=1), encoding='utf-8')
        print(gi, meta)
        if (origin, sx, sy, sz) != best[0]:
            continue
        cell_m = sx / 100
        cands = candidates(height_m, valid, cell_m)
        rgb = colorize(height_m, valid) * (0.35 + 0.65 * shade(height_m, valid, cell_m))[..., None]
        rgb[~valid] = (30, 30, 36)
        img = Image.fromarray(rgb.astype(np.uint8))
        scale = 1600 / max(img.size)
        img = img.resize((int(img.width * scale), int(img.height * scale)), Image.LANCZOS)
        draw = ImageDraw.Draw(img)
        try:
            font = ImageFont.truetype('msyh.ttc', 22)
        except OSError:
            font = ImageFont.load_default()
        for i, c in enumerate(cands, 1):
            box = [c['x'] * scale, c['y'] * scale, (c['x'] + c['size']) * scale, (c['y'] + c['size']) * scale]
            draw.rectangle(box, outline=(255, 70, 50), width=3)
            draw.text((box[0] + 6, box[1] + 4), str(i), fill=(255, 255, 255), font=font, stroke_width=2, stroke_fill=(0, 0, 0))
            c['id'] = i
            # 世界坐标（厘米）方便之后定位
            c['world_cm'] = [origin[0] + (x0 + c['x']) * sx, origin[1] + (y0 + c['y']) * sy]
        km = 1000 / cell_m * scale
        draw.line([20, img.height - 30, 20 + km, img.height - 30], fill=(255, 255, 255), width=4)
        draw.text((24, img.height - 60), '1 km', fill=(255, 255, 255), font=font, stroke_width=2, stroke_fill=(0, 0, 0))
        out = src.with_name(f'{stem}_overview.png')
        img.save(out)
        src.with_name(f'{stem}_candidates.json').write_text(json.dumps(cands, indent=1), encoding='utf-8')
        print(out)
        for c in cands:
            print(c)
    return 0


if __name__ == '__main__':
    sys.exit(main())
