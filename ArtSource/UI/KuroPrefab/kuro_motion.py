"""原作 UI 节点上的动效组件 → 导出数据（kuro_export 调用）。

  - UIEffectTextAnimation（逐字动画，LGUI3 有同名组件）→ node['textAnimations']：选择器 + 属性，组件名保留（补间按名字找）
  - UIEffectOutline（描边效果组件）→ node['outlineEffect']
  - LGUIPlayTweenComponent（补间，原作 PlayTweenAnim 播放一个节点上的全部补间）→ node['tweens']，由 UWuwaUiTweenGroup 播放
用到的曲线资产（CurveFloat）用 PakExport 导出关键帧，放进布局的 curves，由 WuwaKuroHudBuild 建成项目里的 CurveFloat。
原作字段没存的是默认值（Kuro 的 LGUI 与 LGUI3 同源）：补间 duration 0.5、startDelay 0、Linear、Once；Float 0→1；Vector3 0→1。
"""
from __future__ import annotations

import json
import struct

import kuro_prefab as kp

CURVE_ROOT = '/Game/LGUI/HUD/Kuro/Curves'
# 原作补间事件调用的函数 → 项目 UWuwaUiTweenGroup 的动作
DROPPED: list[str] = []
ACTIONS = {'SetSelectorOffset': 'SelectorOffset', 'SetAlpha': 'Alpha', 'SetUIItemAlpha': 'Alpha', 'SetUIItemScale': 'Scale',
           'SetUIRelativeScale3D': 'Scale', 'SetRelativeScale3D': 'Scale', 'SetIsUIActive': 'Active', 'SetUIActive': 'Active', 'SetAnchorOffsetX': 'AnchorOffsetX',
           'SetAnchorOffsetY': 'AnchorOffsetY', 'SetColor': 'Color'}
# 设计分辨率单位的动作（位置），值按画布比例换算
SCALED = {'AnchorOffsetX', 'AnchorOffsetY'}
# 补间类 → 值的分量数
KINDS = {'Float': 1, 'Vector2': 2, 'Vector3': 3, 'Color': 4, 'LinearColor': 4}


class Curves:
    """原作 CurveFloat 资产 → 项目资产路径 + 关键帧。"""

    def __init__(self):
        self.items: dict[str, dict] = {}

    def add(self, ref) -> str | None:
        if not isinstance(ref, dict) or not ref.get('asset'):
            return None
        package = ref['asset']                       # Client/Content/Aki/UI/UIResources/UiFight/Curve/<目录>/<名字>
        rel = package.split('/Curve/', 1)[1] if '/Curve/' in package else package.rsplit('/', 1)[1]
        path = f'{CURVE_ROOT}/{rel}'
        if path not in self.items:
            data = json.loads(kp.package_json(package).read_text(encoding='utf-8'))
            curve = next(o for o in data if o.get('Type') == 'CurveFloat')['Properties'].get('FloatCurve', {})
            keys = [{'time': k.get('Time', 0.0), 'value': k.get('Value', 0.0), 'interp': k.get('InterpMode', 'RCIM_Linear'),
                     'tangentMode': k.get('TangentMode', 'RCTM_Auto'), 'arrive': k.get('ArriveTangent', 0.0), 'leave': k.get('LeaveTangent', 0.0)}
                    for k in curve.get('Keys', [])]
            self.items[path] = {'path': path, 'keys': keys,
                                'preInfinity': curve.get('PreInfinityExtrap', 'RCCE_Constant'), 'postInfinity': curve.get('PostInfinityExtrap', 'RCCE_Constant')}
        return path


def _class(v) -> str:
    return (v or {}).get('class', '') if isinstance(v, dict) else ''


def _vec(v, default=(0.0, 0.0, 0.0)):
    if not isinstance(v, dict):
        return list(default)
    return [v.get('X', default[0]), v.get('Y', default[1]), v.get('Z', default[2])]


def _ease(v, default='Linear') -> str:
    return (v or f'::{default}').split('::')[1]


def text_animations(components, curves: Curves, k: float = 1.0) -> list[dict]:
    out = []
    for cls, props in components:
        if cls != 'UIEffectTextAnimation':
            continue
        sel = props.get('selector') or {}
        sp = sel.get('props', {}) if isinstance(sel, dict) else {}
        anim = {'name': props.get('_name', ''),
                'selector': {'class': _class(sel.get('class')), 'range': sp.get('range', 0.1), 'offset': sp.get('offset', 0.5),
                             'flipDirection': bool(sp.get('flipDirection', False)), 'start': sp.get('start', 0.0), 'end': sp.get('end', 1.0)},
                'properties': []}
        for prop in props.get('properties', []) or []:
            pp = prop.get('props', {})
            item = {'class': _class(prop.get('class')), 'easeType': _ease(pp.get('easeType'), 'InOutSine')}
            curve = curves.add(pp.get('easeCurve'))
            if curve:
                item['easeCurve'] = curve
            for key in ('position', 'scale', 'rotator'):
                if key in pp:
                    item[key] = _vec(pp[key])
            if 'position' in item:                     # 逐字位移是设计分辨率像素，按画布比例换算
                item['position'] = [v * k for v in item['position']]
            for key in ('alpha',):
                if key in pp:
                    item[key] = pp[key]
            if 'color' in pp:
                c = pp['color']
                item['color'] = [c.get(k, 255) for k in 'RGBA']
            anim['properties'].append(item)
        out.append(anim)
    return out


