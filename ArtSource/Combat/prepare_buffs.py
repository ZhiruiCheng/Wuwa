"""原作 buff / buff 表现（cue）→ 项目 GE 资产、DT_Buff、DT_BuffCue 的导入配方，以及要用到这些 buff 的地方（动画通知、技能开始 buff）。
  python -I prepare_buffs.py

数据来源（原作 ConfigDB，flatbuffer，字段见 Define/Config/*.js）：
  db_buff.db 的 buff（Buff.js）：持续方式 DurationPolicy（0 瞬时 / 1 永久 / 2 有时长，和 UE EGameplayEffectDurationType 同值）、时长 DurationMagnitude[0]（秒）、
    授予 Tag GrantedTags、表现 GameplayCueIds、额外效果 ExtraEffectID / ExtraEffectParameters（5 = ExecuteBulletOrBuff：参数 0 = 0 加 buff / 1 子弹 / 2 移除 buff，
    参数 1 = buff ID（# 分隔），参数 2 = 次数 / 层数）。
  db_buffeffect.db 的 gameplaycue（GameplayCue.js）：CueType、Path（图标）、Priority、Parameters（见 FWuwaBuffCueRow）。只导入 UI 类型（2 / 14 / 24 / 37）。
不支持的字段（叠层、属性修改、持续 / 移除 / 施加 Tag 条件、其他额外效果……）直接报错，不静默丢掉；用到时再加。
原作 Tag 名映射成项目 Tag（TAG_MAP），没映射的报错。

动画通知：原作角色蒙太奇里加这些 buff 的 TsAnimNotifyAddBuff，以及 Tag 有项目映射的 TsAnimNotifyStateAddTag 窗口（同名蒙太奇在项目里存在才加；时间按 项目长度 / 原作长度 缩放）。
技能开始 buff：原作 DT_SkillInfo 的 SkillStartBuff，只保留已导入的 buff。

输出（原作数据，不提交）：
  buffs.recipe.json                 → -run=WuwaBuffSetup -Recipe=ArtSource/Combat/buffs.recipe.json -Apply（GE、两张表、图标、动画通知）
  <role>.skill_start_buffs.json     → -run=WuwaDataTableSetup -Recipe=... -OnlyFields=SkillStartBuffs,SkillEndBuffs -Apply
"""
from __future__ import annotations

import json
import re
import sqlite3
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
sys.path.insert(0, str(HERE.parent / 'Bullet'))
sys.path.insert(0, str(HERE.parent / 'UI/KuroPrefab'))
import kuro_fb  # noqa: E402
import kuro_export as ke  # noqa: E402
import kuro_layout as kl  # noqa: E402

AKI = Path('C:/GamePakExtractor/Output/Exports/Client/Content/Aki')
CONFIG = AKI / 'JavaScript/Core/Define/Config'
BUFF_DB = AKI / 'ConfigDB/db_buff.db'
CUE_DB = AKI / 'ConfigDB/db_buffeffect.db'

