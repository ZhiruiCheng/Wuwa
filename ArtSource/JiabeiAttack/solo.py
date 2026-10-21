"""
把一段的配方拆成“每层一个系统”的排查用配方 Solo_<段>.json：每个系统只含一层，不放任何通知（不动蒙太奇）。
  python solo.py [--character changli] Attack03 [--only Dg_03,Kaiqiang_09]
然后：
  UnrealEditor-Cmd ... -run=WuwaChangliAttackEffectSetup -Character=<角色> -Attack=Solo_Attack03 -Apply
  .\\gallery.ps1 [-Role changli] -Systems <输出里列出的系统名> -Columns 6 -Spacing 500 -Pitch -89 -Distance 3000
输出按网格顺序（左→右、下→上）列出每格对应的系统 / 层。生成的资产名带 __<层名>，不会覆盖正式资产。
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import characters  # noqa: E402


def main(argv: list[str]) -> int:
    character, argv = characters.select(argv)
    out = character['out']
    stage = argv[0]
    only = argv[argv.index('--only') + 1].split(',') if '--only' in argv else None
    recipe = json.loads((out / f'{stage}.json').read_text(encoding='utf-8'))
    systems = []
    for system in recipe['systems']:
        if only and not any(part in system['name'] for part in only):
            continue
        for layer in system['layers']:
            name = f"{system['name']}__{layer['name']}"
            solo_layer = {**layer, 'id': f"{layer['id']}__solo"}
            systems.append({**system, 'name': name, 'layers': [solo_layer]})
    solo = {**recipe, 'stage': f'Solo_{stage}', 'systems': systems, 'placements': [], 'unplacedEvents': [], 'solo': True}
    (out / f'Solo_{stage}.json').write_text(json.dumps(solo, ensure_ascii=False, indent=1), encoding='utf-8')
    print(','.join(s['name'] for s in systems))
    for index, s in enumerate(systems):
        print(f'{index:2d} {s["name"]}', file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
