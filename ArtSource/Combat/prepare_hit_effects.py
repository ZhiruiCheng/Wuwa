"""原作受击效果表 → 项目受击效果表（FWuwaHitEffectInfo）的导入配方；怪物另外把受击蒙太奇写进怪物配方。
  python -I prepare_hit_effects.py --character jiabei

数据来源：原作角色 Data/DT_HitEffect（行结构 SHitEffect），由 PakExport 导出成 JSON（缺的自动导出）。
行名 = 子弹表的 被击效果；表属于攻击者（原作 GetBulletHitData(攻击者, 名字)）。只取 FWuwaHitEffectInfo 里有的字段：
被击动作（EHitAnim）、受击朝向Z轴偏转、命中硬直时间、地面受击速度、地面受击移动时间（原作 CharacterHitComponent.A6r）。
空中受击、滞空、落地反弹、地面受击移动曲线 / 最大最小速度还没用到，不导入。

怪物（--character fuludelisi）：受击蒙太奇来自原作 AI 状态机（ConfigDB db_AiStateMachineConfig 的 aistatemachineconfig，JSON），
所有 TaskBeHitMontage 节点的 MontageMap（被击动作 → 蒙太奇名）合在一起，写进 <monster>.monster.json 的 beHitMontages，
同时写 hitEffectTable，由 WuwaMonsterSetup 写进蓝图。

输出 <character>.hit_effects.json，由 -run=WuwaDataTableSetup -Recipe=ArtSource/Combat/<character>.hit_effects.json -Apply 写进 DataTable。原作数据，不提交。
"""
from __future__ import annotations

import argparse
import json
import re
import sqlite3
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
AKI = Path('C:/GamePakExtractor/Output/Exports/Client/Content/Aki')
PAKEXPORT = Path('C:/GamePakExtractor/Tools/PakExport/bin/out/PakExport.exe')
STATE_MACHINE_DB = AKI / 'ConfigDB/db_AiStateMachineConfig.db'
STRUCT = '/Script/Wuwa.WuwaHitEffectInfo'

CHARACTERS = {
    'changli': {'source': 'Character/Role/FemaleXL/ChangLi/Data/DT_HitEffect',
                'table': '/Game/Characters/Role/changli/Data/DT_HitEffect_Changli'},
    'jiabei': {'source': 'Character/Role/FemaleXL/Jiabeilina/Data/DT_HitEffect',
               'table': '/Game/Characters/Role/jiabei/Data/DT_HitEffect_Jiabei'},
    'fuludelisi': {'source': 'Character/Monster/Boss/MB1FuludelisiMd00401/Data/DT_HitEffect',
                   'table': '/Game/Characters/Monster/Boss/Fuludelisi/Data/DT_HitEffect_Fuludelisi',
                   'stateMachine': 'SM_MB1FuludelisiMd00401',
                   'montageDir': '/Game/Characters/Monster/Boss/Fuludelisi/AnimMontage',
                   'recipe': HERE.parent / 'Boss/fuludelisi.monster.json'},
}

# 原作 EHitAnim（UserDefinedEnum，NewEnumerator<n> 的 n 就是值）→ EWuwaHitAnim 的名字
HIT_ANIMS = ['LightLeft', 'LightRight', 'HeavyLeft', 'HeavyRight', 'KnockUp', 'KnockDown', 'Suppress', 'Parried',
             'LightFront', 'LightBack', 'HeavyFront', 'HeavyBack', 'WeaknessBreak']


def ensure_json(package: str) -> Path:
    path = AKI / f'{package}.json'
    if not path.exists():
        result = subprocess.run([str(PAKEXPORT), 'json', f'Client/Content/Aki/{package}.uasset'],
                                capture_output=True, text=True, encoding='utf-8', errors='replace')
        if result.returncode != 0 or not path.exists():
            raise RuntimeError('PakExport json failed: ' + result.stderr[-2000:])
    return path


