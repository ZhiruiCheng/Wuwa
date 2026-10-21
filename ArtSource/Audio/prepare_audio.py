"""原作攻击音频 → 重建配方（不提交，原作数据）。
  python -I prepare_audio.py --character changli

数据来源（都由 FModel 导出到 C:/GamePakExtractor/Output/Exports/Client/Content/Aki）：
  - 动画上的音频通知：动画序列的 TsAnimNotifyStateAudioEvent（动作音效）、蒙太奇的 TsAnimNotifyAudioEvent（喊招语音）；
    特效组里的 EffectModelAudio（DA_Au_*，命中声等）。
  - WwiseAudio/Events/<事件>.json：事件用到的媒体 ID 与原文件名（DebugName）。
  - WwiseAudio_Generated/Event/<事件>.bnk：事件的层级结构（HIRC），用 wwiser 导出 XML 解析。
  - WwiseAudio_Generated/Media/<ID>.wem：Wwise Opus 编码的音频，用 vgmstream 解码成 wav。

原作的 SoundBank 是旧版 Wwise 生成的，项目的 Wwise 2025.1 读不了，所以按 HIRC 在项目自己的 Wwise 工程里同名重建
（build_wwise.py 读本配方）。语音事件里按 State 切换语言的 Switch 容器折叠成中文那一支（项目只用中文语音）。
没有重建的部分（总线、效果器、RTPC、HDR、State）记在 notes 里。
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

HERE = Path(__file__).resolve().parent
AKI = Path('C:/GamePakExtractor/Output/Exports/Client/Content/Aki')
EVENT_JSON = AKI / 'WwiseAudio/Events'
GENERATED = AKI / 'WwiseAudio_Generated'
TOOLS = Path('C:/GamePakExtractor/Tools')
VGMSTREAM = TOOLS / 'vgmstream/vgmstream-cli.exe'
WWISER = TOOLS / 'wwiser/wwiser.pyz'
DECODED = Path('C:/GamePakExtractor/Output/Decoded/WwiseMedia')
# 命令行版 FModel（C:/GamePakExtractor/Tools/PakExport，CUE4Parse + FModel 的设置）：缺的导出文件自动从游戏 pak 里取
PAKEXPORT = TOOLS / 'PakExport/bin/out/PakExport.exe'
LANGUAGE = 'zh'
# 运行时要切换的开关组：重建成 Wwise Switch 容器（其余开关折叠到默认值）。名字来自原作代码（RoleAudioComponent.js）。
KEEP_SWITCH_GROUPS = {'footstep_variant', 'foley_variant'}
# 原作 E_FootstepVariant → footstep_variant 开关值（RoleAudioComponent.js 的 footstepVariantMap）
FOOTSTEP_VARIANTS = ['land', 'run', 'runstop', 'sprint', 'sprintstop', 'walk', 'walkstop', 'turnback']
# 原作 E_FoleyVariant → foley_variant 开关值（RoleAudioComponent.js 的 foleyVariantMap，只有 0~7）
FOLEY_VARIANTS = ['bodyfall', 'fly', 'run', 'sprint', 'hard', 'hardfast', 'weak', 'weakfast']
SWITCH_NAMES = ['footstep_texture', 'footstep_variant', 'role_name', 'foley_variant', *FOOTSTEP_VARIANTS, *FOLEY_VARIANTS,
                'DirtSurface', 'SnowSurface', 'GrassSurface', 'WaterSurface', 'SandSurface', 'IceSurface', 'FabricSurface',
                'GlassSurface', 'ConcreteSurface', 'StoneSurface', 'WoodSurface', 'MetalSurface']


def wwise_id(name: str) -> int:
    """Wwise 短 ID：小写名字的 32 位 FNV-1 哈希。"""
    h = 2166136261
    for b in name.lower().encode():
        h = (h * 16777619) & 0xFFFFFFFF
        h ^= b
    return h


NAME_BY_ID = {str(wwise_id(n)): n for n in SWITCH_NAMES}
VOICE_PREFIX = re.compile(r'^(zh|ja|en|ko)_vo_', re.I)

CHARACTERS = {
    'changli': {
        'title': 'Changli',
        'anim': 'Character/Role/FemaleXL/ChangLi/CommonAnim',
        # 原作资产 → 项目资产（与特效配方 characters.py 的阶段一致：项目 AM_Attack05 用原作 AM_Attack04_1）
        'assets': {
            'Attack01': '/Game/Characters/Role/changli/NewModel/changli2_0R2T1ChangLiMd10011_LOD0_Skeleton_Attack01',
            'Attack02': '/Game/Characters/Role/changli/NewModel/changli2_0R2T1ChangLiMd10011_LOD0_Skeleton_Attack02',
            'Attack03': '/Game/Characters/Role/changli/NewModel/changli2_0R2T1ChangLiMd10011_LOD0_Skeleton_Attack03',
            'Attack04': '/Game/Characters/Role/changli/NewModel/changli2_0R2T1ChangLiMd10011_LOD0_Skeleton_Attack04',
            'AM_Attack01': '/Game/Characters/Role/changli/AnimMontage/AM_Attack01',
            'AM_Attack02': '/Game/Characters/Role/changli/AnimMontage/AM_Attack02',
            'AM_Attack03': '/Game/Characters/Role/changli/AnimMontage/AM_Attack03',
            'AM_Attack04': '/Game/Characters/Role/changli/AnimMontage/AM_Attack04',
            'AM_Attack04_1': '/Game/Characters/Role/changli/AnimMontage/AM_Attack05',
            # 冲刺（原作 AM_Move_F / AM_Move_B，项目手搭的同长度蒙太奇）：冲刺语音 com_dash + 音效
            'AM_Move_F': '/Game/Characters/Role/changli/AnimMontage/AM_Changli_Dash_F',
            'AM_Move_B': '/Game/Characters/Role/changli/AnimMontage/AM_Changli_Dash_B',
            # 重击（长按普攻 1205006）：原作 AM_AttackL 播序列 Attack10
            'Attack10': '/Game/Characters/Role/changli/NewModel/changli2_0R2T1ChangLiMd10011_LOD0_Skeleton_Attack10',
            'AM_AttackL': '/Game/Characters/Role/changli/AnimMontage/AM_AttackL',
            # 空中：普攻 1~4、心眼·跃、下落攻击三段、空中闪避（WuwaRoleSkillSetup 按原作同名建的蒙太奇 + NewModel 动画）
            **{n: f'/Game/Characters/Role/changli/NewModel/changli2_0R2T1ChangLiMd10011_LOD0_Skeleton_{n}'
               for n in ['AirAttack01', 'AirAttack02', 'AirAttack03', 'AirAttack04', 'AirAttack05',
                         'AirAttack_Start', 'AirAttack_Loop', 'AirAttack_End', 'Skill01_Start', 'Skill01_Loop', 'Skill01_End', 'Burst01', 'Burst02']},
            **{n: f'/Game/Characters/Role/changli/AnimMontage/{n}'
               for n in ['AM_AirAttack01', 'AM_AirAttack02', 'AM_AirAttack03', 'AM_AirAttack04', 'AM_AirAttack05',
                         'AM_AirAttack_Start', 'AM_AirAttack_Loop', 'AM_AirAttack_End', 'AM_Jump_Second_F', 'AM_Jump_Second_B',
                         'AM_Skill01_Start', 'AM_Skill01_Loop', 'AM_Skill01_End', 'AM_Burst01', 'AM_Burst02']},
        },
        'effectAudio': 'Effect/EffectAudio/Role/Changli',
        # 原作 db_audio.db roleskinaudio 81001205：FootstepEvent role_footstep_heels
        'footstep': {'event': 'role_footstep_heels', 'roots': ['Characters/Role/changli']},
        # 同一行配置的 FoleyEvent（衣服拟音，只有长离用）
        'foley': {'event': 'role_foley_anke', 'roots': ['Characters/Role/changli']},
        'extraEvents': ['role_foley_anke'],
    },
    'jiabei': {
        'title': 'Jiabei',
        'anim': 'Character/Role/FemaleXL/Jiabeilina/CommonAnim',
        # 嘉贝的音频通知都在蒙太奇上（动画序列上没有）；项目蒙太奇与特效配方 characters.py 的阶段一致。
        'assets': {
            'AM_Attack01_N_S': '/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_Attack01',
            'AM_Attack02_N': '/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_Attack02',
            'AM_Attack03_N': '/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_Attack03',
            'AM_Attack04_N': '/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_Attack04',
            'AM_Charge': '/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_Charge',
            # 冲刺（原作 AM_Move_F / AM_Move_B，项目手搭的同长度蒙太奇）：冲刺语音 com_dash + 音效
            'AM_Move_F': '/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_Dash_F',
            'AM_Move_B': '/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_Dash_B',
            # 空中闪避（通用技能 100003 / 100004）
            'AM_Jump_Second_F': '/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_Jump_Second_F',
            'AM_Jump_Second_B': '/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_Jump_Second_B',
            # 重击 1~3（1208016~1208018，WuwaRoleSkillSetup 按原作同名建）
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
        'effectAudio': 'Effect/EffectAudio/Role/jiabeilina',
        # 原作 db_audio.db roleskinaudio 81001208：FootstepEvent role_footstep_heels
        'footstep': {'event': 'role_footstep_heels', 'roots': ['Characters/Role/jiabei']},
        'foley': {'event': 'role_foley_kelaita', 'roots': ['Characters/Role/jiabei']},
        'extraEvents': ['role_foley_kelaita'],
    },
    # 多个角色共用的事件只建一次（Wwise 事件名全局唯一）：脚步声 role_footstep_heels 等。角色配方里只记通知。
    'common': {
        'title': 'Common',
        'assets': {},
        'extraEvents': ['role_footstep_heels'],
    },
}

def pak_export(mode: str, packages: list[str]) -> None:
    """mode = json / raw；packages 是 Aki 下的相对路径（json 不带扩展名，raw 带扩展名），一次进程导一批。"""
    if not packages:
        return
    patterns = [f'Client/Content/Aki/{p}.uasset' if mode == 'json' else f'Client/Content/Aki/{p}' for p in packages]
    result = subprocess.run([str(PAKEXPORT), mode, *patterns], capture_output=True, text=True, encoding='utf-8', errors='replace')
    if result.returncode != 0:
        raise RuntimeError(f'PakExport {mode} failed: ' + result.stderr[-2000:])


def ensure_json(packages: list[str]) -> None:
    pak_export('json', [p for p in packages if not (AKI / f'{p}.json').exists()])


def ensure_raw(files: list[str]) -> None:
    pak_export('raw', [f for f in files if not (AKI / f).exists()])


# HIRC 类型 → 重建时的 Wwise 对象类型
KINDS = {'CAkSound': 'Sound', 'CAkRanSeqCntr': 'RandomSequenceContainer', 'CAkSwitchCntr': 'SwitchContainer',
         'CAkActorMixer': 'ActorMixer', 'CAkLayerCntr': 'BlendContainer'}
# 重建时照抄的属性（HIRC 属性名 → Wwise 属性名）
PROPS = {'Volume': 'Volume', 'MakeUpGain': 'MakeUpGain', 'Pitch': 'Pitch', 'LPF': 'Lowpass', 'HPF': 'Highpass',
         'BusVolume': 'BusVolume', 'InitialDelay': 'InitialDelay'}


def fmt_name(field: ET.Element) -> str:
    """valuefmt 里方括号中的名字，例如 '0x00 [Volume]' → 'Volume'。"""
    m = re.search(r'\[([^\]]+)\]', field.get('valuefmt') or '')
    return m.group(1) if m else (field.get('valuefmt') or field.get('value'))


def first(node: ET.Element, name: str, default=None):
    for f in node.iter('field'):
        if f.get('name') == name:
            return f.get('value')
    return default


def child_object(node: ET.Element, name: str) -> ET.Element | None:
    for o in node.iter('object'):
        if o.get('name') == name:
            return o
    return None


def prop_bundle(node: ET.Element) -> tuple[dict, dict]:
    """节点自身（NodeInitialParams / ActionInitialValues）的属性与随机范围，不进入子对象。"""
    props, ranged = {}, {}
    for bundle in node.iter('object'):
        name = bundle.get('name') or ''
        if name.startswith('AkPropBundle<AkPropValue'):
            for item in bundle.iter('object'):
                if item.get('name') == 'AkPropBundle':
                    pid = next(f for f in item if f.get('name') == 'pID')
                    val = next(f for f in item if f.get('name') == 'pValue')
                    props[fmt_name(pid)] = float(val.get('value'))
        elif name.startswith('AkPropBundle<RANGED_MODIFIERS'):
            for item in bundle.iter('object'):
                if item.get('name') == 'AkPropBundle':
                    fields = {f.get('name'): f for f in item.iter('field')}
                    ranged[fmt_name(fields['pID'])] = [float(fields['min'].get('value')), float(fields['max'].get('value'))]
    return props, ranged


def bank_xml(event: str, work: Path) -> ET.Element:
    out = work / f'{event}.xml'
    if not out.exists():
        ensure_raw([f'WwiseAudio_Generated/Event/{event}.bnk'])
        bank = GENERATED / 'Event' / f'{event}.bnk'
        subprocess.run([sys.executable, '-I', str(WWISER), '-d', 'xml', '-dn', str(out.with_suffix('')), str(bank)],
                       check=True, cwd=work, capture_output=True)
    return ET.parse(out).getroot()


def parse_bank(root: ET.Element) -> tuple[dict, dict, dict]:
    """HIRC → (节点, 动作, 事件)，键为原作对象 ID。"""
    nodes, actions, events = {}, {}, {}
    items = root.find(".//list[@name='listLoadedItem']")
    for o in items:
        cls = o.get('name')
        oid = first(o, 'ulID')
        if cls in KINDS:
            base = child_object(o, 'NodeBaseParams')
            initial = child_object(base, 'NodeInitialParams')
            props, ranged = prop_bundle(initial)
            positioning = child_object(base, 'PositioningParams')
            pos = {f.get('name'): f.get('value') for f in positioning.iter('field')}
            node = {'id': oid, 'kind': KINDS[cls], 'parent': first(base, 'DirectParentID'),
                    'props': props, 'ranged': ranged,
                    'positioning': {k: pos[k] for k in ('bPositioningInfoOverrideParent', 'bHasListenerRelativeRouting',
                                                        'e3DPositionType', 'eSpatializationMode', 'bEnableAttenuation') if k in pos},
                    'overrideBus': first(base, 'OverrideBusId'), 'children': []}
            children = child_object(o, 'Children')
            if children is not None:
                node['children'] = [f.get('value') for f in children.iter('field') if f.get('name') == 'ulChildID']
            if cls == 'CAkSound':
                source = child_object(o, 'AkBankSourceData')
                node['media'] = first(source, 'sourceID')
                node['streaming'] = 'Streaming' in (next(f for f in source.iter('field') if f.get('name') == 'StreamType').get('valuefmt') or '')
                node['loop'] = props.pop('Loop', None)
            elif cls == 'CAkRanSeqCntr':
                fields = {f.get('name'): f for f in o.iter('field')}
                node['container'] = {
                    'mode': fmt_name(fields['eMode']), 'randomMode': fmt_name(fields['eRandomMode']),
                    'avoidRepeatCount': int(fields['wAvoidRepeatCount'].get('value')),
                    'loopCount': int(fields['sLoopCount'].get('value')),
                    'continuous': first(o, 'bIsContinuous', '0') == '1',
                    'resetPlaylistEachPlay': first(o, 'bResetPlayListAtEachPlay', '0') == '1',
                    'weights': {f.get('value'): None for f in o.iter('field') if f.get('name') == 'ulPlayID'},
                }
                plays = [f for f in o.iter('field') if f.get('name') in ('ulPlayID', 'weight')]
                for play, weight in zip(plays[0::2], plays[1::2]):
                    node['container']['weights'][play.get('value')] = int(weight.get('value'))
            elif cls == 'CAkSwitchCntr':
                packages = {}
                for pk in o.iter('object'):
                    if pk.get('name') == 'CAkSwitchPackage':
                        packages[first(pk, 'ulSwitchID')] = [f.get('value') for f in pk.iter('field') if f.get('name') == 'NodeID']
                node['switch'] = {'groupType': fmt_name(next(f for f in o.iter('field') if f.get('name') == 'eGroupType')),
                                  'group': first(o, 'ulGroupID'), 'default': first(o, 'ulDefaultSwitch'), 'packages': packages}
            if 'AttenuationID' in props:
                node['attenuation'] = str(int(props.pop('AttenuationID')))
            nodes[oid] = node
        elif cls and cls.startswith('CAkAction'):
            props, ranged = prop_bundle(o)
            actions[oid] = {'id': oid, 'type': fmt_name(next(f for f in o.iter('field') if f.get('name') == 'ulActionType')),
                            'target': first(o, 'idExt'), 'props': props, 'ranged': ranged,
                            'fadeCurve': first(o, 'eFadeCurve')}
        elif cls == 'CAkEvent':
            events[oid] = [f.get('value') for f in o.iter('field') if f.get('name') == 'ulActionID']
        elif cls == 'CAkAttenuation':
            curves = []
            for table in o.iter('object'):
                if table.get('name') == 'CAkConversionTable':
                    pts = [(float(p[0].get('value')), float(p[1].get('value')), fmt_name(p[2]))
                           for p in ([f for f in pt if f.tag == 'field'] for pt in table.iter('object') if pt.get('name') == 'AkRTPCGraphPoint')]
                    curves.append({'scaling': fmt_name(next(f for f in table if f.get('name') == 'eScaling')), 'points': pts})
            use = [int(f.get('value')) for f in o.iter('field') if (f.get('name') or '').startswith('curveToUse[')]
            nodes[oid] = {'id': oid, 'kind': 'Attenuation', 'curveToUse': use, 'curves': curves}
    return nodes, actions, events


def media_names(event: str) -> dict:
    ensure_json([f'WwiseAudio/Events/{event}'])
    d = json.loads((EVENT_JSON / f'{event}.json').read_text(encoding='utf-8'))[0]
    out = {}
    for lm in d['EventCookedData']['EventLanguageMap']:
        for m in lm['Value']['Media']:
            out[str(m['MediaId'])] = m['DebugName'].replace(chr(92), '/').rsplit('/', 1)[-1].rsplit('.', 1)[0]
    return {'media': out, 'maxDuration': d.get('MaximumDuration'), 'minDuration': d.get('MinimumDuration'),
            'infinite': d.get('IsInfinite'), 'maxAttenuationRadius': d.get('MaxAttenuationRadius')}


def subtree_media(nodes: dict, nid: str) -> list[str]:
    n = nodes.get(nid)
    if not n:
        return []
    if n['kind'] == 'Sound':
        return [n['media']]
    return [m for c in n['children'] for m in subtree_media(nodes, c)]


def collapse_language(nodes: dict, names: dict, notes: list) -> None:
    """按 State 切换语言的 Switch 容器 → 中文那一支（子树的媒体都是 zh_vo_*）；其他语言的子树删掉。"""
    for nid, n in list(nodes.items()):
        if n.get('kind') != 'SwitchContainer':
            continue
        langs = {}
        for c in n['children']:
            prefixes = {VOICE_PREFIX.match(names.get(m, '')).group(1).lower() for m in subtree_media(nodes, c)
                        if VOICE_PREFIX.match(names.get(m, ''))}
            if len(prefixes) == 1:
                langs[prefixes.pop()] = c
        group_name = NAME_BY_ID.get(n['switch']['group'])
        if group_name in KEEP_SWITCH_GROUPS:
            # 保留：开关组 / 值按 FNV 哈希反查成名字，Wwise 里重建成 Switch 容器
            n['switch']['groupName'] = group_name
            n['switch']['defaultName'] = NAME_BY_ID.get(n['switch']['default'])
            n['switch']['assignments'] = {NAME_BY_ID.get(value, value): nodes_ for value, nodes_ in n['switch']['packages'].items() if nodes_}
            continue
        if LANGUAGE in langs and len(langs) >= 2:
            keep = langs[LANGUAGE]
            n['collapsedFrom'] = f"{n['switch']['groupType']} group {n['switch']['group']} over {sorted(langs)}: kept {LANGUAGE}"
            notes.append(f'Voice language State switch collapsed to "{LANGUAGE}" (the project ships Chinese voice only).')
        else:
            # 其他开关（游戏 Switch，组名是哈希、含义不明）：取包里写明的默认值对应的分支
            default = n['switch']['packages'].get(n['switch']['default']) or []
            if len(default) > 1:
                # 默认值下挂了好几个子节点（同时播）：保留成 Switch 容器，只留默认值这一支（游戏不设这个开关，就按默认值播这几个）
                group = n['switch']['group']
                n['switch']['groupName'] = NAME_BY_ID.get(group, f'Switch_{group}')
                n['switch']['defaultName'] = NAME_BY_ID.get(n['switch']['default'], f"Value_{n['switch']['default']}")
                n['switch']['assignments'] = {n['switch']['defaultName']: default}
                for c in [c for c in n['children'] if c not in default]:
                    drop = [c]
                    while drop:
                        d = drop.pop()
                        drop += nodes.get(d, {}).get('children', [])
                        nodes.pop(d, None)
                n['children'] = list(default)
                notes.append(f'Switch {nid}: default value has {len(default)} nodes (played together); kept as a switch with only the default value.')
                continue
            if len(default) != 1:
                notes.append(f'Switch {nid}: default value {n["switch"]["default"]} has {len(default)} nodes; kept as a switch (not rebuilt).')
                continue
            keep = default[0]
            n['collapsedFrom'] = f"{n['switch']['groupType']} group {n['switch']['group']}: kept default value {n['switch']['default']}"
            label = NAME_BY_ID.get(n['switch']['group'], f"{n['switch']['group']} (name unknown, hashed)")
            value = NAME_BY_ID.get(n['switch']['default'], n['switch']['default'])
            notes.append(f'Switch group {label} collapsed to its default value {value} as stored in the bank.')
        n['kind'] = 'Collapsed'
        for c in n['children']:
            if c == keep:
                continue
            drop = [c]
            while drop:
                d = drop.pop()
                drop += nodes.get(d, {}).get('children', [])
                nodes.pop(d, None)
        n['children'] = [keep]


def notifies(character: dict) -> list[dict]:
    """原作动画上的两种音频通知（时间是原作秒数；项目动画与原作同长，特效配方也按原作时间装）。"""
    out = []
    if not character['assets']:
        return out
    folder = AKI / character['anim']
    ensure_json([f"{character['anim']}/{asset}" for asset in character['assets']])
    for asset, project in character['assets'].items():
        objects = json.loads((folder / f'{asset}.json').read_text(encoding='utf-8'))
        by_name = {o.get('Name'): o for o in objects}
        anim = next(o for o in objects if o.get('Type') in ('AnimSequence', 'AnimMontage'))
        for n in anim['Properties'].get('Notifies', []):
            ref = ((n.get('Notify') or n.get('NotifyStateClass')) or {}).get('ObjectName', '')
            name = ref.split(':', 1)[-1].rstrip("'").rsplit('.', 1)[-1]
            obj = by_name.get(name)
            if not obj or obj.get('Type') not in ('TsAnimNotifyAudioEvent_C', 'TsAnimNotifyStateAudioEvent_C'):
                continue
            p = obj['Properties']
            event = p['AudioEvent']['AssetPathName'].rsplit('.', 1)[-1]
            entry = {'sourceAsset': asset, 'asset': project, 'kind': 'state' if 'State' in obj['Type'] else 'instant',
                     'event': event, 'time': float(n.get('LinkValue', 0.0)), 'duration': float(n.get('Duration', 0.0)),
                     'socket': p.get('SocketName'), 'follow': p.get('Follow', True)}
            if entry['kind'] == 'state':
                # 原作默认值（TsAnimNotifyStateAudioEvent.js）：FadeDuration 500ms、FadeCurve 4（线性）、KeepAlive false
                entry.update({'fadeMs': p.get('FadeDuration', 500), 'fadeCurve': p.get('FadeCurve', 4),
                              'keepAlive': p.get('KeepAlive', False),
                              'trailingEvent': (p.get('TrailingAudioEvent') or {}).get('AssetPathName', '').rsplit('.', 1)[-1] or None})
            out.append(entry)
    return out


def footstep_notifies(character: dict) -> list[dict]:
    """原作移动动画上的两种通知，都在女性体型共用的移动动画（FemaleXL/BaseAnim、BaseAnim3）里：
      - 脚步 TsAnimNotifyFootstepAudio：设 footstep_variant，播角色的 FootstepEvent；
      - 衣服拟音 TsAnimNotifyStateFoleyAudioEvent（持续）：设 foley_variant，播角色的 FoleyEvent，结束时淡出停止
        （TsAnimNotifyStateFoleyAudioEvent.js，默认 FadeDuration 500ms、FadeCurve 4）。原作映射表只有 0~7，
        动画里的 8 / 9 在原作里只报错、不改开关（沿用上次的值，AK 创建时是 weak），这里同样不设开关。
    项目里每个角色有自己的同名移动动画，按名字对应；两套候选（BaseAnim / BaseAnim3）都给命令行，按项目动画的实际时长选。"""
    footstep, foley = character.get('footstep'), character.get('foley')
    if not footstep and not foley:
        return []
    sources = {}
    for folder in ('BaseAnim', 'BaseAnim3'):
        for f in sorted((AKI / 'Character/Role/FemaleXL' / folder).glob('*.json')):
            text = f.read_text(encoding='utf-8')
            if 'TsAnimNotifyFootstepAudio' not in text and 'TsAnimNotifyStateFoleyAudioEvent' not in text:
                continue
            objects = json.loads(text)
            by_name = {o.get('Name'): o for o in objects}
            anim = next(o for o in objects if o.get('Type') == 'AnimSequence')
            steps, windows = [], []
            for n in anim['Properties'].get('Notifies', []):
                ref = ((n.get('Notify') or n.get('NotifyStateClass')) or {}).get('ObjectName', '')
                obj = by_name.get(ref.split(':', 1)[-1].rstrip("'").rsplit('.', 1)[-1])
                if not obj:
                    continue
                p = obj.get('Properties') or {}
                index = int(str(p.get('Variant', 'NewEnumerator0')).rsplit('NewEnumerator', 1)[-1])
                if obj.get('Type') == 'TsAnimNotifyFootstepAudio_C':
                    steps.append({'time': float(n.get('LinkValue', 0.0)), 'variant': FOOTSTEP_VARIANTS[index]})
                elif obj.get('Type') == 'TsAnimNotifyStateFoleyAudioEvent_C':
                    windows.append({'time': float(n.get('LinkValue', 0.0)), 'duration': float(n.get('Duration', 0.0)),
                                    'variant': FOLEY_VARIANTS[index] if index < len(FOLEY_VARIANTS) else None,
                                    'fadeMs': p.get('FadeDuration', 500), 'fadeCurve': p.get('FadeCurve', 4)})
            sources.setdefault(f.stem, {})[folder] = {'length': float(anim['Properties']['SequenceLength']),
                                                      'footstep': steps, 'foley': windows}
    names = sorted(sources, key=len, reverse=True)
    out = []
    for root in (footstep or foley)['roots']:
        for asset in sorted((HERE.parents[1] / 'Content' / root).rglob('*.uasset')):
            stem = asset.stem
            name = next((n for n in names if re.search(rf'(?:^|_){re.escape(n)}(?:_001|3)?$', stem)), None)
            if not name:
                continue
            hint = 'BaseAnim3' if (stem.endswith(name + '3') or 'jiabei3_0' in stem) else 'BaseAnim'
            path = '/Game/' + asset.relative_to(HERE.parents[1] / 'Content').with_suffix('').as_posix()
            # 两套候选都给命令行：按项目动画的实际时长选（比值 1，或整段拉长 1.25 倍——24 帧导入的 30 帧原作动画），
            # 时刻按比值缩放；都符合时优先比值 1，再优先名字提示的那套。
            for kind, config, group in (('footstep', footstep, 'footstep_variant'), ('foley', foley, 'foley_variant')):
                candidates = {folder: {'length': src['length'], 'steps': src[kind]} for folder, src in sources[name].items() if src[kind]}
                if config and candidates:
                    out.append({'sourceAsset': f'FemaleXL/*/{name}', 'asset': path, 'kind': kind, 'event': config['event'],
                                'time': 0.0, 'duration': 0.0, 'switchGroup': group, 'socket': None, 'follow': True,
                                'hint': hint, 'sources': candidates})
    return out


def effect_audio(character: dict) -> list[dict]:
    out = []
    if not list((AKI / character['effectAudio']).glob('DA_Au_*.json')):
        pak_export('json', [f"{character['effectAudio']}/DA_Au_*"])
    for f in sorted((AKI / character['effectAudio']).glob('DA_Au_*.json')):
        obj = json.loads(f.read_text(encoding='utf-8'))[0]
        p = obj.get('Properties', {})
        # 新导出写 ObjectPath（'.../play_x.0'），旧格式写 AssetPathName（'.../play_x.play_x'）
        ref = lambda key: (lambda r: (r.get('AssetPathName') or r.get('ObjectPath') or '').rsplit('/', 1)[-1].split('.', 1)[0] or None)(p.get(key) or {})
        out.append({'dataAsset': f.stem, 'event': ref('AudioEvent'), 'trailingEvent': ref('TrailingAudioEvent'),
                    'properties': {k: v for k, v in p.items() if k not in ('AudioEvent', 'TrailingAudioEvent', 'exportIndex')}})
    return out


def decode(media_id: str, name: str) -> str:
    DECODED.mkdir(parents=True, exist_ok=True)
    wav = DECODED / f'{name}.wav'
    if not wav.exists():
        wem = GENERATED / 'Media' / f'{media_id}.wem'
        if not wem.exists():
            raise FileNotFoundError(f'{wem} ({name}): export it from FModel')
        subprocess.run([str(VGMSTREAM), '-o', str(wav), str(wem)], check=True, capture_output=True)
    return str(wav)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--character', default='changli', choices=sorted(CHARACTERS))
    args = parser.parse_args()
    character = CHARACTERS[args.character]
    work = HERE / character['title'] / 'bank_xml'
    work.mkdir(parents=True, exist_ok=True)

    placements = notifies(character) + footstep_notifies(character)
    effects = effect_audio(character) if character.get('effectAudio') else []
    shared = {e for c in CHARACTERS.values() if c is not character for e in c.get('extraEvents', [])}
    names = sorted(({p['event'] for p in placements} | {e['event'] for e in effects if e['event']}
                    | {p['trailingEvent'] for p in placements if p.get('trailingEvent')} | set(character.get('extraEvents', []))) - shared)
    all_nodes, events, notes = {}, {}, []
    for event in names:
        nodes, actions, bank_events = parse_bank(bank_xml(event, work))
        info = media_names(event)
        collapse_language(nodes, info['media'], notes)
        ensure_raw([f"WwiseAudio_Generated/Media/{n['media']}.wem" for n in nodes.values() if n.get('kind') == 'Sound'])
        for n in nodes.values():
            if n.get('kind') == 'Sound':
                n['name'] = info['media'].get(n['media'], f"media_{n['media']}")
                n['wav'] = decode(n['media'], n['name'])
        for nid, n in nodes.items():
            all_nodes.setdefault(nid, n)
        event_id, action_ids = next(iter(bank_events.items()))
        events[event] = {'id': event_id, 'actions': [actions[a] for a in action_ids],
                         **{k: info[k] for k in ('maxDuration', 'minDuration', 'infinite', 'maxAttenuationRadius')}}
        for a in events[event]['actions']:
            if a['type'] not in ('Play', 'Stop_E_O'):
                notes.append(f'{event}: action {a["id"]} type {a["type"]} is not rebuilt (Play only).')
    # 父节点不在任何事件包里（总线、Init 包里的上层 Actor-Mixer）时，重建为根
    for n in all_nodes.values():
        if n.get('parent') and n['parent'] not in all_nodes:
            n['parentOutsideBanks'] = n['parent']
            n['parent'] = None
        if n.get('overrideBus') not in (None, '0'):
            notes.append(f'Node {n["id"]}: output bus override {n["overrideBus"]} (bus hierarchy is in Init.bnk) routed to the project Master Audio Bus.')
    notes.append('Not rebuilt: effects (Fx share sets), RTPC curves, State props, HDR settings of the original Actor-Mixers.')

    recipe = {'schemaVersion': 1, 'character': args.character, 'language': LANGUAGE,
              'nodes': all_nodes, 'events': events, 'notifies': placements, 'effectAudio': effects,
              'notes': sorted(set(notes))}
    out = HERE / character['title'] / 'audio.json'
    out.write_text(json.dumps(recipe, ensure_ascii=False, indent=1), encoding='utf-8')
    kinds = {}
    for n in all_nodes.values():
        kinds[n['kind']] = kinds.get(n['kind'], 0) + 1
    print(json.dumps({'recipe': str(out), 'events': len(events), 'nodes': kinds, 'notifies': len(placements),
                      'effectAudio': [e['dataAsset'] for e in effects]}, ensure_ascii=False))
    return 0


if __name__ == '__main__':
    sys.exit(main())
