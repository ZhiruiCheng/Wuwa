"""原作伤害表 → 项目伤害表（FWuwaDamageInfo）的导入配方。
  python -I prepare_damage.py

只导入子弹表里用到的伤害 ID：扫描 ArtSource/Bullet/*.bullets.json 的 DamageId（加新角色后重跑即可）。
数据来源：原作 ConfigDB db_damage.db 的 Damage（策划表"j.结算"，flatbuffer BinData），字段和默认值见
JavaScript/Core/Define/Config/Damage.js（RelatedProperty 没填时是 7 = 攻击）。
万分比字段（倍率、浮动）除以 10000 变成小数；浮动没填时项目侧按 1 处理（原作 GetLevelValue 默认 PER_TEN_THOUSAND）。
削韧 / 破霸体 / 能量 / 协奏原样保存（原作单位），后面的步骤再用。
输出 damage.json，由 -run=WuwaDataTableSetup -Recipe=ArtSource/Combat/damage.json -Apply 写进 DataTable。原作数据，不提交。
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
DB = AKI / 'ConfigDB/db_damage.db'
JS = AKI / 'JavaScript/Core/Define/Config/Damage.js'
TABLE = '/Game/Game/Combat/DT_Damage'

# 占位伤害（2026-10-21 用户要求先填合理数值、不查数据）：客户端 db_damage 里嘉贝变身后的招式 / E / 强化 E / 大招这些行是空壳
# （倍率、韧性、元素全 0），按同角色已有行定的量级，原作数据找到后删掉对应项。
# (Type, 倍率, 韧性, 共鸣能量, 协奏)：Type 1 普攻、4 共鸣技能、5 重击、2 大招（和已有行一致）
def _placeholder(kind, rate, tough, energy, concerto):
    return {'Element': 2, 'Type': kind, 'RateLv': [rate], 'ToughLv': [float(tough)], 'Energy': [float(energy)], 'ElementPower': [float(concerto)]}
PLACEHOLDER_DAMAGE = {
    # EX 普攻 1~4 / 变身下落：比普通普攻（0.48~1.44）稍高
    **{i: _placeholder(1, 0.9, 2400, 60, 84) for i in ('1208011001', '1208012001', '1208012002', '1208012003', '1208013001', '1208013004',
                                                         '1208013011', '1208013014', '1208014001', '1208014002', '1208008011', '1208008013')},
    # 爆蛇眼（超级强化普攻）
    '1208004007': _placeholder(1, 2.5, 6000, 150, 210), '1208004997': _placeholder(1, 2.5, 6000, 150, 210),
    '1208066666': _placeholder(1, 0.5, 1500, 37, 52),
    # EX 重击 1~3：普通重击 0.6~2.1
    **{i: _placeholder(5, 1.2, 3000, 75, 105) for i in ('1208016009', '1208016010', '1208016011', '1208016013', '1208016019')},
    # E 跳踢：同长离 E（1.49，韧性 4000，能量 160）
    **{i: _placeholder(4, 1.5, 4000, 160, 0) for i in ('1208007001', '1208007002', '1208007011', '1208007032')},
    # 强化 E（开变身那一下）
    '1208101002': _placeholder(4, 3.0, 8000, 0, 0), '1208101003': _placeholder(4, 3.0, 8000, 0, 0),
    # 大招：起爆 6.0，燃烧每跳 1.2（10 跳 + 最后一下，总量和长离大招 22 同档）
    '1208302002': _placeholder(2, 6.0, 20000, 0, 0), '1208302003': _placeholder(2, 1.2, 2000, 0, 0),
}
# 有攻击类型加成的原作 Type（0 普攻 … 5 声骸）；其他值原作不加成
BONUS_TYPES = range(6)


def used_damage_ids() -> dict[int, list[str]]:
    used: dict[int, list[str]] = {}
    for path in sorted((HERE.parent / 'Bullet').glob('*.bullets.json')):
        recipe = json.loads(path.read_text(encoding='utf-8'))
        for row in recipe['rows']:
            damage_id = int(row.get('DamageId') or 0)
            if damage_id > 0:
                used.setdefault(damage_id, []).append(f"{path.name.split('.')[0]}:{row['Name']}")
    return used


def per_ten_thousand(values) -> list[float]:
    return [v / 10000.0 for v in values or []]


def main() -> int:
    sys.stdout.reconfigure(encoding='utf-8')
    if not DB.exists():
        subprocess.run([str(PAKEXPORT), 'raw', 'Client/Content/Aki/ConfigDB/db_damage.db'], check=True, capture_output=True)
    fields = kuro_fb.parse_js(str(JS))
    connection = sqlite3.connect(DB)
    used = used_damage_ids()
    rows, missing, unsupported = [], [], []
    placeholders = []
    for damage_id in sorted(used):
        found = connection.execute('select BinData from Damage where Id=?', (damage_id,)).fetchone()
        if not found:
            missing.append(damage_id)
            continue
        fb = kuro_fb.FB(found[0])
        d = kuro_fb.decode_table(fb, fb.u32(0), fields, lambda name: None)
        row = {
            'Name': str(damage_id),
            'Element': int(d.get('element', 0)),
            'Type': int(d.get('type', 0)),
            'RelatedProperty': int(d.get('relatedproperty', 7)),
            'CalculateType': int(d.get('calculatetype', 0)),
            'DamageTextType': int(d.get('damagetexttype', 0)),
            'FormulaType': int(d.get('formulatype', 0)),
            'RateLv': per_ten_thousand(d.get('ratelv')),
            'FluctuationLower': per_ten_thousand(d.get('fluctuationlower')),
            'FluctuationUpper': per_ten_thousand(d.get('fluctuationupper')),
            'ToughLv': [float(v) for v in d.get('toughlv') or []],
            'HardnessLv': [float(v) for v in d.get('hardnesslv') or []],
            'Energy': [float(v) for v in d.get('energy') or []],
            'ElementPower': [float(v) for v in d.get('elementpower') or []],
        }
        if not any(row['RateLv']) and str(damage_id) in PLACEHOLDER_DAMAGE:
            row.update(PLACEHOLDER_DAMAGE[str(damage_id)])
            placeholders.append(str(damage_id))
        if row['FormulaType'] != 0 or row['CalculateType'] != 0 or row['RelatedProperty'] not in (2, 7, 10):
            unsupported.append((damage_id, row['FormulaType'], row['CalculateType'], row['RelatedProperty']))
        rows.append(row)
    out = HERE / 'damage.json'
    out.write_text(json.dumps({'table': TABLE, 'struct': '/Script/Wuwa.WuwaDamageInfo', 'source': 'db_damage.db Damage', 'rows': rows},
                              ensure_ascii=False, indent=1), encoding='utf-8')
    print(f'{out}: {len(rows)} damage rows ({len(used)} ids used by bullets)')
    if placeholders:
        print(f'  placeholder damage (empty rows in client data, PLACEHOLDER_DAMAGE): {len(placeholders)} rows', placeholders)
    if missing:
        print('  not in db_damage (bullets keep the id, damage is skipped):', [(m, used[m][:2]) for m in missing])
    if unsupported:
        print('  rows the project does not compute yet (id, formula, calculate, related):', unsupported)
    no_bonus = sorted({(row['Type']) for row in rows if row['Type'] not in BONUS_TYPES})
    if no_bonus:
        print('  Type values without attack-type bonus (as in the original):', no_bonus)
    for row in rows:
        if row['Name'] in ('1205001000', '1208001001'):
            print('  ', {k: row[k] for k in ('Name', 'Element', 'Type', 'RelatedProperty', 'RateLv', 'FluctuationLower', 'FluctuationUpper')})
    return 0


if __name__ == '__main__':
    sys.exit(main())
