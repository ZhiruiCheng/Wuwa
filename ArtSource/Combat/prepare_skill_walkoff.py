"""原作技能表的 WalkOffLedge → 项目技能表 FWuwaSkillInfo.bWalkOffLedge 的导入配方。
  python -I prepare_skill_walkoff.py --character changli

字段名容易读反：原作 CharacterSkillComponent 技能开始 `if (WalkOffLedge) SetWalkOffLedgeRecord(false)`（关掉 bCanWalkOffLedges = 开边缘保护），
DoSkillEndMoveAction 恢复。所以为真 = 技能期间不能走出平台边缘（长离地面普攻都是真，空中普攻 / E 是假）。
由 -run=WuwaDataTableSetup -Recipe=ArtSource/Combat/skill_walkoff_<角色>.json -OnlyFields=bWalkOffLedge -Apply 只写这一个字段
（项目技能表其余字段不动；原作有、项目没有的技能自动跳过）。原作数据，不提交。
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from prepare_skill_lockon import CHARACTERS, source_rows, strip  # noqa: E402


def main() -> int:
    sys.stdout.reconfigure(encoding='utf-8')
    parser = argparse.ArgumentParser()
    parser.add_argument('--character', required=True, choices=sorted(CHARACTERS))
    args = parser.parse_args()
    config = CHARACTERS[args.character]
    # 角色表 + 通用技能表（空中闪避 100003 / 100004 等）
    rows = [{'Name': name, 'bWalkOffLedge': bool(strip(raw).get('WalkOffLedge', False))} for name, raw in source_rows(config).items()]
    out = HERE / f'skill_walkoff_{args.character}.json'
    out.write_text(json.dumps({'table': config['table'], 'source': config['source'], 'rows': rows}, ensure_ascii=False, indent=1), encoding='utf-8')
    print(f'{out.name}: {sum(r["bWalkOffLedge"] for r in rows)}/{len(rows)} rows protect ledges')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
