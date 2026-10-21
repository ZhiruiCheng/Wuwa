"""原作锁定参数表 → 项目锁定参数表（FWuwaLockOnConfig）的导入配方。
  python -I prepare_lockon.py

数据来源：原作 ConfigDB db_lockon.db 的 LockOnConfig（策划表"m.目标筛选配置.xlsx"，flatbuffer BinData），
字段和默认值见 JavaScript/Core/Define/Config/LockOnConfig.js（表里没填的字段用默认值：上 300、下 800、容忍角 22.5）。
输出 lockon.json，由 -run=WuwaDataTableSetup -Recipe=ArtSource/Combat/lockon.json 写进 DataTable。原作数据，不提交。
"""
from __future__ import annotations

import json
import sqlite3
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'Bullet'))
import kuro_fb  # noqa: E402

AKI = Path('C:/GamePakExtractor/Output/Exports/Client/Content/Aki')
PAKEXPORT = Path('C:/GamePakExtractor/Tools/PakExport/bin/out/PakExport.exe')
DB = AKI / 'ConfigDB/db_LockOn.db'
JS = AKI / 'JavaScript/Core/Define/Config/LockOnConfig.js'
TABLE = '/Game/Game/Combat/DT_LockOnConfig'
# 原作字段 → 项目字段，以及原作 flatbuffer 默认值
FIELDS = {'distance': ('Distance', 0), 'updistance': ('UpDistance', 300), 'downdistance': ('DownDistance', 800),
          'sectorradius': ('SectorRadius', 0), 'sectorangle': ('SectorAngle', 0), 'toleranceangle': ('ToleranceAngle', 22.5)}


def main() -> int:
    sys.stdout.reconfigure(encoding='utf-8')
    if not DB.exists():
        subprocess.run([str(PAKEXPORT), 'raw', 'Client/Content/Aki/ConfigDB/db_lockon.db'], check=True, capture_output=True)
    fields = kuro_fb.parse_js(str(JS))
    connection = sqlite3.connect(DB)
    rows = []
    for config_id, blob in connection.execute('select Id, BinData from LockOnConfig order by Id'):
        fb = kuro_fb.FB(blob)
        decoded = kuro_fb.decode_table(fb, fb.u32(0), fields, lambda name: None)
        row = {'Name': str(config_id)}
        for source, (target, default) in FIELDS.items():
            row[target] = float(decoded.get(source, default))
        rows.append(row)
    out = HERE / 'lockon.json'
    out.write_text(json.dumps({'table': TABLE, 'struct': '/Script/Wuwa.WuwaLockOnConfig', 'source': 'db_lockon.db LockOnConfig', 'rows': rows},
                              ensure_ascii=False, indent=1), encoding='utf-8')
    print(f'{out}: {len(rows)} lock-on configs')
    for row in rows:
        if row['Name'] in ('1', '2', '3', '114'):
            print('  ', row)
    return 0


if __name__ == '__main__':
    sys.exit(main())