# 要导入的 buff：长离心眼（普攻 4 / E / 变奏入场加，心眼技开始时 1205000002 移除）
BUFF_IDS = [1205000001, 1205000002,
            # 长离空中普攻次数：空中普攻 / 下落攻击起手各加一层，落地且不在空中普攻里时移除（输入按层数选第几段）
            1205000003,
            # 心眼·跃 1205106 技能开始：0.2 秒内按跳跃不出招也不跳（原作 动作标识.前几帧禁止跳跃）
            1205601001,
            # 空中闪避（通用技能 100003 / 100004，GA_Role_LandEnd_Net 激活时加）：闪避禁止，落地且在前台时移除 = 一次滞空只能空中闪避一次
            1101004005,
            # 长离 E / 大招 / 心眼·极 技能开始：协奏 +1400 / 共鸣能量清空 / 协奏 +1000（技能表 SkillStartBuff）
            1205200001, 1205300001, 1205200005,
            # 大招 2.6 秒心火 +4（SpecialEnergy1），心眼·极 0.11 秒心火 -4
            1205104005, 1205104004,
            # 心火 +1：心眼技子弹（1205005011 / 1205005110）第一次命中给自己加（子弹表 能量恢复类GE数组）
            1205104003,
            # 嘉贝连段窗口（技能表 SkillEndBuff）：普攻 1~4 / 重击 1~3 结束时加 0.1 秒的 buff（1208002001~2007），
            # 到期再加 0.6 秒的 buff（1208002011~2017），授予"刚打完第几段"的 Tag，输入规则按它选下一段
            *range(1208002001, 1208002008), *range(1208002011, 1208002018),
            # 嘉贝变身（强化 E 1208101 技能开始 buff，顺序同技能表）：1208001005 弹药清空、1208003002 协奏 +1000、
            # 1208003313 弹药 = 能量（SE3 += SE1，生效后加 1208003315 能量清空）、1208001001 变身 50 秒（授予 变身中 Tag；
            # 弹药打光（SE3 进入 (-1, 0]）或到期 → 1208001002）。1208001002 0.1 秒后 → 1208001032 移除变身 buff、SE2 / SE3 清空
            1208001005, 1208003002, 1208003313, 1208003315, 1208001001, 1208001002, 1208001032, 1208003316, 1208003317,
            # 嘉贝子弹第一次命中给自己加（子弹表 能量恢复类GE数组）：没变身时 SE1 +2~+15（能量条），变身中 SE3 -2~-10（弹药）
            1208003302, 1208003303, 1208003304, 1208003305, 1208003320, 1208003325,
            1208003307, 1208003308, 1208003309, 1208003310, 1208003327, 1208003330,
            # 嘉贝大招 1208301 技能开始：共鸣能量清空（通用 1101003007）、0.4 秒无敌（通用 1001001029）；
            # 1208001091（14 秒，授予的 Tag ab2b6d02.7a3d88e1 项目没有用到它的地方）不导入
            1101003007, 1001001029]

# 原作属性 ID（EAttributeType，见 CharacterAttributeTypes.js）→ 项目 UWuwaAttributeSet 属性
# 原作 attributeIdsWithMax：属性 → 它的上限属性（额外效果 6 的万分比模式用）
MAX_ATTRIBUTE = {60: 59, 77: 76, 62: 61, 64: 63, 66: 65, 68: 67}
ATTRIBUTE_MAP = {59: 'MaxEnergy', 60: 'Energy', 76: 'MaxConcertoEnergy', 77: 'ConcertoEnergy',
                 61: 'MaxSpecialEnergy1', 62: 'SpecialEnergy1', 63: 'MaxSpecialEnergy2', 64: 'SpecialEnergy2',
                 65: 'MaxSpecialEnergy3', 66: 'SpecialEnergy3', 67: 'MaxSpecialEnergy4', 68: 'SpecialEnergy4'}