def outline_effect(components) -> dict | None:
    for cls, props in components:
        if cls == 'UIEffectOutline':
            c = props.get('outlineColor', {'R': 0, 'G': 0, 'B': 0, 'A': 128})
            size = props.get('outlineSize', {'X': 1.0, 'Y': 1.0})
            return {'color': [c.get(k, 0) for k in 'RGBA'], 'size': [size.get('X', 1.0), size.get('Y', 1.0)]}
    return None


def _events(delegate, file_to_path, k: float) -> list[dict]:
    out = []
    for e in (delegate or {}).get('eventList', []) if isinstance(delegate, dict) else []:
        fn = e.get('functionName', '')
        if fn not in ACTIONS:
            raise ValueError(f'unsupported tween event function {fn}')
        target = (e.get('TargetActor') or {}).get('file')
        if target not in file_to_path:
            # 目标节点没导出（跳过的 UINiagara 这类）：这条事件不要
            DROPPED.append(f"{fn} on {e.get('ComponentName', '')} (actor {target})")
            continue
        raw = bytes(e.get('ParamBuffer') or [])
        fixed: list[float] = [0.0, 0.0, 0.0, 0.0]
        if len(raw) == 1:
            fixed[0] = float(raw[0])                                  # bool
        elif len(raw) == 4 and ACTIONS[fn] == 'Color':
            fixed = [raw[2] / 255, raw[1] / 255, raw[0] / 255, raw[3] / 255]   # FColor 存 BGRA
        elif len(raw) % 4 == 0 and raw:
            vals = struct.unpack(f'<{len(raw) // 4}f', raw)
            fixed[:len(vals)] = vals
        action = ACTIONS[fn]
        scale = k if action in SCALED else 1.0
        out.append({'target': file_to_path[target], 'component': e.get('ComponentName', ''), 'action': action,
                    'useTweenValue': bool(e.get('UseNativeParameter', False)), 'fixed': [v * scale for v in fixed], 'valueScale': scale})
    return out


def _value(v, n: int, default: float) -> list[float]:
    out = [default] * 4
    if isinstance(v, (int, float)):
        out[0] = float(v)
    elif isinstance(v, dict):
        if 'R' in v:
            scale = 255.0 if all(isinstance(v.get(c, 0), int) for c in 'RGBA') else 1.0
            out = [v.get(c, 255) / scale for c in 'RGBA']
        else:
            for i, c in enumerate('XYZW'[:n]):
                out[i] = v.get(c, default)
    return out


def tweens(components, file_to_path: dict, curves: Curves, k: float = 1.0) -> list[dict]:
    out = []
    for cls, props in components:
        if cls != 'LGUIPlayTweenComponent':
            continue
        t = props.get('playTween') or {}
        tp = t.get('props', {})
        kind = _class(t.get('class')).replace('LGUIPlayTween_', '')
        if kind not in KINDS:
            raise ValueError(f'unsupported tween class {kind}')
        n = KINDS[kind]
        color = kind in ('Color', 'LinearColor')
        tween = {'kind': kind,
                 'from': _value(tp.get('from'), n, 1.0 if color else 0.0) if 'from' in tp else ([1.0] * 4 if color else [0.0] * 4),
                 'to': _value(tp.get('to'), n, 1.0) if 'to' in tp else [1.0] * 4,
                 'easeType': _ease(tp.get('easeType')), 'duration': tp.get('duration', 0.5), 'startDelay': tp.get('startDelay', 0.0),
                 'loopType': _ease(tp.get('loopType'), 'Once'), 'loopCount': tp.get('loopCount', -1),
                 'onUpdate': _events(tp.get('onUpdateValue'), file_to_path, k), 'onStart': _events(tp.get('onStart'), file_to_path, k),
                 'onComplete': _events(tp.get('onComplete'), file_to_path, k), 'name': props.get('_name', '')}
        curve = curves.add(tp.get('easeCurve'))
        if curve and tween['easeType'] == 'CurveFloat':
            tween['easeCurve'] = curve
        out.append(tween)
    return out
