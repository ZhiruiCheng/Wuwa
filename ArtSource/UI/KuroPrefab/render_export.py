"""按 LGUI3 的锚点规则把 kuro_export.py 的输出画出来（检查换算，不依赖 UE）。

  python -I render_export.py ../HUD/Kuro/hud_main.layout.json out.png [宽 高]
FUIAnchorData：锚点矩形 = 父矩形按 min/max 取；轴心位置 = 锚点参考点（锚点矩形里按轴心插值）+ pos；尺寸 = 锚点矩形尺寸 + size。
缩放绕轴心，作用到自己和子节点。
"""
import json
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

REPO = Path(__file__).resolve().parents[3]


def main(layout_path, out_path, width=1920, height=1080):
    data = json.loads(Path(layout_path).read_text(encoding='utf-8'))
    nodes = data['nodes']
    canvas = Image.new('RGBA', (width, height), (40, 44, 52, 255))
    draw = ImageDraw.Draw(canvas)
    frames = {}  # 下标 → (左, 下, 宽, 高, 变换 ox, oy, kx, ky)
    for i, n in enumerate(nodes):
        if n['parent'] < 0:
            pl, pb, pw, ph, xf = 0.0, 0.0, float(width), float(height), (0.0, 0.0, 1.0, 1.0)
        else:
            pl, pb, pw, ph, xf = frames[n['parent']]
        a = n['anchor']
        rect = []
        for axis, (start, length) in enumerate(((pl, pw), (pb, ph))):
            lo, hi = start + length * a['min'][axis], start + length * a['max'][axis]
            size = (hi - lo) + a['size'][axis]
            pivot_pos = lo + (hi - lo) * a['pivot'][axis] + a['pos'][axis]
            rect.append((pivot_pos - size * a['pivot'][axis], size, pivot_pos))
        (left, w, pxp), (bottom, h, pyp) = rect
        sx, sy = n.get('scale', [1, 1])
        if n.get('rotation', 0) == 180:  # 绕轴心转半圈 = 两个轴取反
            sx, sy = -sx, -sy
        ox, oy, kx, ky = xf
        own = (ox + kx * (pxp - sx * pxp), oy + ky * (pyp - sy * pyp), kx * sx, ky * sy)
        frames[i] = (left, bottom, w, h, own)
        L, B = own[0] + own[2] * left, own[1] + own[3] * bottom
        W, H = own[2] * w, own[3] * h
        flip = (W < 0, H < 0)
        if W < 0:
            L, W = L + W, -W
        if H < 0:
            B, H = B + H, -H
        if W < 1 or H < 1:
            continue
        top = height - (B + H)
        if n['type'] == 'Texture':
            img = Image.open(REPO / n['texture']).convert('RGBA').resize((int(W), int(H)))
            if flip[0]:
                img = img.transpose(Image.FLIP_LEFT_RIGHT)
            if flip[1]:
                img = img.transpose(Image.FLIP_TOP_BOTTOM)
            c = n['color']
            if c != [255, 255, 255, 255]:
                r, g, b, al = img.split()
                img = Image.merge('RGBA', [ch.point(lambda v, m=m: v * m // 255) for ch, m in zip((r, g, b, al), c)])
            canvas.alpha_composite(img, (int(L), int(top)))
        elif n['type'] == 'Text' and n['text']['text']:
            t = n['text']
            font = ImageFont.truetype('C:/Windows/Fonts/msyh.ttc', max(1, int(t['size'] * abs(own[2]))))
            tw = draw.textlength(t['text'], font=font)
            tx = L if t['hAlign'] == 'Left' else (L + W - tw if t['hAlign'] == 'Right' else L + (W - tw) / 2)
            draw.text((tx, top + (H - t['size'] * own[2]) / 2 - t['size'] * own[2] * 0.15), t['text'], font=font, fill=tuple(n['color']))
    canvas.save(out_path)
    print('RENDERED', out_path)


if __name__ == '__main__':
    args = sys.argv[1:]
    main(args[0], args[1], *(int(v) for v in args[2:4]))