# 原作 Tag → 项目 Tag（DefaultGameplayTags.ini）
TAG_MAP = {
    '角色.R2T1ChangLiMd10011.buff相关.心眼': 'Role.Changli.Buff.TrueSight',
    '角色.Common.切人不隐藏': 'State.Formation.SwitchNoHide',
    '角色.R2T1ChangLiMd10011.动作标识.空中普攻次数': 'Role.Changli.Action.AirAttackCount',
    '行为状态.位置状态.空中': 'State.Position.Air',
    '角色.R2T1ChangLiMd10011.动作标识.空中普攻1': 'Role.Changli.Action.AirAttack1',
    '角色.R2T1ChangLiMd10011.动作标识.空中普攻2': 'Role.Changli.Action.AirAttack2',
    '角色.R2T1ChangLiMd10011.动作标识.空中普攻3': 'Role.Changli.Action.AirAttack3',
    '角色.R2T1ChangLiMd10011.动作标识.空中普攻4': 'Role.Changli.Action.AirAttack4',
    '角色.R2T1ChangLiMd10011.动作标识.空中普攻5': 'Role.Changli.Action.AirAttack5',
    '角色.R2T1ChangLiMd10011.动作标识.前几帧禁止跳跃': 'Role.Changli.Action.JumpForbid',
    '角色.Common.技能通用标识.闪避禁止': 'State.Combat.DodgeForbid',
    '行为状态.位置状态.地面': 'State.Position.Ground',
    '行为状态.逻辑状态.前台': 'State.Formation.OnField',
    '功能.逻辑状态标识.无敌.通用无敌': 'State.Combat.Invincible',
    # 嘉贝连段窗口（原作 Tag 名被哈希：角色.<珂莱塔>.<连段>.<段>；按授予它的 buff 1208002011~2017 和技能表 SkillEndBuff 对上是哪一段）
    '角色.2e985a12.76dc0cb2.f30aed45': 'Role.Jiabei.Combo.Attack1',
    '角色.2e985a12.76dc0cb2.73fc0dd8': 'Role.Jiabei.Combo.Attack2',
    '角色.2e985a12.76dc0cb2.e59e5262': 'Role.Jiabei.Combo.Attack3',
    '角色.2e985a12.76dc0cb2.3c4f6496': 'Role.Jiabei.Combo.Attack4',
    '角色.2e985a12.76dc0cb2.1af097d9': 'Role.Jiabei.Combo.Heavy1',
    '角色.2e985a12.76dc0cb2.42377ed0': 'Role.Jiabei.Combo.Heavy2',
    '角色.2e985a12.76dc0cb2.aad5aa16': 'Role.Jiabei.Combo.Heavy3',
    # 嘉贝 EX 普攻 3 期间（技能表里没有，AM_Attack03_EX 的 AddTag 窗口加）：接招窗口里按普攻出 EX 普攻 4
    '角色.2e985a12.51c4e7d2.8f0883f1': 'Role.Jiabei.Action.ExAttack3',
    # 嘉贝变身中（变身 buff 1208001001 授予；输入规则、子弹按它选变身连段 / 变身子弹）
    '角色.2e985a12.ab2b6d02.0c77b016': 'Role.Jiabei.State.Transformed',
}

# 客户端数据里没有任何东西授予的 Tag（buff、蒙太奇、技能表都搜过）：施加条件里"有它就不加"当作永远满足，"要有它才加"的 buff 不导入。
#   角色.2e985a12.85afd703.68849450 —— 嘉贝某个天赋 / 共鸣链状态（变身 buff 1208001001 有它不加、换成 1208001031）
#   功能.系统状态标识.出招教学 —— 出招教学关卡
NEVER_PRESENT_TAGS = {'角色.2e985a12.85afd703.68849450', '功能.系统状态标识.出招教学'}