def short(key: str) -> str:
    """UserDefinedStruct 的属性名带 _<序号>_<GUID> 后缀。"""
    return re.sub(r'_\d+_[0-9A-F]{32}$', '', key)


def enum_value(text: str) -> int:
    match = re.search(r'NewEnumerator(\d+)$', text or '')
    if not match:
        raise ValueError(f'unexpected enum value {text!r}')
    return int(match.group(1))


def convert(name: str, raw: dict) -> dict:
    row = {short(k): v for k, v in raw.items()}
    speed = row['地面受击速度']
    return {
        'Name': name,
        'BeHitAnim': HIT_ANIMS[enum_value(row['被击动作'])],
        'RotationOffsetZ': row['受击朝向Z轴偏转'],
        'StiffTime': row['命中硬直时间'],
        'GroundSpeed': {'X': speed['X'], 'Y': speed['Y'], 'Z': speed['Z']},
        'GroundMoveTime': row['地面受击移动时间'],
    }


def hit_montages(state_machine: str) -> dict[int, str]:
    """AI 状态机里所有 TaskBeHitMontage 的 MontageMap 合并（同一个被击动作只会出现在一个节点里，重复就报错）。"""
    blob = sqlite3.connect(STATE_MACHINE_DB).execute('select BinData from aistatemachineconfig where Id=?', (state_machine,)).fetchone()
    if not blob:
        raise SystemExit(f'state machine {state_machine} not found')
    text = blob[0].decode('utf-8', 'ignore')
    config = json.JSONDecoder().raw_decode(text[text.find('{'):])[0]
    montages: dict[int, str] = {}

    def walk(node):
        if isinstance(node, dict):
            task = node.get('TaskBeHitMontage')
            if isinstance(task, dict):
                for anim, montage in task.get('MontageMap', []):
                    if montages.get(anim, montage) != montage:
                        raise SystemExit(f'hit anim {anim} maps to both {montages[anim]} and {montage}')
                    montages[anim] = montage
            for value in node.values():
                walk(value)
        elif isinstance(node, list):
            for value in node:
                walk(value)

    walk(config)
    return dict(sorted(montages.items()))


def main() -> int:
    sys.stdout.reconfigure(encoding='utf-8')
    parser = argparse.ArgumentParser()
    parser.add_argument('--character', required=True, choices=sorted(CHARACTERS))
    args = parser.parse_args()
    config = CHARACTERS[args.character]

    exports = json.loads(ensure_json(config['source']).read_text(encoding='utf-8'))
    table = next(o for o in exports if o.get('Type') == 'DataTable')
    rows = [convert(name, raw) for name, raw in table['Rows'].items()]
    out = HERE / f'{args.character}.hit_effects.json'
    out.write_text(json.dumps({'table': config['table'], 'struct': STRUCT, 'source': config['source'], 'rows': rows},
                              ensure_ascii=False, indent=1), encoding='utf-8')
    print(f'{out}: {len(rows)} rows')

    if 'stateMachine' in config:
        montages = hit_montages(config['stateMachine'])
        recipe_path = config['recipe']
        recipe = json.loads(recipe_path.read_text(encoding='utf-8'))
        recipe['hitEffectTable'] = f"{config['table']}.{config['table'].rsplit('/', 1)[1]}"
        recipe['beHitMontages'] = {str(anim): f"{config['montageDir']}/{name}.{name}" for anim, name in montages.items()}
        recipe['beHitMontagesSource'] = f"原作 AI 状态机 {config['stateMachine']} 的 TaskBeHitMontage.MontageMap（键 = EHitAnim 的值）"
        recipe_path.write_text(json.dumps(recipe, ensure_ascii=False, indent=1), encoding='utf-8')
        print(f'{recipe_path}:')
        for anim, name in montages.items():
            print(f'  {anim} {HIT_ANIMS[anim]} -> {name}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
