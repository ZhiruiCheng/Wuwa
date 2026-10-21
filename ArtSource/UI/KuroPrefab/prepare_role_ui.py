"""角色特有的 UI 数据（头像、元素、E/R 技能图标、协奏/特殊能量条）→ -run=WuwaRoleUiSetup 读的配方。

  python -I prepare_role_ui.py

原作没有"角色 UI DataAsset"，这些引用分散在几张表里（下面 ROLES 里的数值就是从这些表里查出来的）：
  RoleInfo（db_role.db roleinfo）   RoleHeadIconBig、ElementId、SpecialEnergyBarId
  SkillButton（db_skillButton.db）  每个角色的按钮：类型 6 = 共鸣技能 E，8 = 共鸣解放 R（和对应的技能 Id）
  DT_SkillInfo_<角色>.SkillIcon     技能图标（Common/Atlas/SkillIcon 里的精灵）
  ElementInfo（db_element_info.db） 元素图标、颜色
  DT_SpecialEnergyBarInfo           能量条：预制体类型、格数、外框贴图、颜色……
项目里把"每个角色自己的"放进 UWuwaRoleDataAsset（头像、元素、技能按钮、能量条 Id），
共享的两张表做成 DataTable：DT_ElementInfo（行名 = 元素标签）、DT_SpecialEnergyBarInfo（行名 = 能量条 Id）。

输出（原作资源，不提交）：ArtSource/UI/HUD/Kuro/role_ui.recipe.json，图片在 ArtSource/UI/HUD/Kuro/Textures。
能量条预制体本身由 kuro_export.py energybar_<Id>.export.json + -run=WuwaKuroHudBuild 生成到 /Game/LGUI/HUD/EnergyBar/EB_<Id>。
"""
from __future__ import annotations

import json
import re
import sqlite3
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import kuro_export as ke  # noqa: E402
import kuro_layout as kl  # noqa: E402

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'Bullet'))
import kuro_fb  # noqa: E402

EXPORTS = kl.kp.EXPORTS
SKILL_ICONS = 'Client/Content/Aki/UI/UIResources/Common/Atlas/SkillIcon'

# 原作表里查到的值（RoleInfo / SkillButton / DT_SkillInfo_*），项目角色资产 → 原作数据
ROLES = [
    {
        'asset': '/Game/Characters/Role/Common/Data/DA_Role_Changli',
        'headIcon': 'Client/Content/Aki/UI/UIResources/Common/Image/IconRoleHead150/T_IconRoleHead150_26_UI',
        'elementId': 2,
        'specialEnergyBarId': 16,
        'skillButtons': [
            {'type': 'ResonanceSkill', 'skillId': 1205201, 'icon': f'{SKILL_ICONS}/SkillIconChangli/SP_IconChangliB1'},
            {'type': 'Liberation', 'skillId': 1205401, 'icon': f'{SKILL_ICONS}/SkillIconChangli/SP_IconChangliC1'},
        ],
    },
    {
        'asset': '/Game/Characters/Role/Common/Data/DA_Role_Jiabeilina',
        'headIcon': 'Client/Content/Aki/UI/UIResources/Common/Image/IconRoleHead150/T_IconRoleHead150_55_UI',
        'elementId': 2,
        'specialEnergyBarId': 120800,
        'skillButtons': [
            {'type': 'ResonanceSkill', 'skillId': 1208007, 'icon': f'{SKILL_ICONS}/SkillIconJiaBeiLiNa/SP_IconJiaBeiLiNaB1'},
            {'type': 'Liberation', 'skillId': 1208301, 'icon': f'{SKILL_ICONS}/SkillIconJiaBeiLiNa/SP_IconJiaBeiLiNaC1'},
        ],
    },
]

# ElementInfo.Id → 项目元素标签（Element.* 的中文名见 Config/DefaultGameplayTags.ini）
ELEMENT_TAGS = {1: 'Element.Glacio', 2: 'Element.Fusion', 3: 'Element.Electro', 4: 'Element.Aero', 5: 'Element.Spectro', 6: 'Element.Havoc'}
# DT_SpecialEnergyBarInfo.PrefabType（ESpecialEnergyBarPrefabType::NewEnumeratorN）→ 项目枚举名。
# 原作 RoleSpecialEnergyBar.js：先按 ExtraType 找角色专属类，找不到才按 PrefabType 取通用类
# [Point, Slot, PointGraduate, Morph, Morph, MorphCountDown, ……]；项目只实现了其中几种，其余按 Custom
PREFAB_TYPES = {1: 'Slot', 3: 'Morph', 4: 'Morph'}
# 原作 RoleSpecialEnergyBar 的 specialEnergyBarClassMap（ExtraType → 角色专属类）里项目实现了的；通用类按 Type 选
BAR_CLASSES = {'Slot': '/Script/Wuwa.WuwaSpecialEnergyBarSlot', 'Morph': '/Script/Wuwa.WuwaSpecialEnergyBarMorph'}
EXTRA_BAR_CLASSES = {120800: '/Script/Wuwa.WuwaSpecialEnergyBarJiaBeiLiNa'}
# 原作属性 Id（Proto_SpecialEnergyN）→ 项目 SpecialEnergyN 的 N
SPECIAL_ENERGY_IDS = {62: 1, 64: 2, 66: 3, 68: 4, 45: 5}