ROLES = {
    'changli': {'prefix': '1205', 'folder': 'Character/Role/FemaleXL/ChangLi', 'project': '/Game/Characters/Role/changli',
                'skillTable': '/Game/Characters/Role/changli/Data/DT_SkillInfo_Changli', 'montagePrefix': 'AM_'},
    'jiabei': {'prefix': '1208', 'folder': 'Character/Role/FemaleXL/Jiabeilina', 'project': '/Game/Characters/Role/jiabei',
               'skillTable': '/Game/Characters/Role/jiabei/Data/DT_SkillInfo_Jiabei', 'montagePrefix': 'AM_Jiabei_'},
}
COMMON_BUFF_DIR = '/Game/Game/Combat/Buff'
UI_CUE_TYPES = {2, 14, 24, 37}
# 支持的字段；其余非默认值的字段报错
SUPPORTED = {'id', 'gedesc', 'durationpolicy', 'durationcalculationpolicy', 'durationmagnitude', 'calculationpolicy', 'gameplaycueids',
             'grantedtags', 'extraeffectid', 'extraeffectparameters', 'extraeffectparametersgrow1', 'extraeffectparametersgrow2',
             'extraeffectcd', 'extraeffectcdfortarget', 'extraeffectprobability',
             # 叠层上限：项目每次加 buff 是独立的一份（不合并），Tag 计数 = 份数；上限只记录（输入层按层数选招时自己夹住）
             'stacklimitcount', 'removaltagignores', 'removaltagrequirements',
             # 原作 怪物.AI角色 身上不加：项目这些 buff 只由角色自己的技能加给自己，不会加到怪物身上
             'applicationtagignores', 'applicationsourcetagignores',
             # 编队共享（原作 FormationPolicy：队伍里其他角色也加）：项目暂不共享，只加在当前角色身上（导入时打印提醒）
             'formationpolicy',
             # 属性修改（只支持瞬时 buff 改状态类属性：能量 / 协奏 / 特殊能量），见 modifiers()
             'gameattributeid', 'modifiermagnitude', 'modifiermagnitude2',
             # 到期时施加别的 buff（正常到期 / 提前移除）：GE 的 AdditionalEffects 组件 OnCompleteNormal / OnCompletePrematurely
             'routineexpirationeffects', 'prematureexpirationeffects',
             # BuffAction：只认下面 IGNORED_BUFF_ACTIONS 里查过、确认不起作用的；其余报错
             'buffaction',
             # 施加条件（持有者身上的 Tag）：GE 的 TargetTagRequirements.ApplicationTagRequirements
             'applicationtagrequirements',
             # 加上时移除带这些 Tag 的 buff（RemoveOther 组件，按 GE 授予的 Tag 查）
             'removebuffwithtags'}
# 嘉贝连段窗口 buff 1208002011~2018 的 BuffAction（类型 3 = 和施加者 Tag 有关的动作，执行逻辑在原作服务端，客户端看不到）。
# 这个 Tag 在客户端数据里没有任何东西授予（蒙太奇、buff、技能表都搜过），按现有数据这条动作不会触发：不导入。
IGNORED_BUFF_ACTIONS = {'3#角色.R2T1CalbrenaMd10011.状态标识.不会移除衔接标记'}
DEFAULTS = {'durationcalculationpolicy': [0], 'modifiermagnitude2': [0], 'calculationpolicy': [0], 'extraeffectparametersgrow1': [0], 'extraeffectparametersgrow2': [0],
            'extraeffectcd': [-1.0], 'extraeffectcdfortarget': [-1.0], 'extraeffectprobability': [10000]}


def decode(db: Path, table: str, js: str, row_id: int) -> dict | None:
    blob = sqlite3.connect(db).execute(f'select BinData from {table} where Id=?', (row_id,)).fetchone()
    if not blob:
        return None
    fb = kuro_fb.FB(blob[0])
    row = kuro_fb.decode_table(fb, fb.u32(0), kuro_fb.parse_js(str(CONFIG / js)), lambda n: None)
    return {k: v for k, v in row.items() if not k.endswith('Array')}


def buff_dir(buff_id: int) -> str:
    for role in ROLES.values():
        if str(buff_id).startswith(role['prefix']):
            return role['project'] + '/Buff'
    return COMMON_BUFF_DIR


