"""特效管线的角色配置：原作导出路径、项目蒙太奇、输出目录。

脚本用 `--character <名字>` 选角色（默认 jiabei），嘉贝莉娜的输出位置保持原样；其他角色输出到 <角色目录>/。
网格几何（Geometry/）、贴图缓存（Textures/）和 Evidence/MasterDefaults.json 是原作共用资源，所有角色共用。
"""
from __future__ import annotations

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ANIM_ROOT = 'Client/Content/Aki/Character/Role/FemaleXL/'


def _stages(montages: str, table: dict[str, tuple[list[str], str]]) -> dict:
    return {stage: {'sources': sources, 'montage': montages + montage} for stage, (sources, montage) in table.items()}


CHARACTERS = {
    'jiabei': {
        'name': 'jiabei', 'title': 'Jiabei', 'cn': '嘉贝莉娜',
        'out': HERE, 'evidence': HERE / 'Evidence', 'tuning': HERE / 'JiabeiTuning.json',
        'role': ANIM_ROOT + 'Jiabeilina/CommonAnim/',
        'niagara': 'Client/Content/Aki/Effect/Niagara/NI_R2T1CalbrenaMd10011/',
        'dataAssets': 'Client/Content/Aki/Effect/DataAsset/Niagara/R2T1CalbrenaMd10011/',
        'stages': _stages('/Game/Characters/Role/jiabei/AnimMontage/', {
            'Attack01': (['Attack01'], 'AM_Jiabei_Attack01'),
            'Attack02': (['Attack02'], 'AM_Jiabei_Attack02'),
            'Attack03': (['Attack03'], 'AM_Jiabei_Attack03'),
            'Attack04': (['Attack04'], 'AM_Jiabei_Attack04'),
            # 攻击中按 Shift：原作 AM_Charge（序列 Attack_charge），蒙太奇本身也挂了残影 / 切换特效
            'Charge': (['Attack_charge', 'AM_Charge'], 'AM_Jiabei_Charge'),
            # 重击 1~3（长按普攻 1208016~1208018，WuwaRoleSkillSetup 按原作同名建）：原作 AM_HeavyAttack_Human0N 播序列 Attack_H_020N
            **{f'Heavy0{i}': ([f'Attack_H_020{i}', f'AM_HeavyAttack_Human0{i}'], f'AM_Jiabei_HeavyAttack_Human0{i}') for i in (1, 2, 3)},
            # 空中下落攻击 1208008：原作 AM_AirAttack_Start / Loop / End（序列 AirAttack_Start / AirAttack_Loop_1 / AirAttack_End）
            'AirAttack_Start': (['AirAttack_Start', 'AM_AirAttack_Start'], 'AM_Jiabei_AirAttack_Start'),
            'AirAttack_Loop': (['AirAttack_Loop_1', 'AM_AirAttack_Loop'], 'AM_Jiabei_AirAttack_Loop'),
            'AirAttack_End': (['AirAttack_End', 'AM_AirAttack_End'], 'AM_Jiabei_AirAttack_End'),
            # 变身（WuwaRoleSkillSetup 按原作同名建）：强化 E 1208101、E 跳踢两段 1208007 / 1208027（原作在 CommonAnim/Jump_Attack）、
            # EX 普攻 1~4 / 4 衔接版 / 爆蛇眼（序列 Attack05~08 / Attack08_H / Attack_H_01）、EX 重击 1~3（和重击同序列，蒙太奇不同）、大招
            'Skill01': (['Skill01', 'AM_Skill01'], 'AM_Jiabei_Skill01'),
            'Jump_Start': (['Attack_Jump_Start', 'Jump_Attack/AM_Attack_Jump_start'], 'AM_Jiabei_Attack_Jump_start'),
            'Jump_Next': (['Attack_Jump_End02', 'Jump_Attack/AM_Attack_Jump_next'], 'AM_Jiabei_Attack_Jump_next'),
            'AttackEx01': (['Attack05', 'AM_Attack01_EX'], 'AM_Jiabei_Attack01_EX'),
            'AttackEx02': (['Attack06', 'AM_Attack02_EX'], 'AM_Jiabei_Attack02_EX'),
            'AttackEx03': (['Attack07', 'AM_Attack03_EX'], 'AM_Jiabei_Attack03_EX'),
            'AttackEx04': (['Attack08', 'AM_Attack04_EX'], 'AM_Jiabei_Attack04_EX'),
            'AttackEx04H': (['Attack08_H', 'AM_Attack04_EXH'], 'AM_Jiabei_Attack04_EXH'),
            'AttackExFinisher': (['Attack_H_01', 'AM_Attack05_1_N'], 'AM_Jiabei_Attack05_1_N'),
            **{f'HeavyEx0{i}': ([f'Attack_H_020{i}', f'AM_HeavyAttack_evil0{i}'], f'AM_Jiabei_HeavyAttack_evil0{i}') for i in (1, 2, 3)},
            'Burst': (['Burst01', 'AM_Burst'], 'AM_Jiabei_Burst'),
            # 大招特写镜头序列 SC_Jiabeilina_Burst01 里直接放的 Niagara（没有通知、没有 DA）：只建系统（配方 solo），由镜头序列配方引用
            'Burst_Sequence': ([], 'AM_Jiabei_Burst'),
            # 大招子弹（起爆 1208302002 / 变身版 2005、持续燃烧 2003 / 2006 及最后一下）的 子弹特效DA：没有通知，只建 Model 组（配方 solo），
            # 由 ArtSource/Bullet/prepare_bullets.py 写进子弹表的 Effects，子弹出生后在子弹位置播放
            'Burst_Bullet': ([], 'AM_Jiabei_Burst'),
        }),
        'bulletEffects': {'Burst_Bullet': ['/Game/Aki/Effect/EffectGroup/R2T1CalbrenaMd10011/DA_Fx_Group_R1b_Boom_Ex01.DA_Fx_Group_R1b_Boom_Ex01',
                                           '/Game/Aki/Effect/EffectGroup/R2T1CalbrenaMd10011/DA_Fx_Group_R1b_Boom_Ex02.DA_Fx_Group_R1b_Boom_Ex02',
                                           '/Game/Aki/Effect/EffectGroup/R2T1CalbrenaMd10011/Shouji/DA_Fx_Group_R1a_Shouji_02.DA_Fx_Group_R1a_Shouji_02']},
        'sequenceSystems': {'Burst_Sequence': ['NS_Fx_R1b_02', 'NS_Fx_R1b_05', 'NS_Fx_R1b_08', 'NS_Fx_R1b_09', 'NS_Fx_R1b_10', 'NS_Fx_R1b_Boom_01',
                                               'NS_Fx_R1a_08_Boom', 'NS_Fx_R1a_08_Boom_2', 'NS_Fx_R1a_08_Boom_3']},
    },
    'changli': {
        'name': 'changli', 'title': 'Changli', 'cn': '长离',
        'out': HERE / 'Changli', 'evidence': HERE / 'Changli' / 'Evidence', 'tuning': HERE / 'Changli' / 'ChangliTuning.json',
        'role': ANIM_ROOT + 'ChangLi/CommonAnim/',
        'niagara': 'Client/Content/Aki/Effect/Niagara/NI_R2T1ChangliMd10011/',
        'dataAssets': 'Client/Content/Aki/Effect/DataAsset/Niagara/R2T1ChangliMd10011/',
        # 原作长离的特效通知大多挂在蒙太奇上，第四段的序列也有；项目 AM_Attack05 播的是原作 Attack04_1。
        'stages': _stages('/Game/Characters/Role/changli/AnimMontage/', {
            'Attack01': (['Attack01', 'AM_Attack01'], 'AM_Attack01'),
            'Attack02': (['Attack02', 'AM_Attack02'], 'AM_Attack02'),
            'Attack03': (['Attack03', 'AM_Attack03'], 'AM_Attack03'),
            'Attack04': (['Attack04', 'AM_Attack04'], 'AM_Attack04'),
            'Attack05': (['Attack04_1', 'AM_Attack04_1'], 'AM_Attack05'),
            # 重击（长按普攻 1205006）：原作 AM_AttackL 播序列 Attack10
            'AttackL': (['Attack10', 'AM_AttackL'], 'AM_AttackL'),
            # 空中（WuwaRoleSkillSetup 按原作同名建的蒙太奇）：普攻 1~4、心眼·跃、下落攻击三段（空中闪避 AM_Jump_Second_F/B 只有 DA_Fx_Group_MoveChongci，组里没有 Niagara 可建）
            **{n: ([n, f'AM_{n}'], f'AM_{n}') for n in ['AirAttack01', 'AirAttack02', 'AirAttack03', 'AirAttack04', 'AirAttack05',
                                                       'AirAttack_Start', 'AirAttack_Loop', 'AirAttack_End',
                                                       # 共鸣技能 E 三段、大招、心眼·极（大招特写镜头里的特效在镜头序列里，另做）
                                                       'Skill01_Start', 'Skill01_Loop', 'Skill01_End', 'Burst01', 'Burst02']},
            # 大招特写镜头序列 SC_Changli_Burst01 里直接放的 Niagara（没有通知、没有 DA）：只建系统（配方 solo），由镜头序列配方引用
            'Burst01_Sequence': ([], 'AM_Burst01'),
        }),
        'sequenceSystems': {'Burst01_Sequence': ['NS_Fx_Changli_Burst03', 'NS_Fx_Changli_Burst04', 'NS_Fx_Changli_Burst05',
                                                 'NS_Fx_Changli_Burst06', 'NS_Fx_Changli_BurstBw01', 'NS_Fx_Changli_BurstBw03']},
    },
}


def select(argv: list[str] | None = None) -> tuple[dict, list[str]]:
    """从参数里取出 --character <名字>，返回 (配置, 其余参数)。"""
    args = list(sys.argv[1:] if argv is None else argv)
    name = 'jiabei'
    if '--character' in args:
        i = args.index('--character')
        name = args[i + 1]
        del args[i:i + 2]
    if name not in CHARACTERS:
        raise SystemExit(f'unknown character {name}; known: {", ".join(CHARACTERS)}')
    character = CHARACTERS[name]
    character['out'].mkdir(parents=True, exist_ok=True)
    character['evidence'].mkdir(parents=True, exist_ok=True)
    return character, args
