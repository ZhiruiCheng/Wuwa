"""技能按钮按下时的点击圈（原作 FightSkill 注册表 10 号 UINiagara：NS_Fx_LGUI_FightSkill）→ 一张环形贴图。

  python -I make_click_ring.py

原作这个特效是一个网格粒子：网格 Mod_Circle_010 是圆环（内半径 22.41、外半径 30.2，UV 的 V 从内圈 0 到外圈 1、U 绕一圈），
材质 MI_UI_Liudong_011 用 T_UI_Mask_012 的 R 通道当透明度。LGUI3 不能在界面里放 Niagara，所以把"圆环网格 + 贴图"
按同样的 UV 映射烘成一张贴图，动画（0.35 秒，缩放 0.98→1.2、透明度曲线）由 UWuwaHudSkillBarPanel 用 LTween 播。

输出（原作资源导出的，不提交）：ArtSource/UI/HUD/Kuro/Textures/T_SkillClickRing.png
"""
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import kuro_export as ke  # noqa: E402
import kuro_layout as kl  # noqa: E402

INNER, OUTER = 22.41, 30.2   # Mod_Circle_010 的内外半径（导出的网格顶点）
SIZE = 256


def main() -> int:
    mask = Image.open(kl.texture_png('Client/Content/Aki/Effect/UI/Textures/Mask/T_UI_Mask_012')).convert('RGBA')
    column = [mask.getpixel((mask.width // 2, y))[0] for y in range(mask.height)]   # 贴图横向一致，取中间一列的 R
    out = Image.new('RGBA', (SIZE, SIZE), (255, 255, 255, 0))
    inner = INNER / OUTER
    c = (SIZE - 1) / 2
    for y in range(SIZE):
        for x in range(SIZE):
            r = ((x - c) ** 2 + (y - c) ** 2) ** 0.5 / (SIZE / 2)
            if inner <= r <= 1.0:
                v = (r - inner) / (1.0 - inner)                      # 内圈 V=0 → 贴图第一行，外圈 V=1 → 最后一行
                out.putpixel((x, y), (255, 255, 255, column[min(mask.height - 1, round(v * (mask.height - 1)))]))
    path = ke.OUT / 'Textures' / 'T_SkillClickRing.png'
    out.save(path)
    print(path)
    return 0


if __name__ == '__main__':
    sys.exit(main())