def modifiers(buff_id: int, raw: dict, errors: list[str]) -> list[dict]:
    """原作 ActiveBuff 瞬时 buff 改状态类属性（AddBaseValue）：
    计算方式 0 = 加 数值；2 = 加 数值/10000 × 另一个属性（[2, 属性, 来源 1=施加者 / 其他=持有者, ...]）。"""
    attr = int(raw.get('gameattributeid', 0))
    if not attr:
        return []
    if attr not in ATTRIBUTE_MAP:
        errors.append(f'buff {buff_id}: attribute {attr} has no project mapping (ATTRIBUTE_MAP)')
        return []
    if int(raw.get('durationpolicy', 0)) != 0:
        errors.append(f'buff {buff_id}: attribute modifier on a lasting buff is not supported yet')
        return []
    magnitude = raw.get('modifiermagnitude', [0])
    if len(magnitude) != 1:
        errors.append(f'buff {buff_id}: per-level modifier magnitude {magnitude} not supported')
        return []
    policy = raw.get('calculationpolicy', [0])
    entry = {'attribute': ATTRIBUTE_MAP[attr], 'magnitude': float(magnitude[0])}
    if policy[0] == 2:
        # [2, 属性, 来源, 取值方式（AbilityUtils.GetAttrValue：0 基础值 / 1 当前值）, 快照（瞬时 buff 执行时就取，无所谓）, 下限, 比例, 上限, 取整]
        based, source = int(policy[1]), int(policy[2]) if len(policy) > 2 else 0
        value_type = int(policy[3]) if len(policy) > 3 else 0
        if based not in ATTRIBUTE_MAP or value_type not in (0, 1) or any(policy[5:]):
            errors.append(f'buff {buff_id}: calculation policy {policy} not supported')
            return []
        entry.update({'basedOn': ATTRIBUTE_MAP[based], 'basedOnInstigator': source == 1, 'basedOnBaseValue': value_type == 0,
                      'coefficient': float(magnitude[0]) / 10000.0})
    elif policy[0] != 0:
        errors.append(f'buff {buff_id}: calculation policy {policy} not supported')
        return []
    return [entry]


