"""原作子弹表 → 项目子弹表（FWuwaBulletInfo）的导入配方。
  python -I prepare_bullets.py --character changli

数据来源：原作 DT_ReBulletDataMain（行结构 SReBulletDataMain），由 PakExport 导出成 JSON（缺的自动导出）。
只取 FWuwaBulletInfo 里有的字段（判定体、出生、时间、命中次数、伤害 ID、受击：被击效果 / 受击方向 / 顿帧）；其余字段等用到的步骤再加。
顿帧曲线（时间膨胀变化曲线，CurveFloat）导出关键帧写进 <character>.curves.json，由 WuwaCurveSetup 建到子弹表同目录的 TimeScale/ 下。
字段含义来自原作代码（Game/NewWorld/Bullet 下的 BulletDataBase.js、BulletActionInitCollision.js、
BulletCollisionUtil.js、BulletActionInitMove.js），见 FWuwaBulletInfo 的注释。

输出 <character>.bullets.json（和 <character>.curves.json）：UDataTable::CreateTableFromJSONString 能直接读的行数组（Name = 子弹 ID），
由 WuwaBulletTableSetup commandlet 写进 DataTable。原作数据，不提交。
目前不支持的行（形状 自定义模型/激光、项目还没有的出生基准/跟随方式）跳过并列在 skipped 里。
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
    # effects：特效管线（ArtSource/JiabeiAttack）的配方目录和资产标题，子弹特效DA 从那里的子弹特效配方（bulletEffects）找
    'changli': {'source': 'Character/Role/FemaleXL/ChangLi/Data/DT_ReBulletDataMain',
                'table': '/Game/Characters/Role/changli/Data/DT_BulletInfo_Changli',
                'effects': (HERE.parent / 'JiabeiAttack' / 'Changli', 'Changli')},
    'jiabei': {'source': 'Character/Role/FemaleXL/Jiabeilina/Data/DT_ReBulletDataMain',
               'table': '/Game/Characters/Role/jiabei/Data/DT_BulletInfo_Jiabei',
               'effects': (HERE.parent / 'JiabeiAttack', 'Jiabei')},
    # 芙露德莉丝 Boss（MB1FuludelisiMd00401）：怪物的子弹表和角色同一格式
    'fuludelisi': {'source': 'Character/Monster/Boss/MB1FuludelisiMd00401/Data/DT_ReBulletDataMain',
                   'table': '/Game/Characters/Monster/Boss/Fuludelisi/Data/DT_BulletInfo_Fuludelisi'},
}

# 原作 EBulletShape 的值 → (EWuwaBulletShape, 是否为大范围版本)。大范围版本（6~9）几何含义与对应小形状相同
# （BulletCollisionUtil.UpdateRegionExtend），并入；大圆柱没有内半径，Y 置 0。
SHAPES = {0: 'Box', 1: 'Sphere', 2: 'Sector', 3: 'Cylinder', 6: 'Box', 7: 'Sphere', 8: 'Sector', 9: 'Cylinder'}
BORN = {0: 'Attacker', 1: 'SkillTarget', 2: 'World', 3: 'Parent'}
# 3：吸附到骨骼（出生偏移按骨骼空间、朝向跟骨骼；原作 BulletBlueprintFunctionLibrary.AttachToBone）
FOLLOW = {0: 'FollowBone', 1: 'Fixed', 3: 'SnapToBone'}


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


def strip(value):
    if isinstance(value, dict):
        return {short(k): strip(v) for k, v in value.items()}
    if isinstance(value, list):
        return [strip(v) for v in value]
    return value


def enum_value(text: str) -> int:
    """'EBulletShape::NewEnumerator3' → 3（用户自定义枚举的值就是 NewEnumerator 后的序号）。"""
    match = re.search(r'NewEnumerator(\d+)$', text or '')
    if not match:
        raise ValueError(f'unexpected enum value {text!r}')
    return int(match.group(1))


def vector(v: dict) -> dict:
    return {'X': v['X'], 'Y': v['Y'], 'Z': v['Z']}


# 原作 命中判定类型（EHitType 的值）→ 阵营位掩码（BulletActionInitHit 的 campNumbers：对自己 1 / 对友方 2 / 对敌方 4 / 对队伍 3 / 对小队 11）；
# 配了 命中判定类型预设（BulletCampType 数据资产）时用资产里的 阵营 值。项目把"对小队"的 8 那一位单独叫 PlayerGroup。
CAMP_NUMBERS = [1, 2, 4, 3, 11]


def hit_camp(base: dict) -> int:
    preset = (base.get('命中判定类型预设') or {}).get('AssetPathName', 'None')
    if preset and preset != 'None':
        package = preset.split('.')[0].replace('/Game/Aki/', '')
        exports = json.loads(ensure_json(package).read_text(encoding='utf-8'))
        return int(next(o for o in exports if o.get('Type') == 'BulletCampType_C')['Properties']['阵营'])
    return CAMP_NUMBERS[enum_value(base['命中判定类型'])]


class Curves:
    """原作 CurveFloat（PakExport 的 ObjectPath）→ 项目资产路径 + 关键帧（同 UI/KuroPrefab/kuro_motion.Curves）。"""

    def __init__(self, folder: str):
        self.folder = folder
        self.items: dict[str, dict] = {}

    def add(self, ref) -> str:
        if not ref:
            return ''
        package = re.sub(r'\.\d+$', '', ref['ObjectPath']).replace('Client/Content/Aki/', '', 1)
        name = package.rsplit('/', 1)[1]
        path = f'{self.folder}/{name}'
        if path in self.items:
            if self.items[path]['source'] != package:
                raise SystemExit(f'curve name clash: {package} and {self.items[path]["source"]}')
        else:
            data = json.loads(ensure_json(package).read_text(encoding='utf-8'))
            curve = next(o for o in data if o.get('Type') == 'CurveFloat')['Properties'].get('FloatCurve', {})
            keys = [{'time': k.get('Time', 0.0), 'value': k.get('Value', 0.0), 'interp': k.get('InterpMode', 'RCIM_Linear'),
                     'tangentMode': k.get('TangentMode', 'RCTM_Auto'), 'arrive': k.get('ArriveTangent', 0.0), 'leave': k.get('LeaveTangent', 0.0)}
                    for k in curve.get('Keys', [])]
            self.items[path] = {'path': path, 'source': package, 'keys': keys,
                                'preInfinity': curve.get('PreInfinityExtrap', 'RCCE_Constant'), 'postInfinity': curve.get('PostInfinityExtrap', 'RCCE_Constant')}
        return f'{path}.{name}'


# 已导入的 buff（prepare_buffs.BUFF_IDS）：子弹表只引用这些
sys.path.insert(0, str(HERE.parent / 'Combat'))
sys.path.insert(0, str(HERE.parent / 'UI/KuroPrefab'))
from prepare_buffs import BUFF_IDS as IMPORTED_BUFFS  # noqa: E402
DROPPED_BUFFS: list[str] = []


def time_scale(ts: dict, curves: Curves) -> dict:
    """原作 时间膨胀 的 攻击顿帧 / 受击顿帧（时长 ≤ 0 不顿帧，CharacterHitComponent.iwr）。"""
    return {'Duration': ts['时间膨胀时长'], 'Value': ts['时间膨胀值'], 'Priority': ts['优先级'],
            'Curve': curves.add(ts['时间膨胀变化曲线']) if ts['时间膨胀时长'] > 0 else ''}


def convert(row_name: str, row: dict, curves: Curves) -> tuple[dict | None, str | None]:
    base, move, scale = row['基础设置'], row['移动设置'], row['时间膨胀']
    shape = enum_value(base['子弹形状'])
    born = enum_value(base['出生位置基准'])
    follow = enum_value(move['子弹跟随类型'])
    if shape not in SHAPES:
        return None, f'shape {shape}'
    if born not in BORN:
        return None, f'born standard {born}'
    if follow not in FOLLOW:
        return None, f'follow type {follow}'
    size = vector(base['初始大小'])
    if shape == 9:
        size['Y'] = 0.0
    bone = move['骨骼名字']
    return {
        'Name': row_name,
        'BulletName': row['子弹名称'],
        'Shape': SHAPES[shape],
        'Size': size,
        'CenterOffset': vector(base['中心位置偏移']),
        'BornStandard': BORN[born],
        'BornOffset': vector(base['出生位置偏移']),
        'BornRotation': {'Pitch': base['初始旋转']['Pitch'], 'Yaw': base['初始旋转']['Yaw'], 'Roll': base['初始旋转']['Roll']},
        'BornDistLimit': vector(base['限制生成距离']),
        'FollowType': FOLLOW[follow],
        'BoneName': '' if bone in (None, 'None') else bone,
        # 原作 是否贴地子弹 / 贴地探测距离
        'bStickGround': base['是否贴地子弹'],
        'StickGroundTraceLength': base['贴地探测距离'],
        'Duration': base['持续时间'],
        'CollisionDelay': base['碰撞判定延迟'],
        'CollisionDuration': base['碰撞判定时长'],
        'bDestroyOnSkillEnd': base['技能结束是否销毁子弹'],
        'VictimCount': base['命中个数'],
        'HitsPerVictim': base['每个单位总作用次数'],
        'TotalHitLimit': base['总作用次数限制'],
        'HitInterval': base['作用间隔'],
        'bIntervalPerVictim': base['作用间隔基于个体'],
        'DamageId': int(base['伤害ID']),
        'HitCamp': hit_camp(base),
        # 受击（原作 CharacterHitComponent）：被击效果 = 攻击者受击效果表的行名；子弹受击方向 = EBulletRelativeDir 的值
        'BeHitEffect': '' if base['被击效果'] in (None, 'None') else base['被击效果'],
        'HitDirection': enum_value(base['子弹受击方向']),
        'bContinuousCollision': base['是否持续碰撞'],
        'AttackerTimeScale': time_scale(scale['攻击顿帧'], curves),
        'VictimTimeScale': time_scale(scale['受击顿帧'], curves),
        'bTimeScaleIgnoreAttacker': scale['攻击顿帧忽略攻击者'],
        # 原作 执行逻辑.能量恢复类GE数组的Id：第一次命中给发射者加的 buff；只留已导入的（prepare_buffs.BUFF_IDS），其余记下来打印
        'EnergyBuffIds': energy_buffs(row_name, row['执行逻辑']['能量恢复类GE数组的Id']),
        # 原作 执行逻辑.命中后对攻击者发射GameplayEvent标签（项目 Tag 见 EVENT_TAG_MAP；没映射的记下来打印）
        'HitEventToAttacker': event_tag(row_name, row['执行逻辑']['命中后对攻击者发射GameplayEvent标签']['TagName']),
        # 原作 子弹允许生成Tag / 子弹禁止生成Tag（项目 Tag 见 OWNER_TAG_MAP；没映射的不限制，记下来打印）
        'SpawnRequiredTags': tag_container(row_name, base['子弹允许生成Tag']),
        'SpawnBlockedTags': tag_container(row_name, base['子弹禁止生成Tag']),
        'Children': children(row_name, row['子子弹设置']),
        'Effects': bullet_effects(row_name, row['表现效果设置']['子弹特效DA']),
    }, None


# 原作 召唤触发：只支持 定时（父子弹出生后按延迟 / 间隔生成，原作 BulletActionSummonBullet）
CHILD_TIMED = 'EBulletChildrenType::NewEnumerator4'


def children(row_name: str, items: list) -> list[dict]:
    out = []
    for c in items:
        if c['召唤触发'] != CHILD_TIMED:
            DROPPED_BUFFS.append(f"{row_name}: child bullet {c['召唤子弹ID']} trigger {c['召唤触发']} not supported")
            continue
        out.append({'BulletId': str(int(c['召唤子弹ID'])), 'Delay': c['召唤子弹延迟'], 'Count': int(c['召唤子弹数量']),
                    'Interval': c['召唤子弹间隔']})
    return out


# 原作角色 Tag → 项目 Tag（同 prepare_bullet_notifies.OWNER_TAG_MAP）
OWNER_TAG_MAP = {'角色.2e985a12.ab2b6d02.0c77b016': 'Role.Jiabei.State.Transformed'}


def tag_container(row_name: str, tags: list) -> str:
    mapped = []
    for tag in tags:
        if tag in OWNER_TAG_MAP:
            mapped.append(f'(TagName="{OWNER_TAG_MAP[tag]}")')
        else:
            DROPPED_BUFFS.append(f'{row_name}: spawn tag {tag} has no project mapping (ignored)')
    return f"(GameplayTags=({','.join(mapped)}))" if mapped else '()'


# 子弹特效DA（原作特效组）→ 特效管线子弹特效配方里的放置（每个放置一个 Model 组：DA_Fx_<Title>_<段>_Group_<名字>）
EFFECT_PLACEMENTS: dict[str, list[dict]] = {}
MISSING_EFFECTS: dict[str, list[str]] = {}


def load_effect_recipes(folder: Path, title: str) -> None:
    for recipe_file in sorted(folder.glob('*.json')):
        try:
            recipe = json.loads(recipe_file.read_text(encoding='utf-8'))
        except (json.JSONDecodeError, UnicodeDecodeError):
            continue
        if not isinstance(recipe, dict) or not recipe.get('bulletEffects'):
            continue
        stage = recipe['stage']
        for p in recipe['placements']:
            key = p.get('decal') or p.get('model')
            if not key:
                # 同 WuwaChangliAttackEffectSetup 的 Suffix：去掉 NS_Reference_ 前缀
                key = p['system'].removeprefix('NS_Reference_')
                if key.startswith('Attack') and '_' in key:
                    key = key.split('_', 1)[1]
            asset = f'DA_Fx_{title}_{stage}_Group_{key}'
            EFFECT_PLACEMENTS.setdefault(p['sourceEffect'], []).append({
                'Model': f'/Game/Effects/Models/{title}/{stage}/{asset}.{asset}',
                'Location': dict(zip('XYZ', p['location'])),
                'Rotation': dict(zip(('Pitch', 'Yaw', 'Roll'), p['rotation'])),
                'Scale': dict(zip('XYZ', p['scale'])),
                'Delay': p['delay'],
            })


def bullet_effects(row_name: str, ref: dict) -> list[dict]:
    path = (ref or {}).get('AssetPathName', 'None')
    if not path or path == 'None':
        return []
    group = path.rsplit('/', 1)[1].split('.', 1)[0]
    if group not in EFFECT_PLACEMENTS:
        MISSING_EFFECTS.setdefault(group, []).append(row_name)
        return []
    return sorted(EFFECT_PLACEMENTS[group], key=lambda e: e['Delay'])


# 原作子弹发的 GameplayEvent Tag → 项目 Tag（原作名被哈希的按用到它的 GA 命名）
EVENT_TAG_MAP = {
    # 嘉贝 E 跳踢踢中 → GA_Jump_MoveAttack 接后半段（原作变量 触发变招）
    '角色.2e985a12.ab2b6d02.2c4cc3bc': 'Role.Jiabei.Event.SkillBranch',
}


def event_tag(row_name: str, tag: str) -> str:
    if tag in (None, '', 'None'):
        return '()'
    if tag not in EVENT_TAG_MAP:
        DROPPED_BUFFS.append(f'{row_name}: hit event {tag} has no project mapping (EVENT_TAG_MAP)')
        return '()'
    return f'(TagName="{EVENT_TAG_MAP[tag]}")'


def energy_buffs(row_name: str, ids: list) -> list[int]:
    kept = [int(i) for i in ids if int(i) in IMPORTED_BUFFS]
    for i in ids:
        if int(i) not in IMPORTED_BUFFS:
            DROPPED_BUFFS.append(f'{row_name}: energy buff {int(i)} not imported')
    return kept


def main() -> int:
    sys.stdout.reconfigure(encoding='utf-8')
    parser = argparse.ArgumentParser()
    parser.add_argument('--character', required=True, choices=sorted(CHARACTERS))
    args = parser.parse_args()
    config = CHARACTERS[args.character]
    if 'effects' in config:
        load_effect_recipes(*config['effects'])

    exports = json.loads(ensure_json(config['source']).read_text(encoding='utf-8'))
    table = next(o for o in exports if o.get('Type') == 'DataTable')
    rows, skipped = [], []
    curves = Curves(config['table'].rsplit('/', 1)[0] + '/TimeScale')
    for name, raw in table['Rows'].items():
        row, reason = convert(name, strip(raw), curves)
        if row:
            rows.append(row)
        else:
            skipped.append({'id': name, 'name': strip(raw).get('子弹名称'), 'reason': reason})

    out = HERE / f'{args.character}.bullets.json'
    out.write_text(json.dumps({'table': config['table'], 'source': config['source'], 'rows': rows, 'skipped': skipped},
                              ensure_ascii=False, indent=1), encoding='utf-8')
    print(f'{out}: {len(rows)} rows, {len(skipped)} skipped')
    curves_out = HERE / f'{args.character}.curves.json'
    curves_out.write_text(json.dumps({'source': config['source'] + ' 时间膨胀变化曲线', 'curves': list(curves.items.values())},
                                     ensure_ascii=False, indent=1), encoding='utf-8')
    print(f'{curves_out}: {len(curves.items)} curves')
    for s in skipped:
        print(f"  skipped {s['id']} {s['name']}: {s['reason']}")
    for d in DROPPED_BUFFS:
        print('  ' + d)
    names = {r['Name'] for r in rows}
    for r in rows:
        for c in r['Children']:
            if c['BulletId'] not in names:
                print(f"  {r['Name']}: child bullet {c['BulletId']} not in the table")
    print(f'  bullet effects: {sum(bool(r["Effects"]) for r in rows)} rows with effects; '
          f'{len(MISSING_EFFECTS)} effect DAs not built yet ({sum(map(len, MISSING_EFFECTS.values()))} rows; '
          f'add them to bulletEffects in ArtSource/JiabeiAttack/characters.py)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
