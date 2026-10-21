"""角色属性（原作 BaseProperty + RolePropertyGrowth）→ -run=WuwaRoleAttributeSetup 读的配方。

  python -I prepare_role_attributes.py

原作（db_property.db）：
  BaseProperty         每个角色 1 级的基础值，行 Id = RoleInfo.PropertyId（长离 1205、嘉贝莉娜 1208）
  RolePropertyGrowth   按 (等级, 突破) 的成长倍率（万分比），只有血量、攻击、防御三项成长
换算到项目（UWuwaAttributeSet）：暴击 / 爆伤是万分比 → 小数；能量类保持原作单位。
协奏能量上限（Proto_ElementEnergyMax）客户端数据里没有（推断是服务端给的）：用原作 DT_QteTag「普通QTE」的触发能量 10000
（RoleElementComponent 协奏达到 TriggerEnergy 才能变奏），伤害表的协奏回复（如长离普攻1 命中 88）也是这个量级。

输出（原作数据，不提交）：ArtSource/Role/role_attributes.recipe.json
"""
from __future__ import annotations

import json
import sqlite3
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'Bullet'))
import kuro_fb  # noqa: E402

AKI = Path('C:/GamePakExtractor/Output/Exports/Client/Content/Aki')
CONFIG_JS = AKI / 'JavaScript/Core/Define/Config'
PROPERTY_DB = AKI / 'ConfigDB/db_property.db'

# 项目角色资产 → 原作 PropertyId（RoleInfo 1205 / 1208 的 propertyid）
ROLES = {
    '/Game/Characters/Role/Common/Data/DA_Role_Changli': 1205,
    '/Game/Characters/Role/Common/Data/DA_Role_Jiabeilina': 1208,
}
ELEMENTS = {0: 'None', 1: 'Glacio', 2: 'Fusion', 3: 'Electro', 4: 'Aero', 5: 'Spectro', 6: 'Havoc'}
CONCERTO_MAX_PLACEHOLDER = 10000.0  # DT_QteTag 普通QTE Energy
# 客户端 BaseProperty 里是 0 的特殊能量上限（推断由服务端给）：
#   嘉贝 SE1（能量条，攒满放强化 E）：DT_SpecialEnergyBarInfo 120800 两格 ExtraFloatParams [60, 100]、DisableKeyOnPercent 100 → 上限 100；
#   嘉贝 SE3（变身弹药）：强化 E 的 1208003313 把 SE3 设成 SE1（满 = 100），变身条 120801 读 SE3 / MaxSE3 → 同为 100
SPECIAL_ENERGY_MAX_PLACEHOLDER = {1208: {1: 100, 3: 100}}


def rows(table: str, js: str):
    fields = kuro_fb.parse_js(str(CONFIG_JS / js))
    db = sqlite3.connect(PROPERTY_DB)
    for *keys, blob in db.execute(f'select *, BinData from {table}'):
        fb = kuro_fb.FB(blob)
        yield kuro_fb.decode_table(fb, fb.u32(0), fields, lambda _name: None)


def main() -> int:
    sys.stdout.reconfigure(encoding='utf-8')
    wanted = set(ROLES.values())
    base = {r['id']: r for r in rows('baseproperty', 'BaseProperty.js') if r.get('id') in wanted}
    properties = []
    for pid in sorted(wanted):
        r = base[pid]
        properties.append({
            'id': pid,
            'element': ELEMENTS[r.get('elementpropertytype', 0)],
            'maxHealth': r.get('lifemax', 0),
            'attack': r.get('atk', 0),
            'defense': r.get('def', 0),
            'critRate': r.get('crit', 0) / 10000,
            'critDamage': r.get('critdamage', 0) / 10000,
            # 共鸣效率（Proto_EnergyEfficiency 万分比）：客户端表里是空的，推断基础 100% 由服务端给
            'energyEfficiency': (r.get('energyefficiency') or 10000) / 10000,
            'maxEnergy': r.get('energymax', 0),
            'maxConcertoEnergy': r.get('elementenergymax', 0) or CONCERTO_MAX_PLACEHOLDER,
            **{f'maxSpecialEnergy{i}': r.get(f'specialenergy{i}max', 0) or SPECIAL_ENERGY_MAX_PLACEHOLDER.get(pid, {}).get(i, 0)
               for i in range(1, 6)},
        })
    growth = [{'level': r.get('level', 1), 'breachLevel': r.get('breachlevel', 0),
               'maxHealthRatio': r.get('lifemaxratio', 10000) / 10000, 'attackRatio': r.get('atkratio', 10000) / 10000,
               'defenseRatio': r.get('defratio', 10000) / 10000}
              for r in rows('rolepropertygrowth', 'RolePropertyGrowth.js')]
    recipe = {
        'baseTable': '/Game/Characters/Role/Common/Data/DT_RoleBaseProperty',
        'growthTable': '/Game/Characters/Role/Common/Data/DT_RolePropertyGrowth',
        'roles': [{'asset': asset, 'basePropertyId': pid} for asset, pid in ROLES.items()],
        'baseProperties': properties,
        'growth': sorted(growth, key=lambda g: (g['level'], g['breachLevel'])),
    }
    out = HERE / 'role_attributes.recipe.json'
    out.write_text(json.dumps(recipe, ensure_ascii=False, indent=1), encoding='utf-8')
    for p in properties:
        print(p['id'], p['element'], 'Lv1 HP', p['maxHealth'], 'ATK', p['attack'], 'DEF', p['defense'])
    print(f'{out}: {len(properties)} roles, {len(growth)} growth rows')
    return 0


if __name__ == '__main__':
    sys.exit(main())
