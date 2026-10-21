"""界面特效（原作 NS_Fx_LGUI_*）→ -run=WuwaUiEffectSetup 的配方。

  python -I prepare_ui_effects.py

原作这些特效由鸣潮自研 LGUI 的 UINiagara 在界面里播放；项目用 UWuwaUINiagara（Niagara CPU 模拟 → LGUI 网格）播放，
系统本身按原作脚本解码出的数值重建（ArtSource/JiabeiAttack/vm_decode.py 解原作烘焙进字节码的模块输入）。
目前是试点：NS_Fx_LGUI_FightSkill（技能按钮按下时的光圈，FightSkill 注册表 10 号）。

输出（原作资源，不提交）：ArtSource/UI/HUD/Kuro/Effects/{ui_effects.recipe.json, Textures/, Geometry/}
"""
from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
sys.path.insert(0, str(HERE))
import kuro_layout as kl  # noqa: E402

OUT = REPO / 'ArtSource/UI/HUD/Kuro/Effects'
EXPORTS = Path('C:/GamePakExtractor/Output/Exports')
BLENDER = Path('C:/Blender4.3.2/blender-4.3.2-windows-x64/blender.exe')
GEOMETRY_SCRIPT = REPO / 'ArtSource/JiabeiAttack/uemodel_to_geometry.py'


def niagara_json(package: str) -> list:
    path = EXPORTS / f'{package}.json'
    if not path.exists():
        subprocess.run([str(kl.kp.PAKEXPORT), 'json', f'{package}.uasset'], check=True, capture_output=True)
    return json.loads(path.read_text(encoding='utf-8'))


def color_curve_alpha(system: list) -> tuple[list[float], float, float]:
    """系统里第一条颜色曲线的 A 通道 LUT（值、起始时间、时间范围）。"""
    for e in system:
        if e['Type'] == 'NiagaraDataInterfaceColorCurve':
            p = e['Properties']
            return p['ShaderLUT'][3::4], p.get('LUTMinTime', 0.0), 1.0 / p['LUTInvTimeRange']
    raise SystemExit('no colour curve')


def simplify(points: list[tuple[float, float]], tolerance: float) -> list[tuple[float, float]]:
    """Douglas-Peucker：保留折线上偏差大于 tolerance 的点。"""
    if len(points) < 3:
        return points
    (x0, y0), (x1, y1) = points[0], points[-1]
    worst, index = 0.0, 0
    for i in range(1, len(points) - 1):
        x, y = points[i]
        expected = y0 + (y1 - y0) * (x - x0) / (x1 - x0)
        if abs(y - expected) > worst:
            worst, index = abs(y - expected), i
    if worst <= tolerance:
        return [points[0], points[-1]]
    return simplify(points[:index + 1], tolerance)[:-1] + simplify(points[index:], tolerance)


def piecewise_hlsl(points: list[tuple[float, float]], t: str) -> str:
    """分段线性曲线的 HLSL 表达式（嵌套三目）。"""
    expr = f'{points[-1][1]:.5f}'
    for (xa, ya), (xb, yb) in reversed(list(zip(points, points[1:]))):
        expr = f'({t} < {xb:.5f} ? lerp({ya:.5f}, {yb:.5f}, saturate(({t} - {xa:.5f}) / {xb - xa:.5f})) : {expr})'
    return expr


def export_mesh(package: str) -> str:
    """原作网格 → Geometry JSON（Blender + UEFormat，和攻击特效同一个转换脚本）。"""
    name = package.rsplit('/', 1)[1]
    target = OUT / 'Geometry' / f'{name}.json'
    if not target.exists():
        if not (EXPORTS / f'{package}.uemodel').exists():
            subprocess.run([str(kl.kp.PAKEXPORT), 'export', f'{package}.uasset'], check=True, capture_output=True)
        target.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([str(BLENDER), '-b', '--factory-startup', '--addons', 'bl_ext.user_default.io_scene_ueformat',
                        '--python', str(GEOMETRY_SCRIPT), '--', str(target.parent), package], check=True, capture_output=True)
    return name


