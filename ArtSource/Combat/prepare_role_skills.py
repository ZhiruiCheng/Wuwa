"""角色技能的批量配方：原作技能表 + 蒙太奇 → -run=WuwaRoleSkillSetup 读的配方（不改原作数据）。
  python -I prepare_role_skills.py --character changli --skills 1205101,1205102
输出 <角色>.skills.json（原作数据，不提交）：
  anims     蒙太奇用到的项目动画序列 + 原作是否开根运动 / 锁根骨（开了的由命令行打开并挂 UWuwaPromoteRootMotionModifier）；
  montages  按原作重建：插槽、片段（序列 / 起止 / 速率）、分段、混合时间；
            技能流程通知（其余通知各有自己的流水线：子弹、位移、buff、特效、音效）：
              TsAnimNotifyEndSkill            → ReadyEnd（AnimNotify_SkillReadyEnd）
              TsAnimNotifyStateNextAtt        → 同级接招窗口（AnimNotifyState_SkillAcceptInput）
              TsAnimNotifyChangeSkillPriority → 改打断等级（WuwaAnimNotify_ChangeSkillPriority）
              EndSkill 之后到蒙太奇结尾        → 移动取消窗口（原作 CharacterMoveComponent：IsMainSkillReadyEnd 时移动输入 StopGroup1Skill「移动打断技能」）
  skills    技能表行：ID、名字、蒙太奇、GA（每个技能一个蓝图，父类按原作 SkillMode / SkillGA 选）、打断等级、WalkOffLedge、分类 Tag。
动画序列要先导入项目（Tools/Blender 流水线）。没导入的会列出来，命令行报错不建这个蒙太奇。
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
AKI = Path('C:/GamePakExtractor/Output/Exports/Client/Content/Aki')
PROJECT_CONTENT = Path('C:/UEProjects/Wuwa/Content')

CHARACTERS = {
    'changli': {
        'source': 'Character/Role/FemaleXL/ChangLi',
        'table': '/Game/Characters/Role/changli/Data/DT_SkillInfo_Changli',
        'montageFolder': '/Game/Characters/Role/changli/AnimMontage',
        'gaFolder': '/Game/Characters/Role/changli/GA',
        'montagePrefix': 'AM_',
        # 项目动画：Blender 流水线导出的 2.0 模型动画
        'animFolders': ['/Game/Characters/Role/changli/NewModel'],
        'animPrefix': 'changli2_0R2T1ChangLiMd10011_LOD0_Skeleton_',
        'roleTag': 'Skill.Role.Changli',
        # 每个技能的 GA 蓝图名（项目命名，和已有的 GA_Attack1~5 同风格）
        'gaNames': {
            1205101: 'GA_AirAttack1', 1205102: 'GA_AirAttack2', 1205103: 'GA_AirAttack3', 1205104: 'GA_AirAttack4',
            1205105: 'GA_AirAttackPlunge', 1205106: 'GA_AirAttack5',
            1205006: 'GA_AttackHeavy',
            1205201: 'GA_Skill01', 1205401: 'GA_Burst01', 1205402: 'GA_Burst02',
            100003: 'GA_AirDodgeF', 100004: 'GA_AirDodgeB',
        },
    },
    'jiabei': {
        'source': 'Character/Role/FemaleXL/Jiabeilina',
        'table': '/Game/Characters/Role/jiabei/Data/DT_SkillInfo_Jiabei',
        'montageFolder': '/Game/Characters/Role/jiabei/AnimMontage',
        'gaFolder': '/Game/Characters/Role/jiabei/GA',
        'montagePrefix': 'AM_Jiabei_',
        'animFolders': ['/Game/Characters/Role/jiabei/CommonAnim/Animations'],
        'animPrefix': 'jiabei2_0R2T1CalbrenaMd10011_LOD0_Skeleton_',
        'roleTag': 'Skill.Role.Jiabei',
        'gaNames': {100003: 'GA_AirDodgeF', 100004: 'GA_AirDodgeB',
                    1208016: 'GA_Heavy1', 1208017: 'GA_Heavy2', 1208018: 'GA_Heavy3',
                    1208008: 'GA_AirAttackPlunge',
                    # 变身（强化 E 1208101 开启）：E 跳踢 / 变身后跳踢、EX 普攻 1~4、EX 普攻 4 衔接版、爆蛇眼、EX 重击 1~3、变身下落攻击、大招
                    1208101: 'GA_SkillEnhanced', 1208007: 'GA_Skill01', 1208027: 'GA_Skill01Ex',
                    1208011: 'GA_AttackEx1', 1208012: 'GA_AttackEx2', 1208013: 'GA_AttackEx3', 1208014: 'GA_AttackEx4',
                    1208009: 'GA_AttackEx4H', 1208006: 'GA_AttackExFinisher',
                    1208020: 'GA_HeavyEx1', 1208021: 'GA_HeavyEx2', 1208022: 'GA_HeavyEx3',
                    1208028: 'GA_AirAttackPlungeEx', 1208301: 'GA_Burst01'},
    },
}

# 原作 SkillGA → 项目 GA 父类（原生类，按原作蓝图字节码移植）；SkillMode 0（没有专门 GA）用通用的播蒙太奇技能。
GA_PARENTS = {
    None: {'parent': '/Script/Wuwa.WuwaSkillMontageAbility'},
    # 起手 → 下落循环（SetForceSpeed 向下 5000）→ 落地；长离 1205106 只有起手一段，播完就结束
    'GA_Common_AirAttack': {'parent': '/Script/Wuwa.WuwaAirAttackAbility'},
    # 同上，只多一条：激活时在地面就结束（原作去播不存在的 Animations[3]）
    'GA_ChangLi_AirAttack': {'parent': '/Script/Wuwa.WuwaAirAttackAbility', 'properties': {'bEndIfActivatedOnGround': 'True'}},
    # 通用空中闪避 100003 / 100004：加闪避禁止 buff，至少 0.5 秒，落地 0.1 秒后 / 攀爬 / 蒙太奇结束时结束，结束时有移动输入就冲刺
    'GA_Role_LandEnd_Net': {'parent': '/Script/Wuwa.WuwaAirDodgeAbility'},
    # 长离大招：在空中先往下 10 米找地面、放到地面上，再播
    'GA_ChangLi_Burst': {'parent': '/Script/Wuwa.WuwaBurstAbility'},
    # 嘉贝空中下落攻击：同样是 起手 → 下落循环 → 落地段（Animations[0~2]）；原作空中按住普攻时改播转圈的 [3] Loop1，
    # 项目按用户要求简化掉（空中只按下出下落攻击），[3] / [4] 不播
    'GA_Common_AirAttack_jiabeilina': {'parent': '/Script/Wuwa.WuwaAirAttackAbility', 'montageLimit': 3},
    # 嘉贝 EX 普攻 / 强化 E / 大招：播 [0]，正常播完再放 1208999 变身待机（Stand3 姿势）；按用户要求简化掉待机技能，就是播蒙太奇
    'GA_AttackEx_To_Stand3': {'parent': '/Script/Wuwa.WuwaSkillMontageAbility'},
    # 嘉贝 E 跳踢：播 [0] 起跳踢；踢中（子弹命中给自己发 变招 事件）立刻接 [1] 后半段，没踢中 [0] 播完就结束
    'GA_Jump_MoveAttack_onlystandard': {'parent': '/Script/Wuwa.WuwaHitBranchAbility',
                                        'properties': {'BranchEventTag': '(TagName="Role.Jiabei.Event.SkillBranch")'}},
}

# 原作技能分类 Tag（最后一段）→ 项目技能分类 Tag
TAG_MAP = {'普攻': 'Skill.Type.NormalAttack', '闪避': 'Skill.Type.Dodge', '技能': 'Skill.Type.ResonanceSkill', '大招': 'Skill.Type.Burst'}

# 原作通用技能表（所有角色共用的闪避等）；用到的行放进各角色自己的技能表，蒙太奇按 MontagePaths 在角色 CommonAnim 里找
COMMON_TABLE = 'Data/Fight/DT_Common_Role_SkillInfo'

# 原作 SkillTag 里的动作标识（完整 Tag 名）→ 项目 Tag：技能期间加在自己身上（技能表 ActionTags），输入 / buff 要查的才映射
ACTION_TAG_MAP = {
    **{f'角色.R2T1ChangLiMd10011.动作标识.空中普攻{i}': f'Role.Changli.Action.AirAttack{i}' for i in range(1, 6)},
    '角色.R2T1ChangLiMd10011.动作标识.空中下落攻击': 'Role.Changli.Action.AirPlunge',
    '角色.R2T1ChangLiMd10011.动作标识.地面跳跃派生': 'Role.Changli.Action.GroundJumpDerive',
    # 嘉贝（珂莱塔）：原作 Tag 名被哈希（角色.<角色>.<动作标识>.<段>），按技能表里各招自带的 Tag 对上是哪一段
    '角色.2e985a12.51c4e7d2.9d400b39': 'Role.Jiabei.Action.Attack1',
    '角色.2e985a12.51c4e7d2.6123148d': 'Role.Jiabei.Action.Attack2',
    '角色.2e985a12.51c4e7d2.f051ffab': 'Role.Jiabei.Action.Attack3',
    '角色.2e985a12.51c4e7d2.ab7692f0': 'Role.Jiabei.Action.Attack4',
    '角色.2e985a12.13bc4761.a2722340': 'Role.Jiabei.Action.Heavy1',
    '角色.2e985a12.13bc4761.448e4d16': 'Role.Jiabei.Action.Heavy2',
    '角色.2e985a12.13bc4761.d9978178': 'Role.Jiabei.Action.Heavy3',
    # 嘉贝变身后的招式（EX 普攻 3 的标识不在技能表，在蒙太奇的 AddTag 窗口里，见 prepare_buffs TAG_MAP）
    '角色.2e985a12.51c4e7d2.3c569609': 'Role.Jiabei.Action.ExAttack1',
    '角色.2e985a12.51c4e7d2.8cb31693': 'Role.Jiabei.Action.ExAttack2',
    '角色.2e985a12.51c4e7d2.7d4fd618': 'Role.Jiabei.Action.ExAttack4',
    '角色.2e985a12.51c4e7d2.96a3d291': 'Role.Jiabei.Action.ExFinisher',
    '角色.2e985a12.13bc4761.7edd5204': 'Role.Jiabei.Action.ExHeavy1',
    '角色.2e985a12.13bc4761.5bc06377': 'Role.Jiabei.Action.ExHeavy2',
    '角色.2e985a12.13bc4761.24f80c87': 'Role.Jiabei.Action.ExHeavy3',
    '角色.2e985a12.64f2febe.ccff5050': 'Role.Jiabei.Action.EnhancedSkill',
}

FLOW_CLASSES = {'TsAnimNotifyEndSkill_C', 'TsAnimNotifyStateNextAtt_C', 'TsAnimNotifyChangeSkillPriority_C'}


def load(path: Path):
    return json.loads(path.read_text(encoding='utf-8'))


def strip(value):
    """DataTable 行的字段名带 GUID 后缀（SkillName_2_xxxx）。"""
    if isinstance(value, dict):
        return {re.sub(r'_\d+_[0-9A-F]{32}$', '', k): strip(v) for k, v in value.items()}
    if isinstance(value, list):
        return [strip(v) for v in value]
    return value


def project_anim(config: dict, sequence: str) -> str | None:
    for folder in config['animFolders']:
        path = f"{folder}/{config['animPrefix']}{sequence}"
        if (PROJECT_CONTENT / (path.removeprefix('/Game/') + '.uasset')).exists():
            return path
    return None


def montage(config: dict, original: str, anims: dict, missing: list) -> dict:
    # 大多在 CommonAnim 下，个别在子目录（嘉贝 E 跳踢在 CommonAnim/Jump_Attack）
    json_path = AKI / config['source'] / 'CommonAnim' / f'{original}.json'
    if not json_path.exists():
        json_path = next((AKI / config['source'] / 'CommonAnim').rglob(f'{original}.json'))
    data = load(json_path)
    m = next(o for o in data if o['Type'] == 'AnimMontage')
    p = m['Properties']
    objects = {o['Name']: o for o in data if 'Name' in o}
    tracks = []
    for track in p.get('SlotAnimTracks', []):
        segments = []
        for s in track['AnimTrack']['AnimSegments']:
            sequence = s['AnimReference']['ObjectName'].split("'")[1]
            path = project_anim(config, sequence)
            if path is None:
                missing.append(sequence)
                continue
            anims.setdefault(sequence, path)
            segments.append({'anim': path, 'start': s.get('AnimStartTime', 0.0), 'end': s.get('AnimEndTime', 0.0),
                             'rate': s.get('AnimPlayRate', 1.0), 'startPos': s.get('StartPos', 0.0)})
        tracks.append({'slot': track.get('SlotName', 'DefaultSlot'), 'segments': segments})
    length = p.get('SequenceLength', 0.0)
    flow = []
    for e in p.get('Notifies', []):
        ref = e.get('Notify') or e.get('NotifyStateClass') or {}
        cls = ref.get('ObjectName', '').split("'")[0]
        if cls not in FLOW_CLASSES:
            continue
        props = objects.get(ref['ObjectName'].split(':')[-1].rstrip("'"), {}).get('Properties', {})
        time = e.get('LinkValue', 0.0)
        if cls == 'TsAnimNotifyEndSkill_C':
            flow.append({'kind': 'ReadyEnd', 'time': time})
            flow.append({'kind': 'MovementCancel', 'time': time, 'duration': max(0.0, length - time)})
        elif cls == 'TsAnimNotifyStateNextAtt_C':
            flow.append({'kind': 'AcceptInput', 'time': time, 'duration': e.get('Duration', 0.0)})
        else:
            flow.append({'kind': 'Priority', 'time': time, 'value': int(props.get('Priority', 0))})
    name = original if original.startswith('AM_') else 'AM_' + original
    if config['montagePrefix'] != 'AM_':
        name = config['montagePrefix'] + name.removeprefix('AM_')
    return {
        'name': name, 'source': f"{config['source']}/{json_path.relative_to(AKI / config['source']).with_suffix('').as_posix()}", 'sourceLength': length,
        'tracks': tracks,
        'sections': [{'name': c['SectionName'], 'time': c.get('SegmentBeginTime', 0.0), 'next': c.get('NextSectionName', 'None')}
                     for c in p.get('CompositeSections', [])],
        # 原作没写的就是引擎默认值（0.25）
        'blendIn': p.get('BlendIn', {}).get('BlendTime', 0.25),
        'blendOut': p.get('BlendOut', {}).get('BlendTime', 0.25),
        'blendOutTriggerTime': p.get('BlendOutTriggerTime', -1.0),
        'rate': p.get('RateScale', 1.0),
        'flowNotifies': sorted(flow, key=lambda f: f['time']),
    }


def main() -> int:
    sys.stdout.reconfigure(encoding='utf-8')
    parser = argparse.ArgumentParser()
    parser.add_argument('--character', required=True, choices=sorted(CHARACTERS))
    parser.add_argument('--skills', required=True, help='原作技能 ID，逗号分隔')
    args = parser.parse_args()
    config = CHARACTERS[args.character]
    rows = dict(next(o for o in load(AKI / f'{COMMON_TABLE}.json') if o.get('Type') == 'DataTable')['Rows'])
    rows.update(next(o for o in load(AKI / config['source'] / 'Data' / 'DT_SkillInfo.json') if o.get('Type') == 'DataTable')['Rows'])
    anims: dict[str, str] = {}
    montages: dict[str, dict] = {}
    skills, missing, unsupported = [], [], []
    for skill_id in [int(s) for s in args.skills.split(',')]:
        row = strip(rows[str(skill_id)])
        ga = (row.get('SkillGA') or {}).get('AssetPathName', 'None')
        ga = None if ga == 'None' else ga.rsplit('.', 1)[-1].removesuffix('_C')
        if ga not in GA_PARENTS:
            unsupported.append((skill_id, row.get('SkillName'), ga))
            continue
        names = []
        # 角色技能：Animations（资产引用）；通用技能：MontagePaths（角色 CommonAnim 里的蒙太奇名）
        originals = [ref['AssetPathName'].rsplit('.', 1)[1] for ref in row.get('Animations') or []] or list(row.get('MontagePaths') or [])
        if GA_PARENTS[ga].get('montageLimit'):
            originals = originals[:GA_PARENTS[ga]['montageLimit']]
        for original in originals:
            montages.setdefault(original, montage(config, original, anims, missing))
            names.append(f"{config['montageFolder']}/{montages[original]['name']}")
        tags = sorted({TAG_MAP[t['TagName'].split('.')[-1]] for t in row.get('SkillTag', []) if t['TagName'].split('.')[-1] in TAG_MAP})
        action_tags = sorted({ACTION_TAG_MAP[t['TagName']] for t in row.get('SkillTag', []) if t['TagName'] in ACTION_TAG_MAP})
        skills.append({
            'id': skill_id, 'name': row.get('SkillName', ''), 'montages': names,
            'ga': {'path': f"{config['gaFolder']}/{config['gaNames'].get(skill_id, f'GA_{skill_id}')}",
                   'parent': GA_PARENTS[ga]['parent'], 'properties': GA_PARENTS[ga].get('properties', {}),
                   'originalGA': ga, 'interruptLevel': int(row.get('InterruptLevel', 0))},
            'tags': tags + [config['roleTag']],
            'actionTags': action_tags,
            # 原作 WalkOffLedge：为真 = 技能期间开边缘保护
            'walkOffLedge': bool(row.get('WalkOffLedge', False)),
        })
    # 原作序列是否开根运动 / 锁根骨
    anim_entries = []
    for sequence, path in sorted(anims.items()):
        found = list((AKI / config['source']).rglob(f'{sequence}.json')) + list((AKI / 'Character/Role/FemaleXL').glob(f'BaseAnim*/{sequence}.json'))
        props = next((o for o in load(found[0]) if o['Type'] == 'AnimSequence'), {}).get('Properties', {}) if found else {}
        anim_entries.append({'asset': path, 'source': sequence, 'rootMotion': bool(props.get('bEnableRootMotion', False)),
                             'forceRootLock': bool(props.get('bForceRootLock', False))})
    recipe = {
        'schemaVersion': 1, 'character': args.character, 'table': config['table'],
        'montageFolder': config['montageFolder'], 'anims': anim_entries,
        'montages': list(montages.values()), 'skills': skills,
    }
    out = HERE / f'{args.character}.skills.json'
    out.write_text(json.dumps(recipe, ensure_ascii=False, indent=1), encoding='utf-8')
    print(f'{out.name}: {len(anim_entries)} anims, {len(montages)} montages, {len(skills)} skills')
    for s in skills:
        print(' ', s['id'], s['name'], s['ga']['path'].rsplit('/', 1)[1], 'lv', s['ga']['interruptLevel'],
              [m.rsplit('/', 1)[1] for m in s['montages']], 'ledge' if s['walkOffLedge'] else '', s['actionTags'], s['ga']['originalGA'] or '')
    for m in montages.values():
        print('   ', m['name'], [(f['kind'], round(f['time'], 3)) for f in m['flowNotifies']])
    # 整张原作技能表的 ActionTags（只含有项目映射的动作标识）：给已经在项目里、不走本配方重建的技能补这一列
    # （-run=WuwaDataTableSetup -Recipe=ArtSource/Combat/<角色>.action_tags.json -OnlyFields=ActionTags -Apply；项目表里没有的行自动跳过）
    action_rows = []
    for row_id, raw in rows.items():
        tags = sorted({ACTION_TAG_MAP[t['TagName']] for t in strip(raw).get('SkillTag', []) if t['TagName'] in ACTION_TAG_MAP})
        if tags:
            action_rows.append({'Name': row_id, 'ActionTags': '(GameplayTags=(' + ','.join(f'(TagName="{t}")' for t in tags) + '))'})
    (HERE / f'{args.character}.action_tags.json').write_text(json.dumps(
        {'table': config['table'], 'source': f"{config['source']}/Data/DT_SkillInfo SkillTag 动作标识（ACTION_TAG_MAP）", 'rows': action_rows},
        ensure_ascii=False, indent=1), encoding='utf-8')
    print(f'{args.character}.action_tags.json: ' + ', '.join(f"{r['Name']}" for r in action_rows))
    if missing:
        print('  anims not imported into the project:', sorted(set(missing)))
    if unsupported:
        print('  skills whose original GA has no project port yet:', unsupported)
    return 1 if missing or unsupported else 0


if __name__ == '__main__':
    raise SystemExit(main())
