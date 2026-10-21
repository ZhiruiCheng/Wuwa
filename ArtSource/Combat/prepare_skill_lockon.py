"""原作技能表里与"技能目标"有关的字段 → 项目技能表（FWuwaSkillInfo）的部分字段导入配方。
  python -I prepare_skill_lockon.py --character changli

只取 IsLockOn / SkillTarget.LockOnConfigId / SkillTarget.SkillTargetPriority / SkillDirection，
由 -run=WuwaDataTableSetup -Recipe=ArtSource/Combat/skill_lockon_<角色>.json -OnlyFields=bIsLockOn,LockOnConfigId,SkillTargetPriority,SkillDirection
只写这几个字段（项目技能表其余字段不动；原作有、项目没有的技能自动跳过）。原作数据，不提交。

枚举：原作是 UserDefinedEnum，代码里用的数值不是 NewEnumeratorN 的 N，而是枚举资产 Names 里的值（ESkillTargetPriority / ESkillTargetDirection），
项目枚举按这个数值排（EWuwaSkillTargetPriority / EWuwaSkillDirection）。
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
CHARACTERS = {
    'changli': {'source': 'Character/Role/FemaleXL/ChangLi/Data/DT_SkillInfo', 'table': '/Game/Characters/Role/changli/Data/DT_SkillInfo_Changli'},
    'jiabei': {'source': 'Character/Role/FemaleXL/Jiabeilina/Data/DT_SkillInfo', 'table': '/Game/Characters/Role/jiabei/Data/DT_SkillInfo_Jiabei'},
}
# 项目枚举按原作数值排列
PRIORITY = ['InputDirection', 'ActorDirection', 'KeepLockTarget', 'Distance', 'CameraDirection',
            'InputDirectionPrecise', 'CameraDirectionPrecise', 'ActorDirectionPrecise', 'GameSetting']
DIRECTION = ['SkillTarget', 'InputDirection', 'ActorDirection', 'CameraDirection', 'VehicleSkillTarget']


def ensure_json(package: str) -> Path:
    path = AKI / f'{package}.json'
    if not path.exists():
        result = subprocess.run([str(PAKEXPORT), 'json', f'Client/Content/Aki/{package}.uasset'],
                                capture_output=True, text=True, encoding='utf-8', errors='replace')
        if result.returncode != 0 or not path.exists():
            raise RuntimeError('PakExport json failed: ' + result.stderr[-2000:])
    return path


def enum_values(package: str) -> dict:
    """UserDefinedEnum 资产：'E::NewEnumeratorN' → 代码里用的数值。"""
    exports = json.loads(ensure_json(package).read_text(encoding='utf-8'))
    enum = next(o for o in exports if o.get('Type') == 'UserDefinedEnum')
    return {k: v for k, v in enum['Names'].items() if not k.endswith('_MAX')}


def strip(value):
    if isinstance(value, dict):
        return {re.sub(r'_\d+_[0-9A-F]{32}$', '', k): strip(v) for k, v in value.items()}
    if isinstance(value, list):
        return [strip(v) for v in value]
    return value


# 原作通用技能表（所有角色共用：闪避 100001~100004 等）；项目把用到的行放进各角色自己的技能表
COMMON_SOURCE = 'Data/Fight/DT_Common_Role_SkillInfo'


def source_rows(config: dict) -> dict:
    """角色技能表 + 通用技能表的行（同名以角色表为准）。"""
    rows = {}
    for source in (COMMON_SOURCE, config['source']):
        exports = json.loads(ensure_json(source).read_text(encoding='utf-8'))
        rows.update(next(o for o in exports if o.get('Type') == 'DataTable')['Rows'])
    return rows


def main() -> int:
    sys.stdout.reconfigure(encoding='utf-8')
    parser = argparse.ArgumentParser()
    parser.add_argument('--character', required=True, choices=sorted(CHARACTERS))
    args = parser.parse_args()
    config = CHARACTERS[args.character]
    priority = enum_values('Character/BaseCharacter/ESkillTargetPriority')
    direction = enum_values('Character/BaseCharacter/ESkillTargetDirection')
    rows = []
    for name, raw in source_rows(config).items():
        row = strip(raw)
        target = row.get('SkillTarget') or {}
        rows.append({
            'Name': name,
            'bIsLockOn': bool(row.get('IsLockOn', False)),
            'LockOnConfigId': int(target.get('LockOnConfigId', 0)),
            'SkillTargetPriority': PRIORITY[priority[target.get('SkillTargetPriority', 'ESkillTargetPriority::NewEnumerator13')]],
            'SkillDirection': DIRECTION[direction[row.get('SkillDirection', 'ESkillTargetDirection::NewEnumerator0')]],
        })
    out = HERE / f'skill_lockon_{args.character}.json'
    out.write_text(json.dumps({'table': config['table'], 'source': config['source'], 'rows': rows}, ensure_ascii=False, indent=1), encoding='utf-8')
    print(f'{out}: {len(rows)} skills')
    return 0


if __name__ == '__main__':
    sys.exit(main())
