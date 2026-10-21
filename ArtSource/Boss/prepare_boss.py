"""Boss 的蒙太奇和技能表配方：从原作导出整理（不改原作数据），由 WuwaBossSetup 命令行读取。
  python -I prepare_boss.py fuludelisi
输出 <boss>.boss.json（不提交，原作数据）：
  anims    项目里的动画序列 + 原作是否开根运动（开了的由命令行打开并挂 UWuwaPromoteRootMotionModifier）；
  montages 按原作蒙太奇重建：插槽、片段（序列 / 起止时间 / 播放速率）、分段、混合时间。原作的通知（Ts* 系列）先不搬；
  skills   原作 DT_SkillInfo 里选中的行：技能 ID（行名）、名字、蒙太奇。GA 留空（怪物 AI 那一步再做）。
动画序列要先导入项目（Tools/Blender/uemodel_to_fbx.py --centimeters → ImportAssets），命名 <前缀><原作序列名>。
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
AKI = Path('C:/GamePakExtractor/Output/Exports/Client/Content/Aki')

BOSSES = {
    'fuludelisi': {
        'source': 'Character/Monster/Boss/MB1FuludelisiMd00401',
        'root': '/Game/Characters/Monster/Boss/Fuludelisi',
        'animPrefix': 'MB1FuludelisiMd00401_',
        'table': '/Game/Characters/Monster/Boss/Fuludelisi/Data/DT_SkillInfo_Fuludelisi',
        # 第一步只做一阶段的基础招式（没挂特殊 GA 的）+ 出生 / 入战；行名就是原作技能 ID
        'skills': [80025001, 80025002, 80025003, 80025004, 80025025, 80025026,
                   # 移动类：前走 / 后走 / 左走（原作蒙太奇 = 起步 + 循环 + 收步）、前闪 / 后闪 / 左闪、弹刀·后闪
                   80025022, 80025023, 80025024, 80025029, 80025030, 80025031, 80025032],
        # 只挂这些蒙太奇（其余的是技能后面接的攻击，攻击动画还没全部导入；导入后去掉这一项即可）
        'montageFilter': r'^(Walk_|Move_Limit_|AM_|Attack01_st1$|Attack02_Montage$|Attack03_st1$|Attack04_st1$|Born$|PatrolToFight$)',
        # 技能之外也要的蒙太奇：死亡、受击、格挡、瘫痪
        'extraMontages': ['AM_Death', 'AM_Behit_S_L', 'AM_Behit_S_R', 'AM_Behit_B_L', 'AM_Behit_B_R',
                          'AM_Behit_Block_L', 'AM_Behit_Block_R',
                          'AM_SM_Paralysis_Start', 'AM_SM_Paralysis_Loop', 'AM_SM_Paralysis_End'],
    },
}


def load(path: Path):
    return json.loads(path.read_text(encoding='utf-8'))


def field(row: dict, name: str):
    """DataTable 行的字段名带 GUID 后缀（SkillName_2_xxxx），按前缀取。"""
    return next((v for k, v in row.items() if k.split('_')[0] == name), None)


def montage_name(original: str) -> str:
    """项目命名：AM_ 前缀，去掉原作的 _Montage 后缀（Attack02_Montage → AM_Attack02）。"""
    name = original[:-len('_Montage')] if original.endswith('_Montage') else original
    return name if name.startswith('AM_') else 'AM_' + name


def montage(boss: dict, original: str, anims: dict) -> dict:
    data = load(AKI / boss['source'] / 'CommonAnim' / f'{original}.json')
    m = next(o for o in data if o['Type'] == 'AnimMontage')
    p = m['Properties']
    tracks = []
    for track in p.get('SlotAnimTracks', []):
        segments = []
        for s in track['AnimTrack']['AnimSegments']:
            seq = s['AnimReference']['ObjectName'].split("'")[1]
            anims.setdefault(seq, None)
            segments.append({'anim': f"{boss['root']}/CommonAnim/{boss['animPrefix']}{seq}",
                             'start': s.get('AnimStartTime', 0.0), 'end': s.get('AnimEndTime', 0.0),
                             'rate': s.get('AnimPlayRate', 1.0), 'startPos': s.get('StartPos', 0.0)})
        tracks.append({'slot': track.get('SlotName', 'DefaultSlot'), 'segments': segments})
    return {
        'name': montage_name(original), 'source': f"{boss['source']}/CommonAnim/{original}",
        'tracks': tracks,
        'sections': [{'name': c['SectionName'], 'time': c.get('SegmentBeginTime', 0.0), 'next': c.get('NextSectionName', 'None')}
                     for c in p.get('CompositeSections', [])],
        # 原作没写的就是引擎默认值（0.25）
        'blendIn': p.get('BlendIn', {}).get('BlendTime', 0.25),
        'blendOut': p.get('BlendOut', {}).get('BlendTime', 0.25),
        'blendOutTriggerTime': p.get('BlendOutTriggerTime', -1.0),
        'rate': p.get('RateScale', 1.0),
    }


def main(name: str) -> int:
    sys.stdout.reconfigure(encoding='utf-8')
    boss = BOSSES[name]
    rows = load(AKI / boss['source'] / 'Data' / 'DT_SkillInfo.json')[0]['Rows']
    anims: dict[str, bool | None] = {}
    montages: dict[str, dict] = {}
    skills = []
    for skill_id in boss['skills']:
        row = rows[str(skill_id)]
        names = []
        for ref in field(row, 'Animations') or []:
            original = ref['AssetPathName'].rsplit('.', 1)[1]
            if 'montageFilter' in boss and not re.match(boss['montageFilter'], original):
                print(f'  skip {original} (skill {skill_id}): not in montageFilter')
                continue
            montages.setdefault(original, montage(boss, original, anims))
            names.append(f"{boss['root']}/AnimMontage/{montage_name(original)}")
        ga = (field(row, 'SkillGA') or {}).get('AssetPathName', 'None')
        skills.append({'id': skill_id, 'name': field(row, 'SkillName'), 'montages': names,
                       'originalGA': None if ga == 'None' else ga})
    for original in boss['extraMontages']:
        montages.setdefault(original, montage(boss, original, anims))
    # 原作序列是否开根运动
    for seq in anims:
        data = load(AKI / boss['source'] / 'CommonAnim' / f'{seq}.json')
        p = next(o for o in data if o['Type'] == 'AnimSequence')['Properties']
        anims[seq] = bool(p.get('bEnableRootMotion', False))
    recipe = {
        'schemaVersion': 1, 'boss': name, 'table': boss['table'], 'montageFolder': f"{boss['root']}/AnimMontage",
        'anims': [{'asset': f"{boss['root']}/CommonAnim/{boss['animPrefix']}{seq}", 'source': seq, 'rootMotion': root}
                  for seq, root in sorted(anims.items())],
        'montages': list(montages.values()),
        'skills': skills,
        'notes': [
            'Montage notifies (Ts* audio/rotate/position/effect/bullet) are not ported yet; bullets come with the bullet-notify step.',
            'Skills have no GA yet; the original GA (if any) is kept in originalGA for the monster-AI step.',
        ],
    }
    out = HERE / f'{name}.boss.json'
    out.write_text(json.dumps(recipe, ensure_ascii=False, indent=1), encoding='utf-8')
    print(f'{out}: {len(recipe["anims"])} anims, {len(montages)} montages, {len(skills)} skills')
    for s in skills:
        print(' ', s['id'], s['name'], [m.rsplit('/', 1)[1] for m in s['montages']], s['originalGA'] or '')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else 'fuludelisi'))