def export_texture(package: str) -> dict:
    name = package.rsplit('/', 1)[1]
    target = OUT / 'Textures' / f'{name}.png'
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(kl.texture_png(package), target)
    return {'name': name, 'file': f'Textures/{name}.png', 'srgb': False}


def fight_skill() -> dict:
    """NS_Fx_LGUI_FightSkill：1 个网格发射器 Empty（vm_decode 解出的原作脚本）：
    出生  Lifetime = 0.35；Initial.Scale = 1.5 + random(0.1)；Position = 0
    更新  Scale = Initial.Scale × 曲线(0.98 → 1.2，线性)；Color = 颜色曲线(NormalizedAge)，A × 1.2 × AlphaScale(1)
    网格 Mod_Circle_010（圆环，内半径 22.41、外半径 30.2），材质 MI_UI_Liudong_011（叠加，T_UI_Mask_012 的 R 当遮罩）。
    """
    system = niagara_json('Client/Content/Aki/Effect/UI/Niagaras/Common/NS_Fx_LGUI_FightSkill')
    values, start, span = color_curve_alpha(system)
    points = simplify([(start + span * i / (len(values) - 1), v) for i, v in enumerate(values)], 0.01)
    alpha = piecewise_hlsl(points, 'Particles.NormalizedAge')
    # Initial.Scale 每个粒子在 1.5~1.6 随机；这个发射器只有一个粒子，取中值 1.55（差 3%，看不出来）
    return {
        'name': 'NS_UI_FightSkill',
        'source': 'NS_Fx_LGUI_FightSkill',
        'emitters': [{
            'name': 'Empty',
            'mesh': export_mesh('Client/Content/Aki/Effect/UI/Mesh/Mod_Circle_010'),
            'material': 'UI_Liudong_011',
            'spawnCount': 1, 'spawnTime': 0.0, 'loopDuration': 0.375, 'sortOrder': 0, 'randomSeed': 1,
            'spawn': [
                {'name': 'Particles.Lifetime', 'type': 'float', 'hlsl': '0.35'},
                {'name': 'Particles.Position', 'type': 'position', 'hlsl': 'float3(0, 0, 0)'},
                {'name': 'Particles.Scale', 'type': 'vec3', 'hlsl': 'float3(1.55, 1.55, 1.55) * 0.98'},
                {'name': 'Particles.Color', 'type': 'color', 'hlsl': 'float4(1, 1, 1, 0)'},
            ],
            'update': [
                {'name': 'Particles.Scale', 'type': 'vec3', 'hlsl': 'float3(1.55, 1.55, 1.55) * lerp(0.98, 1.2, Particles.NormalizedAge)'},
                {'name': 'Particles.Color', 'type': 'color', 'hlsl': f'float4(1, 1, 1, saturate({alpha} * 1.2))'},
            ],
        }],
    }


def main() -> int:
    sys.stdout.reconfigure(encoding='utf-8')
    OUT.mkdir(parents=True, exist_ok=True)
    recipe = {
        'folder': '/Game/LGUI/HUD/Kuro/Effects',
        'sourceRoot': OUT.relative_to(REPO).as_posix(),
        'textures': [export_texture('Client/Content/Aki/Effect/UI/Textures/Mask/T_UI_Mask_012')],
        # 原作 MI_UI_Liudong_011：MI_AdvacedEffect_UI 的实例，叠加、双面、Base = T_UI_Mask_012、Base_AlphaSwitch = R
        'materials': [{'id': 'UI_Liudong_011', 'texture': 'T_UI_Mask_012', 'blend': 'additive', 'channel': [1, 0, 0, 0]}],
        'systems': [fight_skill()],
    }
    recipe['meshes'] = sorted({e['mesh'] for s in recipe['systems'] for e in s['emitters'] if e.get('mesh')})
    path = OUT / 'ui_effects.recipe.json'
    path.write_text(json.dumps(recipe, ensure_ascii=False, indent=1), encoding='utf-8')
    print(path, [s['name'] for s in recipe['systems']])
    return 0


if __name__ == '__main__':
    sys.exit(main())