def convert_buff(buff_id: int, errors: list[str]) -> dict:
    raw = decode(BUFF_DB, 'buff', 'Buff.js', buff_id)
    if raw is None:
        raise SystemExit(f'buff {buff_id} not found')
    for action in raw.get('buffaction', []):
        if action not in IGNORED_BUFF_ACTIONS:
            errors.append(f'buff {buff_id}: BuffAction {action} not supported')
    for key, value in raw.items():
        if key not in SUPPORTED and value not in (0, 0.0, '', [], None, False):
            errors.append(f'buff {buff_id}: unsupported field {key} = {value}')
        if key in DEFAULTS and value != DEFAULTS[key] and not (key == 'calculationpolicy' and raw.get('gameattributeid'))                 and not (key.startswith('extraeffectparametersgrow') and int(raw.get('extraeffectid', 0)) == 6):
            errors.append(f'buff {buff_id}: unsupported non-default {key} = {value}')
    policy = int(raw.get('durationpolicy', 0))
    tags = []
    for tag in raw.get('grantedtags', []):
        if tag not in TAG_MAP:
            errors.append(f'buff {buff_id}: tag {tag} has no project mapping (TAG_MAP)')
        else:
            tags.append(TAG_MAP[tag])
    removal = []
    for tag in raw.get('removaltagignores', []):
        if tag not in TAG_MAP:
            errors.append(f'buff {buff_id}: removal tag {tag} has no project mapping (TAG_MAP)')
        else:
            removal.append(TAG_MAP[tag])
    removal_require = []
    for tag in raw.get('removaltagrequirements', []):
        if tag not in TAG_MAP:
            errors.append(f'buff {buff_id}: removal tag {tag} has no project mapping (TAG_MAP)')
        else:
            removal_require.append(TAG_MAP[tag])
    # 施加条件：持有者身上 有这些 Tag 不加 / 要有这些 Tag 才加（原作 怪物.AI角色、没人授予的 Tag（NEVER_PRESENT_TAGS）的"不加"永远满足，略过）
    application_ignore, application_require = [], []
    for key, out in (('applicationtagignores', application_ignore), ('applicationtagrequirements', application_require)):
        for tag in raw.get(key, []):
            if key == 'applicationtagignores' and (tag == '怪物.AI角色' or tag in NEVER_PRESENT_TAGS):
                continue
            if tag in NEVER_PRESENT_TAGS:
                errors.append(f'buff {buff_id}: requires {tag}, which nothing grants (never applies; do not import)')
            elif tag not in TAG_MAP:
                errors.append(f'buff {buff_id}: {key} tag {tag} has no project mapping (TAG_MAP)')
            else:
                out.append(TAG_MAP[tag])
    if any(t != '怪物.AI角色' and t not in NEVER_PRESENT_TAGS for t in raw.get('applicationsourcetagignores', [])):
        errors.append(f"buff {buff_id}: applicationsourcetagignores = {raw['applicationsourcetagignores']} not supported")
    # 加上时移除带这些 Tag 的 buff：没映射的 Tag 项目里没有 buff 会授予，略过
    remove_with_tags = []
    for tag in raw.get('removebuffwithtags', []):
        if tag in TAG_MAP:
            remove_with_tags.append(TAG_MAP[tag])
        else:
            print(f'  note: buff {buff_id} removebuffwithtags {tag}: no imported buff grants it, skipped')
    if raw.get('formationpolicy'):
        print(f'  note: buff {buff_id} formationpolicy {raw["formationpolicy"]} (team sharing) is not ported; applied to the current role only')
    magnitude = float(raw.get('durationmagnitude', [0])[0]) if policy == 2 else 0.0
    if policy == 2 and magnitude < 0:
        # 原作 ActiveBuff.SetDuration：时长 < 0 = 无限（GetRemainDuration 返回 INFINITY_DURATION）
        policy, magnitude = 1, 0.0
    remove, add, attribute_event = [], [], None
    effect = int(raw.get('extraeffectid', 0))
    if effect == 5:
        params = raw['extraeffectparameters']
        goal, ids = int(params[0]), [int(x) for x in params[1].split('#')]
        if goal == 2:
            remove = ids
        elif goal == 0:
            add = ids
        else:
            errors.append(f'buff {buff_id}: ExecuteBulletOrBuff goal {goal} (bullets) not supported')
    elif effect == 6:
        # AttributeEventEffects：[属性, 万分比 1, 目标类型（0 加 buff）, buff ID#, 离开区间撤掉 1, 监听对象, 施加对象]；
        # 区间 (成长参数 1, 成长参数 2]，按等级取值，没配 = -1（AbilityUtils.GetLevelValue 默认值）
        params = raw['extraeffectparameters']
        attr, per = int(params[0]), int(params[1]) == 1
        bounds = [raw.get(k) or [-1] for k in ('extraeffectparametersgrow1', 'extraeffectparametersgrow2')]
        if int(params[2]) != 0 or any(int(x or 0) for x in params[5:]) or any(len(b) != 1 for b in bounds):
            errors.append(f'buff {buff_id}: attribute event {params} {bounds} not supported (only add buffs to the owner, one level)')
        elif attr not in ATTRIBUTE_MAP or (per and MAX_ATTRIBUTE.get(attr) not in ATTRIBUTE_MAP):
            errors.append(f'buff {buff_id}: attribute event attribute {attr} has no project mapping')
        else:
            attribute_event = {'attribute': ATTRIBUTE_MAP[attr], 'perTenThousand': per,
                               'maxAttribute': ATTRIBUTE_MAP[MAX_ATTRIBUTE[attr]] if per else '',
                               'lower': float(bounds[0][0]), 'upper': float(bounds[1][0]),
                               'addBuffs': [int(x) for x in params[3].split('#')],
                               'removeOnLeave': len(params) > 4 and int(params[4] or 0) == 1}
    elif effect:
        errors.append(f'buff {buff_id}: extra effect {effect} not supported')
    name = f'GE_Buff_{buff_id}'
    return {
        'id': buff_id,
        'path': f'{buff_dir(buff_id)}/{name}',
        'durationPolicy': ['Instant', 'Infinite', 'HasDuration'][policy],
        'duration': magnitude,
        'grantedTags': tags,
        'removalIgnoreTags': removal,
        'removalRequireTags': removal_require,
        'modifiers': modifiers(buff_id, raw, errors),
        'stackLimit': int(raw.get('stacklimitcount', 0)),
        'cueIds': [int(c) for c in raw.get('gameplaycueids', [])],
        'removeBuffs': remove,
        'removeBuffsWithTags': remove_with_tags,
        'addBuffs': add,
        'applicationIgnoreTags': application_ignore,
        'applicationRequireTags': application_require,
        'attributeEvent': attribute_event,
        'expireBuffs': {'normal': [int(x) for x in raw.get('routineexpirationeffects', [])],
                        'premature': [int(x) for x in raw.get('prematureexpirationeffects', [])]},
        'source': f'db_buff {buff_id}: {json.dumps(raw, ensure_ascii=False, default=str)[:300]}',
    }


