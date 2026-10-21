"""怪物初始属性、名字、Boss 血条配置 → 怪物蓝图配方（<monster>.monster.json，由 WuwaMonsterSetup 写进蓝图）。
  python -I prepare_monster_attributes.py fuludelisi --template 665800000 --curve 0 --level 90

--template：原作怪物模板（ConfigDB db_template.db 的 templateconfig，JSON）：AttributeComponent.PropertyId（基础属性行）、
BaseInfoComponent.TidName（名字，查 zh-Hans/lang_multi_text.db）、MonsterComponent.BossViewConfig（Boss 血条类型）。
芙露德莉斯（模型 390295 = MB1FuludelisiMd00401）有 4 个模板；665800000（Monster_Branch2.3_01）放在 2_4_Babel 的芙露德莉斯关卡（地图 39033）。
模板里的 Level 是 1，实际等级由世界等级加成决定，这里按 --level 给。--property 可以覆盖模板的基础属性行。

数据来源：原作 ConfigDB db_property.db 的 BaseProperty（1 级基础属性）和 MonsterPropertyGrowth（按 CurveId + Level 的成长倍率，万分比）。
生命 / 攻击 / 防御 = floor(基础 × 倍率 / 10000)，和原作 RoleModel.GetAttributeByLevel 的角色成长同一规则（怪物成长在服务器算，客户端看不到）。
暴击伤害、抗性等万分比字段 / 10000 变成小数；不随等级变的字段直接取基础值。
芙露德莉丝本体的属性行不在客户端数据里，这里先用占位行（--property），以后找到真实行再换。原作数据，不提交。
"""
from __future__ import annotations

import argparse
import json
import sqlite3
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'Bullet'))
import kuro_fb  # noqa: E402

CONFIG = Path('C:/GamePakExtractor/Output/Exports/Client/Content/Aki/JavaScript/Core/Define/Config')
DB = Path('C:/GamePakExtractor/Output/Exports/Client/Content/Aki/ConfigDB/db_property.db')
TEMPLATE_DB = DB.with_name('db_template.db')
TEXT_DB = DB.parent / 'zh-Hans/lang_multi_text.db'


def load_template(template_id: int) -> dict:
    row = sqlite3.connect(TEMPLATE_DB).execute('select BinData from templateconfig where Id=?', (template_id,)).fetchone()
    if not row:
        raise SystemExit(f'template {template_id} not found')
    text = row[0].decode('utf-8', 'ignore')
    return json.JSONDecoder().raw_decode(text[text.find('{'):])[0]


def text_of(key: str) -> str:
    row = sqlite3.connect(TEXT_DB).execute('select Content from MultiText where Id=?', (key,)).fetchone()
    return row[0] if row else ''
# 原作字段 → (项目属性 "类.属性", 万分比转小数)
RESISTANCES = ['damageresistancephys'] + [f'damageresistanceelement{i}' for i in range(1, 7)]


def decode(connection, table: str, js: str, where: str, args) -> list[dict]:
    fields = kuro_fb.parse_js(str(CONFIG / js))
    out = []
    for (blob,) in connection.execute(f'select BinData from {table} where {where}', args):
        fb = kuro_fb.FB(blob)
        out.append(kuro_fb.decode_table(fb, fb.u32(0), fields, lambda name: None))
    return out


