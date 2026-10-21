"""Extract a character's attack-effect evidence from cooked exports (Jiabei 嘉贝莉娜 by default; see characters.py).

Usage: python -I prepare_evidence.py [--character jiabei|changli]

Writes, in the same layout ChangliSlash/prepare_attack.py consumes:
- Evidence/AttackFxReference.json: every AnimNotifyEffect / AnimNotifyStateEffect of each stage
  (time, effect group, socket, location / rotation / scale, Attached), plus each group's children;
- Evidence/LayerParameters_<Stage>.json: emitter layers, renderers, materials and curve LUTs of the Niagara
  systems the stage uses (via ChangliSlash/Evidence/extract_system_layers.py, pointed at the character's folders).
Evidence/ is the character's evidence folder (Jiabei: Evidence/, others: <Character>/Evidence/).

The project montages play the 30 fps sequences at their original length, so timelineScale = 1.
Never modifies source exports.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'ChangliSlash' / 'Evidence'))
import extract_system_layers as layers  # noqa: E402
sys.path.insert(0, str(HERE))
import characters  # noqa: E402

CHARACTER, ARGS = characters.select()
EXPORTS = Path('C:/GamePakExtractor/Output/Exports')
ROLE = CHARACTER['role']
layers.NIAGARA = CHARACTER['niagara']
layers.DATA_ASSETS = CHARACTER['dataAssets']
layers.OUT = CHARACTER['evidence']
EVIDENCE = CHARACTER['evidence']

# 段 -> 原作来源（序列 / 蒙太奇 JSON）与项目蒙太奇
STAGES = CHARACTER['stages']
EFFECT_NOTIFY = re.compile(r'AnimNotify(State)?Effect')


def load(package: str) -> list[dict]:
    path = EXPORTS / (re.sub(r'\.\d+$', '', package) + '.json')
    if not path.exists():
        return []
    data = json.loads(path.read_text(encoding='utf-8-sig'))
    return data if isinstance(data, list) else [data]


def object_by_ref(objects: list[dict], ref: dict | None) -> dict | None:
    if not ref:
        return None
    name = ref['ObjectName'].split(':')[-1].rstrip("'")
    return next((o for o in objects if o.get('Name') == name), None)


def asset_path(ref) -> str | None:
    if isinstance(ref, dict):
        return ref.get('AssetPathName') or ref.get('ObjectPath')
    return ref


def to_package(path: str) -> str:
    """'/Game/Aki/X/Y.Y' or 'Client/Content/Aki/X/Y.0' -> 'Client/Content/Aki/X/Y'"""
    path = path.replace('/Game/', 'Client/Content/', 1) if path.startswith('/Game/') else path
    return re.sub(r'\.[A-Za-z0-9_]+$', '', path)


def group_children(group_package: str, seen: set[str] | None = None) -> list[dict]:
    """Children of an effect group: model class, data asset and delay (nested groups flattened)."""
    seen = seen or set()
    if group_package in seen:
        return []
    seen.add(group_package)
    objects = load(group_package)
    if not objects:
        return [{'missing': group_package}]
    out = []
    for entry in objects[0].get('Properties', {}).get('EffectData', []):
        key = entry['Key']
        model_class = key.split("'", 1)[0]
        package = to_package(key.split("'", 2)[1])
        delay = float(entry.get('Value', 0.0))
        if 'Group' in model_class:
            for child in group_children(package, seen):
                out.append({**child, 'delay': child.get('delay', 0.0) + delay})
            continue
        child = {'modelClass': model_class, 'dataAsset': package, 'delay': delay}
        if 'Niagara' in model_class:
            da = load(package)
            ref = da[0].get('Properties', {}).get('NiagaraRef') if da else None
            child['niagara'] = to_package(ref['ObjectPath']) if ref else None
        out.append(child)
    return out


def events_of(source: str) -> tuple[list[dict], dict]:
    objects = load(ROLE + source)
    owner = next((o for o in objects if o.get('Type') in ('AnimSequence', 'AnimMontage')), None)
    if owner is None:
        raise FileNotFoundError(ROLE + source)
    props = owner['Properties']
    info = {'source': source, 'type': owner['Type'], 'file': str(EXPORTS / (ROLE + source + '.json')),
            'lengthSeconds': props.get('SequenceLength')}
    events = []
    for notify in props.get('Notifies', []):
        ref = notify.get('Notify') or notify.get('NotifyStateClass')
        if not ref or not EFFECT_NOTIFY.search(ref['ObjectName']):
            continue
        obj = object_by_ref(objects, ref) or {}
        p = obj.get('Properties', {})
        effect = asset_path(p.get('EffectDataAssetRef'))
        state = notify.get('NotifyStateClass') is not None
        chance = notify.get('NotifyTriggerChance', 1.0)
        enabled = notify.get('bEnabled', True)
        events.append({
            'notifyObject': obj.get('Name'), 'notifyClass': obj.get('Type'), 'owner': source,
            'state': state,
            'authoredSeconds': notify.get('LinkValue', 0.0),
            'durationSeconds': notify.get('Duration', 0.0),
            'triggerOffsetSeconds': notify.get('TriggerTimeOffset', 0.0),
            'enabled': enabled, 'triggerChance': chance,
            # 原作 TriggerChance 为 0 的通知不会触发
            'included': bool(enabled and chance > 0 and effect),
            'effectAsset': effect,
            'socketName': p.get('SocketName') or 'Root',
            'location': p.get('Location'), 'rotation': p.get('Rotation'), 'scale': p.get('Scale'),
            'attachedExplicit': p.get('Attached'),
            'detachWhenSkillEndExplicit': p.get('DetachWhenSkillEnd'),
        })
    return events, info


def bullet_event(effect: str) -> dict:
    """子弹特效DA 当作 0 秒处、不跟随的一个特效通知（原作子弹在自己位置播放；特效组自己的 Scale 常量当作通知缩放）。"""
    objects = load(to_package(effect))
    scale = ((objects[0].get('Properties') or {}).get('Scale') or {}).get('Constant') if objects else None
    return {'notifyObject': to_package(effect).rsplit('/', 1)[1], 'notifyClass': 'BulletEffect', 'owner': 'Bullet', 'state': False,
            'authoredSeconds': 0.0, 'durationSeconds': 0.0, 'triggerOffsetSeconds': 0.0, 'enabled': True, 'triggerChance': 1.0,
            'included': True, 'effectAsset': effect, 'socketName': 'Root', 'location': None, 'rotation': None, 'scale': scale,
            'attachedExplicit': False, 'detachWhenSkillEndExplicit': None}


def main() -> int:
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    attacks = []
    for stage, config in STAGES.items():
        sources, all_events = [], []
        for source in config['sources']:
            events, info = events_of(source)
            sources.append(info)
            all_events += events
        all_events += [bullet_event(e) for e in CHARACTER.get('bulletEffects', {}).get(stage, [])]
        groups = {}
        for event in all_events:
            if event['included']:
                package = to_package(event['effectAsset'])
                groups.setdefault(package.rsplit('/', 1)[1], group_children(package))
        # 每个 Niagara 子特效：NS 名 -> DA 名
        pairs = {}
        for children in groups.values():
            for child in children:
                if child.get('niagara'):
                    # 层参数提取只认角色自己的 Niagara 目录；公共目录的（如 NI_Common 的 NS_Fx_Move）先跳过并列出
                    if not child['niagara'].startswith(CHARACTER['niagara'].rstrip('/')):
                        print(f"  {stage}: skipping {child['niagara']} (outside {CHARACTER['niagara']})")
                        continue
                    # 角色目录下的子目录（如 Shouji/）保留相对路径，层参数提取按 角色目录 + 名字 找导出
                    pairs[child['niagara'].removeprefix(CHARACTER['niagara'])] = child['dataAsset'].removeprefix(CHARACTER['dataAssets'])
        # 镜头序列里直接放的 Niagara：没有 DA
        for ns in CHARACTER.get('sequenceSystems', {}).get(stage, []):
            pairs.setdefault(ns, '')
        if pairs:
            layers.main(f'LayerParameters_{stage}.json', [f'{ns}:{da}' for ns, da in sorted(pairs.items())])
        attacks.append({
            'projectAttack': stage, 'projectMontage': config['montage'], 'sources': sources,
            'timelineScale': 1.0,
            'sourceEvents': [e for e in all_events if not e['state']],
            'sourceSequenceEffects': [dict(e, authoredBeginSeconds=e['authoredSeconds']) for e in all_events if e['state']],
            'groups': groups,
            'sequenceSystems': CHARACTER.get('sequenceSystems', {}).get(stage, []),
            'bulletEffects': CHARACTER.get('bulletEffects', {}).get(stage, []),
        })
        unsupported = sorted({c['modelClass'] for cs in groups.values() for c in cs if 'niagara' not in c and 'missing' not in c})
        print(f"{stage}: events={sum(e['included'] for e in all_events)}/{len(all_events)} groups={len(groups)} "
              f"niagara={len(pairs)} otherModels={unsupported}")
    reference = {
        'scope': f"{CHARACTER['title']} attack effects (AnimNotifyEffect / AnimNotifyStateEffect) from cooked sequence and montage JSON.",
        'generator': Path(__file__).name, 'attacks': attacks,
    }
    (EVIDENCE / 'AttackFxReference.json').write_text(json.dumps(reference, ensure_ascii=False, indent=1), encoding='utf-8')
    return 0


if __name__ == '__main__':
    sys.exit(main())