def convert_cues(cue_ids: list[int], textures: ke.Textures) -> list[dict]:
    rows = []
    for cue_id in cue_ids:
        raw = decode(CUE_DB, 'gameplaycue', 'GameplayCue.js', cue_id)
        if raw is None or int(raw.get('cuetype', 0)) not in UI_CUE_TYPES:
            continue
        package = kl.kp.game_to_package(raw['path'].split('.')[0])
        rows.append({'Name': str(cue_id), 'CueType': int(raw['cuetype']), 'Icon': textures.texture(package)['file'],
                     'Priority': int(raw.get('priority', 0)), 'Parameters': [str(p) for p in raw.get('parameters', [])]})
    return rows


def strip_key(key: str) -> str:
    return re.sub(r'_\d+_[0-9A-F]{32}$', '', key)


def montage_notifies(buff_ids: set[int]) -> list[dict]:
    """原作角色蒙太奇里加这些 buff 的 TsAnimNotifyAddBuff → 项目同名蒙太奇。"""
    out = []
    for role in ROLES.values():
        for path in sorted((AKI / role['folder']).rglob('AM_*.json')):
            data = json.loads(path.read_text(encoding='utf-8'))
            montage = next((o for o in data if o.get('Type') == 'AnimMontage'), None)
            if not montage:
                continue
            notifies = {}
            for n in montage['Properties'].get('Notifies', []):
                obj = (n.get('Notify') or {}).get('ObjectName', '')
                notifies[obj.split(':')[-1].rstrip("'")] = n.get('LinkValue', n.get('Time', 0.0))
            for o in data:
                # 原作 TsAnimNotifyStateAddTag 窗口：Tag 有项目映射（TAG_MAP）的装成 Add Loose Tag Window（例：AM_AirAttack_Start 切人不隐藏）
                if o.get('Type') == 'TsAnimNotifyStateAddTag_C' and o.get('Properties', {}).get('Tag', {}).get('TagName') in TAG_MAP:
                    project = f"{role['project']}/AnimMontage/{role['montagePrefix']}{path.stem.removeprefix('AM_')}"
                    if not (REPO / 'Content' / (project.replace('/Game/', '') + '.uasset')).exists():
                        continue
                    event = next(n for n in montage['Properties'].get('Notifies', [])
                                 if (n.get('NotifyStateClass') or {}).get('ObjectName', '').split(':')[-1].rstrip("'") == o['Name'])
                    out.append({'montage': project, 'time': event.get('LinkValue', 0.0), 'duration': event.get('Duration', 0.0),
                                'tag': TAG_MAP[o['Properties']['Tag']['TagName']],
                                'sourceLength': montage['Properties'].get('SequenceLength', 0.0),
                                'source': f"{role['folder']}/{path.relative_to(AKI / role['folder']).as_posix()}"})
                if o.get('Type') == 'TsAnimNotifyAddBuff_C' and int(o.get('Properties', {}).get('BuffId', 0)) in buff_ids:
                    asset = path.stem
                    project = f"{role['project']}/AnimMontage/{role['montagePrefix']}{asset.removeprefix('AM_')}"
                    if not (REPO / 'Content' / (project.replace('/Game/', '') + '.uasset')).exists():
                        print(f'  (skip notify: {asset} not in project)')
                        continue
                    out.append({'montage': project, 'time': notifies.get(o['Name'], 0.0), 'buffId': int(o['Properties']['BuffId']),
                                'sourceLength': montage['Properties'].get('SequenceLength', 0.0),
                                'source': f"{role['folder']}/{path.relative_to(AKI / role['folder']).as_posix()}"})
    return out


