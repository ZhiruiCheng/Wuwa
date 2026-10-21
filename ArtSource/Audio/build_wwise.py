"""按 prepare_audio.py 的配方，在项目 Wwise 工程里同名重建原作事件（WAAPI，HTTP）。
  python build_wwise.py --character changli [--generate]

需要 WAAPI：开着 Wwise 编辑器（User Preferences 里启用 WAAPI，HTTP 端口 8090），或者在命令行启动
  WwiseConsole.exe waapi-server <Wuwa_WwiseProject.wproj> --http-port 8090
重跑会删掉 \\Containers\\Default Work Unit\\Wuwa_<角色> 和同名事件后重建。

层级（与原作 HIRC 一致，对象名带原作 ID 便于对照）：
  Containers\\Default Work Unit\\Wuwa_<角色>（只用来归类，无属性）
    └ 原作根 Actor-Mixer（PropertyContainer，3D 定位 + 衰减）→ … → 随机容器 → Sound
  Events\\Default Work Unit\\Wuwa_<角色>\\<原作事件名>：Play（延迟、概率）/ Stop（本对象、淡出）
语言 Switch 与默认值 Switch 已在配方里折叠；折叠节点的音量类属性叠加到保留的子节点上。
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import urllib.error
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
WAAPI = 'http://127.0.0.1:8090/waapi'
WWISE_CONSOLE = Path('C:/Audiokinetic/Wwise_2025.1.10.9233/Authoring/x64/Release/bin/WwiseConsole.exe')
PROJECT = HERE.parents[1] / 'Wuwa_WwiseProject' / 'Wuwa_WwiseProject.wproj'

# HIRC 属性 → Wwise 属性；叠加类属性（dB / 音分 / 滤波）在折叠时累加
PROPS = {'Volume': 'Volume', 'MakeUpGain': 'MakeUpGain', 'Pitch': 'Pitch', 'LPF': 'Lowpass', 'HPF': 'Highpass',
         'Positioning_CenterPercent': 'CenterPercentage'}
ADDITIVE = {'Volume', 'MakeUpGain', 'Pitch', 'LPF', 'HPF'}
KIND_TYPE = {'ActorMixer': 'PropertyContainer', 'RandomSequenceContainer': 'RandomSequenceContainer', 'Sound': 'Sound',
             'BlendContainer': 'BlendContainer', 'SwitchContainer': 'SwitchContainer'}
# 衰减曲线：curveToUse 下标 → WAAPI 曲线类型（只照抄能确定含义的：0~2 音量，3 低通）
CURVE_SLOTS = {0: 'VolumeDryUsage', 3: 'LowPassFilterUsage'}
SHAPES = {'Linear': 'Linear', 'Constant': 'Constant', 'Log3': 'Log3', 'Log1': 'Log1', 'Exp1': 'Exp1', 'Exp3': 'Exp3',
          'SCurve': 'SCurve', 'InvSCurve': 'InvSCurve', 'Sine': 'Sine', 'SineRecip': 'SineRecip'}


class Waapi:
    def call(self, uri: str, args: dict | None = None, options: dict | None = None) -> dict:
        body = json.dumps({'uri': uri, 'args': args or {}, 'options': options or {}}).encode()
        request = urllib.request.Request(WAAPI, body, {'Content-Type': 'application/json'})
        try:
            return json.loads(urllib.request.urlopen(request, timeout=300).read())
        except urllib.error.HTTPError as e:
            raise RuntimeError(f'{uri}: {e.read().decode()}') from None

    def find(self, path: str) -> dict | None:
        try:
            r = self.call('ak.wwise.core.object.get', {'waql': f'$ "{path}"'}, {'return': ['id', 'name', 'type']})
        except RuntimeError:
            return None
        return (r.get('return') or [None])[0]


def common_name(names: list[str]) -> str:
    prefix = os.path.commonprefix(names).rstrip('_0123456789')
    return prefix or names[0]


def build(character: str, generate: bool) -> int:
    recipe = json.loads((HERE / character.capitalize() / 'audio.json').read_text(encoding='utf-8'))
    nodes, events = recipe['nodes'], recipe['events']
    w = Waapi()
    w.call('ak.wwise.core.getInfo')

    def resolve(nid: str, acc: dict | None = None) -> tuple[str, dict]:
        """折叠节点 → 保留的子节点，并累加折叠节点的属性。"""
        acc = dict(acc or {})
        n = nodes[nid]
        while n['kind'] == 'Collapsed':
            for k, v in n.get('props', {}).items():
                acc[k] = acc.get(k, 0.0) + v if k in ADDITIVE else v
            nid = n['children'][0]
            n = nodes[nid]
        return nid, acc

    # 父子关系（折叠节点的子节点挂到折叠节点的父节点下）
    children: dict[str | None, list[tuple[str, dict]]] = {}
    for nid, n in nodes.items():
        if n['kind'] in ('Attenuation', 'Collapsed'):
            continue
        parent = n.get('parent')
        inherited = {}
        while parent and nodes.get(parent, {}).get('kind') == 'Collapsed':
            for k, v in nodes[parent].get('props', {}).items():
                inherited[k] = inherited.get(k, 0.0) + v if k in ADDITIVE else v
            parent = nodes[parent].get('parent')
        children.setdefault(parent, []).append((nid, inherited))

    root_name = f'Wuwa_{character.capitalize()}'
    container_root = f'\\Containers\\Default Work Unit\\{root_name}'
    event_root = f'\\Events\\Default Work Unit\\{root_name}'
    for path in (container_root, event_root):
        if w.find(path):
            w.call('ak.wwise.core.object.delete', {'object': path})
    w.call('ak.wwise.core.object.create', {'parent': '\\Containers\\Default Work Unit', 'type': 'Folder', 'name': root_name})
    w.call('ak.wwise.core.object.create', {'parent': '\\Events\\Default Work Unit', 'type': 'Folder', 'name': root_name})

    # 衰减
    attenuations = {}
    for nid, n in nodes.items():
        if n['kind'] != 'Attenuation':
            continue
        name = f'{root_name}_Attenuation_{nid}'
        path = f'\\Attenuations\\Default Work Unit\\{name}'
        if w.find(path):
            w.call('ak.wwise.core.object.delete', {'object': path})
        obj = w.call('ak.wwise.core.object.create', {'parent': '\\Attenuations\\Default Work Unit', 'type': 'Attenuation', 'name': name})
        radius = max((p[0] for c in n['curves'] for p in c['points']), default=0.0)
        w.call('ak.wwise.core.object.setProperty', {'object': obj['id'], 'property': 'RadiusMax', 'value': radius})
        for slot, curve_type in CURVE_SLOTS.items():
            index = n['curveToUse'][slot]
            if index < 0:
                continue
            points = [{'x': x, 'y': y, 'shape': SHAPES.get(shape, 'Linear')} for x, y, shape in n['curves'][index]['points']]
            w.call('ak.wwise.core.object.setAttenuationCurve', {'object': obj['id'], 'curveType': curve_type, 'use': 'Custom', 'points': points})
        for aux in ('VolumeWetGameUsage', 'VolumeWetUserUsage'):
            if n['curveToUse'][1 if aux == 'VolumeWetGameUsage' else 2] == n['curveToUse'][0]:
                w.call('ak.wwise.core.object.setAttenuationCurve', {'object': obj['id'], 'curveType': aux, 'use': 'UseVolumeDry', 'points': []})
        attenuations[nid] = obj['id']

    created: dict[str, str] = {}
    imports = []

    def name_of(nid: str) -> str:
        n = nodes[nid]
        if n['kind'] == 'Sound':
            return n['name']
        sounds = [nodes[s]['name'] for s in sound_names(nid)]
        label = common_name(sounds) if sounds else n['kind']
        return f'{label}_{nid}' if n['kind'] != 'ActorMixer' else f'AM_{nid}'

    def sound_names(nid: str) -> list[str]:
        n = nodes[nid]
        if n['kind'] == 'Sound':
            return [nid]
        return [s for c in n.get('children', []) for s in sound_names(resolve(c)[0])]

    def apply_props(obj_id: str, n: dict, extra: dict) -> None:
        props = dict(n.get('props', {}))
        for k, v in extra.items():
            props[k] = props.get(k, 0.0) + v if k in ADDITIVE else v
        for k, v in props.items():
            if k in PROPS:
                w.call('ak.wwise.core.object.setProperty', {'object': obj_id, 'property': PROPS[k], 'value': v})
            elif k == 'Priority':
                w.call('ak.wwise.core.object.setProperty', {'object': obj_id, 'property': 'OverridePriority', 'value': True})
                w.call('ak.wwise.core.object.setProperty', {'object': obj_id, 'property': 'Priority', 'value': int(v)})
        for k, (lo, hi) in n.get('ranged', {}).items():
            if k in PROPS:
                w.call('ak.wwise.core.object.setRandomizer', {'object': obj_id, 'property': PROPS[k], 'enabled': True, 'min': lo, 'max': hi})
        pos = n.get('positioning', {})
        if pos.get('bPositioningInfoOverrideParent') == '1':
            for prop, value in (('OverridePositioning', True), ('ListenerRelativeRouting', pos.get('bHasListenerRelativeRouting') == '1'),
                                ('3DSpatialization', int(pos.get('eSpatializationMode', '0'))), ('3DPosition', int(pos.get('e3DPositionType', '0'))),
                                ('EnableAttenuation', pos.get('bEnableAttenuation') == '1')):
                w.call('ak.wwise.core.object.setProperty', {'object': obj_id, 'property': prop, 'value': value})
        if n.get('attenuation') in attenuations:
            w.call('ak.wwise.core.object.setReference', {'object': obj_id, 'reference': 'Attenuation', 'value': attenuations[n['attenuation']]})
        c = n.get('container')
        if c:
            settings = {'RandomOrSequence': 1 if c['mode'] == 'Random' else 0,
                        'NormalOrShuffle': 0 if c['randomMode'] == 'Shuffle' else 1,
                        'RandomAvoidRepeating': c['avoidRepeatCount'] > 0,
                        'RandomAvoidRepeatingCount': max(1, c['avoidRepeatCount']),
                        'PlayMechanismStepOrContinuous': 0 if c['continuous'] else 1,
                        'PlayMechanismResetPlaylistEachPlay': c['resetPlaylistEachPlay'],
                        'GlobalOrPerObject': 1}
            for prop, value in settings.items():
                w.call('ak.wwise.core.object.setProperty', {'object': obj_id, 'property': prop, 'value': value})
        if n['kind'] == 'Sound':
            w.call('ak.wwise.core.object.setProperty', {'object': obj_id, 'property': 'IsStreamingEnabled', 'value': bool(n.get('streaming'))})
            if n.get('loop') is not None:
                w.call('ak.wwise.core.object.setProperty', {'object': obj_id, 'property': 'IsLoopingEnabled', 'value': True})
                w.call('ak.wwise.core.object.setProperty', {'object': obj_id, 'property': 'IsLoopingInfinite', 'value': int(n['loop']) == 0})

    def create(nid: str, parent_path: str, extra: dict) -> None:
        n = nodes[nid]
        name = name_of(nid)
        path = f'{parent_path}\\{name}'
        if n['kind'] == 'Sound':
            r = w.call('ak.wwise.core.audio.import', {
                'importOperation': 'replaceExisting',
                'default': {'importLanguage': 'SFX'},
                'imports': [{'audioFile': n['wav'], 'objectPath': f'{parent_path}\\<Sound SFX>{name}',
                             'originalsSubFolder': root_name}]}, {'return': ['id']})
            obj_id = r['objects'][-1]['id']
        else:
            obj_id = w.call('ak.wwise.core.object.create', {'parent': parent_path, 'type': KIND_TYPE[n['kind']], 'name': name})['id']
        created[nid] = obj_id
        apply_props(obj_id, n, extra)
        for child, inherited in children.get(nid, []):
            create(child, path, inherited)

    def switch_group(name: str, values: list[str]) -> dict:
        """Switches\\Default Work Unit\\<组>（多个角色共用，已有就补缺的值），返回 {值: 对象 ID}。"""
        path = f'\\Switches\\Default Work Unit\\{name}'
        group = w.find(path) or w.call('ak.wwise.core.object.create', {'parent': '\\Switches\\Default Work Unit', 'type': 'SwitchGroup', 'name': name})
        existing = {o['name']: o['id'] for o in w.call('ak.wwise.core.object.get', {'waql': f'$ "{group["id"]}" select children'},
                                                        {'return': ['id', 'name']})['return']}
        for value in values:
            if value not in existing:
                existing[value] = w.call('ak.wwise.core.object.create', {'parent': group['id'], 'type': 'Switch', 'name': value})['id']
        return {'group': group['id'], **existing}

    for nid, inherited in children.get(None, []):
        create(nid, container_root, inherited)

    # Switch 容器：关联开关组、默认值，按原作分配表把子节点分到各开关值
    for nid, n in nodes.items():
        if n['kind'] != 'SwitchContainer' or nid not in created:
            continue
        sw = n['switch']
        values = sorted(set(sw['assignments']) | ({sw['defaultName']} if sw.get('defaultName') else set()))
        ids = switch_group(sw['groupName'], values)
        w.call('ak.wwise.core.object.setReference', {'object': created[nid], 'reference': 'SwitchGroupOrStateGroup', 'value': ids['group']})
        if sw.get('defaultName'):
            w.call('ak.wwise.core.object.setReference', {'object': created[nid], 'reference': 'DefaultSwitchOrState', 'value': ids[sw['defaultName']]})
        for value, targets in sw['assignments'].items():
            for target in targets:
                child, _ = resolve(target)
                w.call('ak.wwise.core.switchContainer.addAssignment', {'child': created[child], 'stateOrSwitch': ids[value]})

    for event, data in events.items():
        actions = []
        for a in data['actions']:
            target, _ = resolve(a['target'])
            if target not in created:
                raise RuntimeError(f'{event}: action target {a["target"]} was not built')
            action = {'type': 'Action', 'name': '', '@Target': created[target]}
            if a['type'] == 'Play':
                action['@ActionType'] = 1
                if 'Probability' in a['props']:
                    action['@Probability'] = a['props']['Probability']
            elif a['type'] == 'Stop_E_O':
                # Stop_E_O：停止本游戏对象上的目标（原作淡出时长单位毫秒）
                action.update({'@ActionType': 2, '@Scope': 0, '@FadeTime': a['props'].get('TransitionTime', 0.0) / 1000.0,
                               '@FadeOutCurve': int(a.get('fadeCurve') or 4)})
            else:
                continue
            if 'DelayTime' in a['props']:
                action['@Delay'] = a['props']['DelayTime'] / 1000.0
            actions.append(action)
        # Action 没有名字，一次 create 带多个同名（空名）子对象会被拒绝，逐个加
        event_id = w.call('ak.wwise.core.object.create', {'parent': event_root, 'type': 'Event', 'name': event})['id']
        for action in actions:
            w.call('ak.wwise.core.object.create', {'parent': event_id, **action})

    w.call('ak.wwise.core.project.save')
    print(json.dumps({'containers': len(created), 'events': len(events), 'attenuations': len(attenuations)}))
    if generate:
        r = w.call('ak.wwise.core.soundbank.generate', {'soundbanks': [{'name': e} for e in events], 'writeToDisk': True,
                                                         'platforms': ['Windows']}, {})
        print('generated', len(r.get('logs', [])) if isinstance(r, dict) else r)
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--character', default='changli')
    parser.add_argument('--generate', action='store_true', help='generate the event SoundBanks after building')
    args = parser.parse_args()
    return build(args.character, args.generate)


if __name__ == '__main__':
    sys.exit(main())