def main() -> int:
    sys.stdout.reconfigure(encoding='utf-8')
    parser = argparse.ArgumentParser()
    parser.add_argument('monster')
    parser.add_argument('--property', type=int)
    parser.add_argument('--template', type=int)
    parser.add_argument('--curve', type=int, default=0)
    parser.add_argument('--level', type=int, required=True)
    args = parser.parse_args()

    template = load_template(args.template) if args.template else {}
    property_id = args.property or template.get('AttributeComponent', {}).get('PropertyId')
    if not property_id:
        print('need --property or --template')
        return 1
    args.property = property_id
    connection = sqlite3.connect(DB)
    base = decode(connection, 'BaseProperty', 'BaseProperty.js', 'Id=?', (args.property,))
    if not base:
        print(f'BaseProperty {args.property} not found')
        return 1
    base = base[0]
    growth = [g for g in decode(connection, 'MonsterPropertyGrowth', 'MonsterPropertyGrowth.js', '1=1', ())
              if int(g.get('curveid', 0)) == args.curve and int(g.get('level', 0)) == args.level]
    if len(growth) != 1:
        print(f'MonsterPropertyGrowth curve {args.curve} level {args.level}: {len(growth)} rows')
        return 1
    growth = growth[0]

    def grown(field: str, ratio: str) -> float:
        return float(int(base.get(field, 0) * growth.get(ratio, 10000) / 10000))

    attributes = {
        'WuwaAttributeSet.Level': float(args.level),
        'WuwaAttributeSet.MaxHealth': grown('lifemax', 'lifemaxratio'),
        'WuwaAttributeSet.Attack': grown('atk', 'atkratio'),
        'WuwaAttributeSet.Defense': grown('def', 'defratio'),
        'WuwaAttributeSet.CritRate': base.get('crit', 0) / 10000.0,
        'WuwaAttributeSet.CritDamage': base.get('critdamage', 0) / 10000.0,
        'WuwaAttributeSet.ElementType': float(base.get('elementpropertytype', 0)),
    }
    for element, field in enumerate(RESISTANCES):
        attributes[f'WuwaCombatAttributeSet.ElementResistance{element}'] = base.get(field, 0) / 10000.0
    # 韧性 / 质量（原作 BaseDamageComponent 削韧、BaseAttributeComponent 自动回复；不随等级变）。缺省值是原作 BaseProperty 的 flatbuffer 默认值。
    # ToughMax 要在 Tough 前面（Tough 按 ToughMax 夹）；时间字段原作是毫秒，项目用秒；三个削韧系数原作万分比。
    attributes['WuwaCombatAttributeSet.ToughMax'] = float(base.get('toughmax', 0))
    attributes['WuwaCombatAttributeSet.Tough'] = float(base.get('tough', 0))
    attributes['WuwaCombatAttributeSet.ToughRecover'] = float(base.get('toughrecover', 0))
    attributes['WuwaCombatAttributeSet.ToughRecoverDelayTime'] = base.get('toughrecoverdelaytime', 0) / 1000.0
    attributes['WuwaCombatAttributeSet.WeakTime'] = base.get('weaktime', 0) / 1000.0
    attributes['WuwaCombatAttributeSet.ToughChange'] = base.get('toughchange', 10000) / 10000.0
    attributes['WuwaCombatAttributeSet.ToughReduce'] = base.get('toughreduce', 10000) / 10000.0
    attributes['WuwaCombatAttributeSet.SkillToughRatio'] = base.get('skilltoughratio', 10000) / 10000.0
    attributes['WuwaCombatAttributeSet.Mass'] = float(base.get('mass', 100))

    recipe_path = HERE / f'{args.monster}.monster.json'
    recipe = json.loads(recipe_path.read_text(encoding='utf-8'))
    recipe['attributes'] = attributes
    if template:
        name_key = template.get('BaseInfoComponent', {}).get('TidName', '')
        recipe['displayName'] = text_of(name_key)
        view = template.get('MonsterComponent', {}).get('BossViewConfig', {})
        recipe['bossViewConfig'] = {'BossStateViewType': view.get('BossStateViewType', 0), 'ShowDistance': view.get('ShowDistance', 0) * 1.0,
                                    'bOnlyShowInBattleState': bool(view.get('OnlyShowInBattleState', False))}
        recipe['attributesSource'] = (f'模板 {args.template}：BaseProperty {args.property} × MonsterPropertyGrowth 曲线 {args.curve} 等级 {args.level}'
                                      f'（模板等级 1 + 世界等级加成，这里直接给 {args.level}）')
    else:
        recipe['attributesSource'] = f'BaseProperty {args.property} × MonsterPropertyGrowth 曲线 {args.curve} 等级 {args.level}'

    recipe_path.write_text(json.dumps(recipe, ensure_ascii=False, indent=1), encoding='utf-8')
    print(f'{recipe_path}:')
    print('  growth', {k: growth.get(k) for k in ('lifemaxratio', 'atkratio', 'defratio')})
    for name, value in attributes.items():
        print(f'  {name} = {value:g}')
    if template:
        print('  displayName', recipe['displayName'], 'bossViewConfig', recipe['bossViewConfig'])
    return 0


if __name__ == '__main__':
    sys.exit(main())