def game_path(soft: dict) -> str | None:
    path = soft.get('AssetPathName', 'None')
    return None if path == 'None' else path


def element_rows(textures: ke.Textures) -> list[dict]:
    db = sqlite3.connect(EXPORTS / 'Client/Content/Aki/ConfigDB/db_element_info.db')
    # 颜色按字段名取 ElementColor（原作 BattleUiRoleData 用它给协奏圈、元素图标上色）；一行里有好几种颜色，不能按位置猜
    fields = kuro_fb.parse_js(str(EXPORTS / 'Client/Content/Aki/JavaScript/Core/Define/Config/ElementInfo.js'))
    element_colors = {}
    for element_id, blob in db.execute('select Id, BinData from elementinfo'):
        fb = kuro_fb.FB(blob)
        element_colors[element_id] = kuro_fb.decode_table(fb, fb.u32(0), fields, lambda _name: None)['elementcolor']
    rows = []
    for element_id, blob in db.execute('select Id, BinData from elementinfo order by Id'):
        if element_id not in ELEMENT_TAGS:
            continue
        strings = [s.decode() for s in re.findall(rb'[ -~]{6,}', blob)]
        # 战斗界面用的是 Common/Image/IconElement/T_IconElement<元素>1（HUD_Main 里 TexStateIcon 的默认图就是 T_IconElementLight1）
        icon = next(s for s in strings if re.search(r'/Image/IconElement/T_IconElement\w+1\.', s))
        color = element_colors[element_id]
        rows.append({'tag': ELEMENT_TAGS[element_id], 'id': element_id,
                     'icon': textures.texture(kl.kp.game_to_package(icon.split('.')[0]))['file'], 'color': color})
    return rows


def energy_bar_rows(ids: list[int], textures: ke.Textures) -> list[dict]:
    table = next((EXPORTS / 'Client/Content/Aki/Data/Fight/UI').glob('DT_SpecialEnergyBarInfo*.json'))
    rows = json.loads(table.read_text(encoding='utf-8'))[0]['Rows']
    out = []
    for bar_id in ids:
        raw = rows[str(bar_id)]
        get = lambda field: next(v for k, v in raw.items() if k.split('_')[0] == field)  # noqa: E731
        prefab_type = int(re.search(r'\d+$', get('PrefabType')).group())
        icons = {}
        for field in ('TexturePath', 'EnableTexturePath'):
            path = game_path(get(field))
            icons[field] = textures.texture(kl.kp.game_to_package(path.split('.')[0]))['file'] if path else None
        out.append({
            'id': bar_id,
            'prefab': f'/Game/LGUI/HUD/EnergyBar/EB_{bar_id}',
            'prefabType': 'Custom' if get('ExtraType') else PREFAB_TYPES.get(prefab_type, 'Custom'),
            'barClass': EXTRA_BAR_CLASSES.get(get('ExtraType')) or BAR_CLASSES.get(PREFAB_TYPES.get(prefab_type, ''), ''),
            'specialEnergyIndex': SPECIAL_ENERGY_IDS[get('AttributeId')],
            'disableKeyOnPercent': get('DisableKeyOnPercent'),
            'slotNum': get('SlotNum'),
            'effectColor': get('EffectColor'),
            'pointColor': get('PointColor'),
            'texture': icons['TexturePath'],
            'enableTexture': icons['EnableTexturePath'],
            'extraFloatParams': get('ExtraFloatParams'),
        })
    return out


def main() -> int:
    sys.stdout.reconfigure(encoding='utf-8')
    textures = ke.Textures()
    roles = []
    for role in ROLES:
        roles.append({
            'asset': role['asset'],
            'headIcon': textures.texture(role['headIcon'])['file'],
            'elementTag': ELEMENT_TAGS[role['elementId']],
            'specialEnergyBarId': role['specialEnergyBarId'],
            'skillButtons': [{'type': b['type'], 'skillId': b['skillId'], 'icon': textures.sprite(b['icon'])['file']}
                             for b in role['skillButtons']],
        })
    recipe = {
        'textureDir': '/Game/LGUI/HUD/Kuro/RoleUI/Textures',
        'elementTable': '/Game/LGUI/HUD/RoleUI/DT_ElementInfo',
        'energyBarTable': '/Game/LGUI/HUD/RoleUI/DT_SpecialEnergyBarInfo',
        'roles': roles,
        'elements': element_rows(textures),
        'energyBars': energy_bar_rows(sorted({r['specialEnergyBarId'] for r in ROLES}), textures),
    }
    out = ke.OUT / 'role_ui.recipe.json'
    out.write_text(json.dumps(recipe, ensure_ascii=False, indent=1), encoding='utf-8')
    print(f'{out}: {len(roles)} roles, {len(recipe["elements"])} elements, {len(recipe["energyBars"])} energy bars')
    return 0


if __name__ == '__main__':
    sys.exit(main())