def skill_start_buffs(buff_ids: set[int]) -> dict[str, list[dict]]:
    """原作 DT_SkillInfo 的 SkillStartBuff / SkillEndBuff（只留已导入的 buff）→ 技能表 SkillStartBuffs / SkillEndBuffs。"""
    out = {}
    for name, role in ROLES.items():
        data = json.loads((AKI / role['folder'] / 'Data/DT_SkillInfo.json').read_text(encoding='utf-8'))
        table = next(o for o in data if o.get('Type') == 'DataTable')
        rows = []
        for row_name, raw in table['Rows'].items():
            fields = {strip_key(k): v for k, v in raw.items()}
            start = [int(b) for b in fields.get('SkillStartBuff', [])]
            end = [int(b) for b in fields.get('SkillEndBuff', [])]
            rows.append({'Name': row_name, 'SkillStartBuffs': [b for b in start if b in buff_ids],
                         'SkillEndBuffs': [b for b in end if b in buff_ids]})
        out[name] = rows
    return out


def main() -> int:
    sys.stdout.reconfigure(encoding='utf-8')
    errors: list[str] = []
    buffs = [convert_buff(b, errors) for b in BUFF_IDS]
    known = {b['id'] for b in buffs}
    for b in buffs:
        for ref in b['removeBuffs'] + b['addBuffs'] + b['expireBuffs']['normal'] + b['expireBuffs']['premature']                 + (b['attributeEvent']['addBuffs'] if b['attributeEvent'] else []):
            if ref not in known:
                errors.append(f"buff {b['id']} refers to buff {ref}, which is not imported")
    if errors:
        print('\n'.join(errors))
        return 1
    textures = ke.Textures()
    cues = convert_cues(sorted({c for b in buffs for c in b['cueIds']}), textures)
    notifies = montage_notifies(known)
    recipe = {
        'source': 'db_buff.db + db_buffeffect.db (ArtSource/Combat/prepare_buffs.py)',
        'buffTable': '/Game/Game/Combat/DT_Buff',
        'cueTable': '/Game/Game/Combat/DT_BuffCue',
        'iconDir': '/Game/LGUI/HUD/Buff/Icons',
        'buffs': buffs,
        'cues': cues,
        'notifies': notifies,
    }
    (HERE / 'buffs.recipe.json').write_text(json.dumps(recipe, ensure_ascii=False, indent=1), encoding='utf-8')
    print(f"buffs.recipe.json: {len(buffs)} buffs, {len(cues)} UI cues, {len(notifies)} notifies")
    for n in notifies:
        if 'tag' in n:
            print(f"  window {n['montage']} @ {n['time']:.3f}s +{n['duration']:.3f}s tag {n['tag']}")
        else:
            print(f"  notify {n['montage']} @ {n['time']:.3f}s add {n['buffId']}")
    for name, rows in skill_start_buffs(known).items():
        table = ROLES[name]['skillTable']
        (HERE / f'{name}.skill_start_buffs.json').write_text(json.dumps(
            {'table': table, 'source': f"{ROLES[name]['folder']}/Data/DT_SkillInfo SkillStartBuff / SkillEndBuff", 'rows': rows},
            ensure_ascii=False, indent=1), encoding='utf-8')
        print(f'{name}.skill_start_buffs.json: ' + ', '.join(f"{r['Name']}={r['SkillStartBuffs']}" for r in rows if r['SkillStartBuffs'])
              + ' | end: ' + ', '.join(f"{r['Name']}={r['SkillEndBuffs']}" for r in rows if r['SkillEndBuffs']))
    return 0


if __name__ == '__main__':
    sys.exit(main())
