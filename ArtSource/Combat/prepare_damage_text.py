"""原作伤害飘字配置 → 项目伤害飘字表（FWuwaDamageTextInfo）的导入配方。
  python -I prepare_damage_text.py

数据来源：原作 ConfigDB db_damage_text.db 的 DamageText（flatbuffer，字段见 Define/Config/DamageText.js）。
行名 = 飘字配置 Id。原作选哪一行（DamageUiManager.Q2t）：伤害表 DamageTextType 有效（非 0、-1、2）就用它；
否则伤害为 0 → 9（免疫），元素 > 0 → 元素 Id（1~6），治疗 → 8，其余（物理）→ 10。
颜色是十六进制 RRGGBBAA（6 位时不透明）；暴击特效（UINiagara）、图标、性能优化版飘字项目还没有，不导入。
输出 damage_text.json，由 -run=WuwaDataTableSetup -Recipe=ArtSource/Combat/damage_text.json -Apply 写进 DataTable。原作数据，不提交。
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
DB = AKI / 'ConfigDB/db_damage_text.db'
JS = AKI / 'JavaScript/Core/Define/Config/DamageText.js'
TABLE = '/Game/LGUI/HUD/Damage/DT_DamageText'


def color(hex_text: str) -> list[int]:
    h = (hex_text or 'FFFFFFFF').strip()
    if len(h) == 6:
        h += 'FF'
    return [int(h[i:i + 2], 16) for i in (0, 2, 4, 6)]


def main() -> int:
    sys.stdout.reconfigure(encoding='utf-8')
    if not DB.exists():
        subprocess.run([str(PAKEXPORT), 'raw', 'Client/Content/Aki/ConfigDB/db_damage_text.db'], check=True, capture_output=True)
    fields = kuro_fb.parse_js(str(JS))
    rows = []
    for row_id, blob in sqlite3.connect(DB).execute('select Id, BinData from DamageText order by Id'):
        fb = kuro_fb.FB(blob)
        d = kuro_fb.decode_table(fb, fb.u32(0), fields, lambda name: None)
        tc, sc, ctc, csc = (color(d.get(k, '')) for k in ('textcolor', 'strokecolor', 'crittextcolor', 'critstrokecolor'))
        rows.append({
            'Name': str(row_id),
            'TextColor': {'R': tc[0], 'G': tc[1], 'B': tc[2], 'A': tc[3]},
            'StrokeColor': {'R': sc[0], 'G': sc[1], 'B': sc[2], 'A': sc[3]},
            'CritTextColor': {'R': ctc[0], 'G': ctc[1], 'B': ctc[2], 'A': ctc[3]},
            'CritStrokeColor': {'R': csc[0], 'G': csc[1], 'B': csc[2], 'A': csc[3]},
            'MinDeviationX': float(d.get('mindeviationx', 0)), 'MaxDeviationX': float(d.get('maxdeviationx', 0)),
            'MinDeviationY': float(d.get('mindeviationy', 0)), 'MaxDeviationY': float(d.get('maxdeviationy', 0)),
            'OwnDamageSequence': d.get('owndamagesequence', '') or 'None',
            'OwnCriticalDamageSequence': d.get('owncriticaldamagesequence', '') or 'None',
            'MonsterDamageSequence': d.get('monsterdamagesequence', '') or 'None',
            'MonsterCriticalDamageSequence': d.get('monstercriticaldamagesequence', '') or 'None',
            'DamageTextSequence': d.get('damagetextsequence', '') or 'None',
        })
    out = HERE / 'damage_text.json'
    out.write_text(json.dumps({'table': TABLE, 'struct': '/Script/Wuwa.WuwaDamageTextInfo', 'source': 'db_damage_text.db DamageText', 'rows': rows},
                              ensure_ascii=False, indent=1), encoding='utf-8')
    print(f'{out}: {len(rows)} rows')
    for row in rows:
        if row['Name'] in ('1', '2', '4', '8', '9', '10'):
            print('  ', row['Name'], row['TextColor'], row['CritTextColor'], row['MonsterDamageSequence'],
                  row['MinDeviationX'], row['MaxDeviationX'], row['MinDeviationY'], row['MaxDeviationY'])
    return 0


if __name__ == '__main__':
    sys.exit(main())
