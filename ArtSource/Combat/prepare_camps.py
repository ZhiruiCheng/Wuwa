"""原作阵营表 DT_Camp → 项目阵营表（FWuwaCampRow）的导入配方。
  python -I prepare_camps.py

原作 DT_Camp（行结构 SCamp）每行一个 Value 数组，下标 = 阵营在表里的行序号；
FightLibrary.Init 读表时把 0 记成敌对（2）、1 记成友好（1）、其它记成 0（CampUtils.GetCampRelationship 的返回值）。
项目按阵营名存成 Map（行顺序无关）。输出 camps.json，由 WuwaCampTableSetup commandlet 写进 DataTable。原作数据，不提交。
"""
from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
AKI = Path('C:/GamePakExtractor/Output/Exports/Client/Content/Aki')
PAKEXPORT = Path('C:/GamePakExtractor/Tools/PakExport/bin/out/PakExport.exe')
SOURCE = 'AI/AIFunctionCommon/DT_Camp'
TABLE = '/Game/Game/Combat/DT_Camp'
RELATION = {0: 'Hostile', 1: 'Friendly'}


def ensure_json(package: str) -> Path:
    path = AKI / f'{package}.json'
    if not path.exists():
        result = subprocess.run([str(PAKEXPORT), 'json', f'Client/Content/Aki/{package}.uasset'],
                                capture_output=True, text=True, encoding='utf-8', errors='replace')
        if result.returncode != 0 or not path.exists():
            raise RuntimeError('PakExport json failed: ' + result.stderr[-2000:])
    return path


def main() -> int:
    sys.stdout.reconfigure(encoding='utf-8')
    exports = json.loads(ensure_json(SOURCE).read_text(encoding='utf-8'))
    table = next(o for o in exports if o.get('Type') == 'DataTable')
    names = list(table['Rows'].keys())
    rows = []
    for name, raw in table['Rows'].items():
        values = next(v for k, v in raw.items() if re.match(r'Value(_\d+_[0-9A-F]{32})?$', k))
        if len(values) != len(names):
            raise ValueError(f'{name}: {len(values)} relations for {len(names)} camps')
        rows.append({'Name': name, 'Relations': {names[i]: RELATION.get(v, 'Neutral') for i, v in enumerate(values)}})
    out = HERE / 'camps.json'
    out.write_text(json.dumps({'table': TABLE, 'source': SOURCE, 'rows': rows}, ensure_ascii=False, indent=1), encoding='utf-8')
    print(f'{out}: {len(rows)} camps')
    for row in rows:
        hostile = [k for k, v in row['Relations'].items() if v == 'Hostile']
        print(f"  {row['Name']}: hostile to {', '.join(hostile) or '-'}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
