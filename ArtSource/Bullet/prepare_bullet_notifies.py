"""原作蒙太奇上的"添加子弹"通知 → 项目蒙太奇上 Spawn Bullet 通知的安装配方。
  python -I prepare_bullet_notifies.py --character changli

数据来源：原作蒙太奇（PakExport 导出的 JSON，缺的自动导出）上的 TsAnimNotifyReSkillEvent。
子弹 ID 的取法照原作 TsAnimNotifyReSkillEvent.K2_Notify：
  - 勾了 使用子弹id数组：子弹id数组 里的每一颗（配了 随机子弹权重数组 时只随机挑一颗——这里遇到就报错，项目还没有）；
  - 没勾：只用 子弹数据名（即使 子弹id数组 里有东西也不用，例如嘉贝普攻1）。
出生 / 初速度偏移数组、召唤者子弹、骨骼名字 等配置项目还没有，遇到非空就报错，免得装上去行为不一样。

输出 <character>.bullet_notifies.json，由 WuwaBulletNotifySetup commandlet 装到项目蒙太奇上。原作数据，不提交。
时间是原作秒数；commandlet 按项目蒙太奇的实际时长核对（同长，或整段拉长 1.25 倍的 24 帧导入）。
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
AKI = Path('C:/GamePakExtractor/Output/Exports/Client/Content/Aki')
PAKEXPORT = Path('C:/GamePakExtractor/Tools/PakExport/bin/out/PakExport.exe')

# 原作蒙太奇 → 项目蒙太奇。与音频配方（ArtSource/Audio/prepare_audio.py）、特效配方的阶段对应一致：
# 长离项目 AM_Attack05 用原作 AM_Attack04_1；嘉贝第一段用 AM_Attack01_N_S。
# 项目还没有对应蒙太奇的（嘉贝 _EX 强化段等）不装，见 notInstalled。
CHARACTERS = {
    'changli': {
        'anim': 'Character/Role/FemaleXL/ChangLi/CommonAnim',
        'montages': {
            'AM_Attack01': '/Game/Characters/Role/changli/AnimMontage/AM_Attack01',
            'AM_Attack02': '/Game/Characters/Role/changli/AnimMontage/AM_Attack02',
            'AM_Attack03': '/Game/Characters/Role/changli/AnimMontage/AM_Attack03',
            'AM_Attack04': '/Game/Characters/Role/changli/AnimMontage/AM_Attack04',
            'AM_Attack04_1': '/Game/Characters/Role/changli/AnimMontage/AM_Attack05',
            # 重击（长按普攻 1205006，WuwaRoleSkillSetup 按原作同名建）
            'AM_AttackL': '/Game/Characters/Role/changli/AnimMontage/AM_AttackL',
            # 空中：普攻 1~4、心眼·跃、下落攻击三段（WuwaRoleSkillSetup 按原作同名建）
            **{f'AM_AirAttack0{i}': f'/Game/Characters/Role/changli/AnimMontage/AM_AirAttack0{i}' for i in range(1, 6)},
            **{f'AM_AirAttack_{p}': f'/Game/Characters/Role/changli/AnimMontage/AM_AirAttack_{p}' for p in ('Start', 'Loop', 'End')},
            # E（共鸣技能）三段、大招、心眼·极
            **{f'AM_Skill01_{p}': f'/Game/Characters/Role/changli/AnimMontage/AM_Skill01_{p}' for p in ('Start', 'Loop', 'End')},
            'AM_Burst01': '/Game/Characters/Role/changli/AnimMontage/AM_Burst01',
            'AM_Burst02': '/Game/Characters/Role/changli/AnimMontage/AM_Burst02',
            'AM_Jump_Second_F': '/Game/Characters/Role/changli/AnimMontage/AM_Jump_Second_F',
            'AM_Jump_Second_B': '/Game/Characters/Role/changli/AnimMontage/AM_Jump_Second_B',
        },
    },
    'jiabei': {
        'anim': 'Character/Role/FemaleXL/Jiabeilina/CommonAnim',
        'montages': {
            'AM_Attack01_N_S': '/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_Attack01',
            'AM_Attack02_N': '/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_Attack02',
            'AM_Attack03_N': '/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_Attack03',
            'AM_Attack04_N': '/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_Attack04',
            'AM_Charge': '/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_Charge',
            'AM_Jump_Second_F': '/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_Jump_Second_F',
            'AM_Jump_Second_B': '/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_Jump_Second_B',
            # 重击 1~3（1208016~1208018）
            'AM_HeavyAttack_Human01': '/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_HeavyAttack_Human01',
            'AM_HeavyAttack_Human02': '/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_HeavyAttack_Human02',
            'AM_HeavyAttack_Human03': '/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_HeavyAttack_Human03',
            # 空中下落攻击 1208008（起手 / 下落循环 / 落地段）
            'AM_AirAttack_Start': '/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_AirAttack_Start',
            'AM_AirAttack_Loop': '/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_AirAttack_Loop',
            'AM_AirAttack_End': '/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_AirAttack_End',
            # 变身：强化 E、E 跳踢两段（原作在 CommonAnim/Jump_Attack 子目录）、EX 普攻 1~4 / 4 衔接版 / 爆蛇眼、EX 重击 1~3、大招
            **{m: f'/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_{m.removeprefix("AM_")}'
               for m in ('AM_Skill01', 'AM_Attack01_EX', 'AM_Attack02_EX', 'AM_Attack03_EX', 'AM_Attack04_EX', 'AM_Attack04_EXH',
                         'AM_Attack05_1_N', 'AM_HeavyAttack_evil01', 'AM_HeavyAttack_evil02', 'AM_HeavyAttack_evil03', 'AM_Burst')},
            'Jump_Attack/AM_Attack_Jump_start': '/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_Attack_Jump_start',
            'Jump_Attack/AM_Attack_Jump_next': '/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_Attack_Jump_next',
        },
    },
}

# 项目通知还不支持的原作配置项：出现非空值就报错
UNSUPPORTED = ['子弹出生位置偏移', '子弹初速度偏移', '子弹出生位置偏移数组', '子弹初速度偏移数组', '使用召唤者子弹',
               '随机子弹权重数组', '传入当前实体位置', '骨骼名字', '指定Tag的Component', '通知实体子弹动画帧事件']


# 原作 UserDefinedEnum（代码里用枚举资产 Names 的值，不是 NewEnumeratorN 的 N）
SKILL_BEHAVIOR_CONDITION = {'ESkillBehaviorConditionType::NewEnumerator0': 'HasSkillTarget', 'ESkillBehaviorConditionType::NewEnumerator5': 'OwnerTags'}
SKILL_BEHAVIOR_BULLET_ACTION = 'ESkillBehaviorActionType::NewEnumerator3'   # 创建子弹
# 原作 Tag → 项目 Tag。项目里没有的机制（QTE、共鸣链、心火层数）的 Tag 角色身上永远没有：条件按"没有"静态判定
OWNER_TAG_MAP = {'角色.R2T1ChangLiMd10011.buff相关.心眼': 'Role.Changli.Buff.TrueSight',
                 # 嘉贝变身中（原作 Tag 名被哈希；强化 E 的变身 buff 1208001031 / 1208001001 授予）
                 '角色.2e985a12.ab2b6d02.0c77b016': 'Role.Jiabei.State.Transformed'}
NEVER_PRESENT_TAG_PREFIXES = ('角色.R2T1ChangLiMd10011.动作标识.QTE', '角色.R2T1ChangLiMd10011.共鸣.', '角色.R2T1ChangLiMd10011.buff相关.心火')


def strip_guid(value):
    if isinstance(value, dict):
        return {re.sub(r'_\d+_[0-9A-F]{32}$', '', k): strip_guid(v) for k, v in value.items()}
    if isinstance(value, list):
        return [strip_guid(v) for v in value]
    return value


def skill_behavior_bullets(obj: dict, source: str, length: float, project: str, time: float, errors: list) -> list:
    """原作 TsAnimNotifySkillBehavior：每个行为 = 条件组（全部满足，公式为空时）+ 行为组。只装 创建子弹 行为，条件折算到 Spawn Bullet 通知上。"""
    out = []
    for behavior in strip_guid(obj['Properties'].get('技能行为', [])):
        bullets = [b['bulletRowName'] for a in behavior.get('SkillBehaviorActionGroup', []) if a.get('ActionType') == SKILL_BEHAVIOR_BULLET_ACTION
                   for b in a.get('Bullets', []) for _ in range(int(b.get('bulletCount', 1)))]
        if not bullets:
            continue
        if behavior.get('SkillBehaviorConditionFormula'):
            errors.append(f'{source} @ {time:.3f}s: SkillBehavior condition formula not supported')
            continue
        entry = {'sourceAsset': source, 'sourceLength': length, 'asset': project, 'time': time, 'bulletIds': bullets,
                 'requiredTags': [], 'blockedTags': []}
        possible = True
        for c in behavior.get('SkillBehaviorConditionGroup', []):
            kind = SKILL_BEHAVIOR_CONDITION.get(c.get('ConditionType'))
            reverse = bool(c.get('Reverse'))
            if kind == 'HasSkillTarget':
                entry['skillTarget'] = 'NoTarget' if reverse else 'HasTarget'
            elif kind == 'OwnerTags':
                tags = [t if isinstance(t, str) else t['TagName'] for t in c.get('TagToCheck', [])]
                if c.get('AnyTag') and len(tags) > 1:
                    errors.append(f'{source} @ {time:.3f}s: SkillBehavior any-tag check with {len(tags)} tags not supported')
                    possible = False
                    continue
                for tag in tags:
                    if tag in OWNER_TAG_MAP:
                        entry['blockedTags' if reverse else 'requiredTags'].append(OWNER_TAG_MAP[tag])
                    elif tag.startswith(NEVER_PRESENT_TAG_PREFIXES):
                        if not reverse:
                            possible = False   # 要求一个永远不会有的 Tag：这个行为永远不触发
                    else:
                        errors.append(f'{source} @ {time:.3f}s: SkillBehavior tag {tag} has no mapping')
                        possible = False
            else:
                errors.append(f'{source} @ {time:.3f}s: SkillBehavior condition {c.get("ConditionType")} not supported')
                possible = False
        if possible:
            out.append(entry)
        else:
            print(f'  (SkillBehavior {source} @ {time:.3f}s bullets {bullets}: condition never true in the project, skipped)')
    return out


def ensure_json(package: str) -> Path:
    path = AKI / f'{package}.json'
    if not path.exists():
        result = subprocess.run([str(PAKEXPORT), 'json', f'Client/Content/Aki/{package}.uasset'],
                                capture_output=True, text=True, encoding='utf-8', errors='replace')
        if result.returncode != 0 or not path.exists():
            raise RuntimeError('PakExport json failed: ' + result.stderr[-2000:])
    return path


def is_empty(value) -> bool:
    if value in (None, False, '', 'None', 0):
        return True
    if isinstance(value, list):
        return len(value) == 0
    if isinstance(value, dict):  # FVector 之类：全 0 算空
        return all(v in (0, 0.0) for v in value.values())
    return False


def bullet_ids(props: dict) -> list[str]:
    """原作 TsAnimNotifyReSkillEvent.K2_Notify 的取法。"""
    if props.get('使用子弹id数组'):
        return [str(i) for i in props.get('子弹id数组', []) if str(i) not in ('', 'None')]
    name = str(props.get('子弹数据名') or '')
    return [name] if name not in ('', 'None') else []


def main() -> int:
    sys.stdout.reconfigure(encoding='utf-8')
    parser = argparse.ArgumentParser()
    parser.add_argument('--character', required=True, choices=sorted(CHARACTERS))
    args = parser.parse_args()
    config = CHARACTERS[args.character]

    notifies, errors = [], []
    for source, project in config['montages'].items():
        objects = json.loads(ensure_json(f"{config['anim']}/{source}").read_text(encoding='utf-8'))
        by_name = {o.get('Name'): o for o in objects}
        montage = next(o for o in objects if o.get('Type') == 'AnimMontage')
        length = float(montage['Properties']['SequenceLength'])
        for n in montage['Properties'].get('Notifies', []):
            ref = ((n.get('Notify') or n.get('NotifyStateClass')) or {}).get('ObjectName', '')
            obj = by_name.get(ref.split(':', 1)[-1].rstrip("'").rsplit('.', 1)[-1])
            if obj and obj.get('Type') == 'TsAnimNotifySkillBehavior_C':
                notifies.extend(skill_behavior_bullets(obj, source, length, project, float(n.get('LinkValue', 0.0)), errors))
                continue
            if not obj or obj.get('Type') != 'TsAnimNotifyReSkillEvent_C':
                continue
            props = obj['Properties']
            time = float(n.get('LinkValue', 0.0))
            for key in UNSUPPORTED:
                if not is_empty(props.get(key)):
                    errors.append(f'{source} @ {time:.3f}s: {key} = {props[key]!r} is not supported yet')
            ids = bullet_ids(props)
            if not ids:
                errors.append(f'{source} @ {time:.3f}s: no bullet id')
                continue
            notifies.append({'sourceAsset': source, 'sourceLength': length, 'asset': project, 'time': time, 'bulletIds': ids})

    # 原作在这个角色目录下还有哪些蒙太奇带"添加子弹"，项目没有对应蒙太奇（只列出来，便于以后补）
    folder = AKI / config['anim']
    not_installed = sorted(p.stem for p in folder.glob('AM_*.json')
                           if p.stem not in config['montages'] and 'TsAnimNotifyReSkillEvent' in p.read_text(encoding='utf-8'))

    out = HERE / f'{args.character}.bullet_notifies.json'
    out.write_text(json.dumps({'character': args.character, 'notifies': notifies, 'notInstalled': not_installed},
                              ensure_ascii=False, indent=1), encoding='utf-8')
    print(f'{out}: {len(notifies)} notifies on {len(config["montages"])} montages; {len(not_installed)} original montages not mapped')
    for n in notifies:
        print(f"  {n['sourceAsset']} @ {n['time']:.3f}s -> {', '.join(n['bulletIds'])}")
    for e in errors:
        print(f'  ERROR {e}')
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
