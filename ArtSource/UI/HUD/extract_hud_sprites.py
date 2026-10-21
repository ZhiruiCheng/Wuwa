"""从鸣潮 UI 资源里提取 HUD 用的精灵图，存到 Textures/ 下，再由 -run=WuwaHudAssetSetup 导入成 /Game/LGUI/HUD/Textures 里的贴图。

图集没有附带切片数据：box 是按 alpha 连通区域切出来的裁剪框 [x, y, w, h]（像素，图集左上角为原点）。
这些是从游戏里提取的资源，只在本地使用，不要提交到公开仓库。

usage: python extract_hud_sprites.py [UIResources 目录]
"""
import os
import sys

from PIL import Image

DEFAULT_ROOT = r'C:/GamePakExtractor/WutheringWaves-UIResources-3.6/UIResources'

ENTRIES = [
    {"name": "T_HUD_Icon_Terminal", "source": "UiFight/Atlas/T_TPI_UiFight2_UIAtlas.png", "box": [928, 374, 71, 68]},
    {"name": "T_HUD_Icon_Chess", "source": "UiFight/Atlas/T_TPI_UiFight2_UIAtlas.png", "box": [862, 738, 53, 71]},
    {"name": "T_HUD_Icon_Cards", "source": "UiFight/Atlas/T_TPI_UiFight2_UIAtlas.png", "box": [759, 721, 65, 69]},
    {"name": "T_HUD_Icon_Social", "source": "UiFight/Atlas/T_TPI_UiFight2_UIAtlas.png", "box": [855, 561, 74, 69]},
    {"name": "T_HUD_Icon_Quest", "source": "UiFight/Atlas/T_TPI_UiFight2_UIAtlas.png", "box": [957, 559, 56, 67]},
    {"name": "T_HUD_Icon_Gacha", "source": "UiFight/Atlas/T_TPI_UiFight2_UIAtlas.png", "box": [1193, 176, 68, 75]},
    {"name": "T_HUD_Icon_Event", "source": "UiFight/Atlas/T_TPI_UiFight2_UIAtlas.png", "box": [842, 375, 59, 67]},
    {"name": "T_HUD_Icon_Guide", "source": "UiFight/Atlas/T_TPI_UiFight2_UIAtlas.png", "box": [896, 277, 80, 81]},
    {"name": "T_HUD_Icon_Radio", "source": "UiFight/Atlas/T_TPI_UiFight2_UIAtlas.png", "box": [760, 813, 71, 70]},
    {"name": "T_HUD_Icon_Resonator", "source": "UiFight/Atlas/T_TPI_UiFight2_UIAtlas.png", "box": [1044, 554, 60, 74]},
    {"name": "T_HUD_Icon_Menu", "source": "UiFight/Atlas/T_TPI_UiFight2_UIAtlas.png", "box": [834, 461, 75, 76]},
    {"name": "T_HUD_RedDot", "source": "UiFight/Atlas/T_TPI_UiFight2_UIAtlas.png", "box": [832, 920, 28, 28]},
    {"name": "T_HUD_MiniMap_Frame", "source": "UiFight/Atlas/T_TPI_UiFight1_UIAtlas.png", "box": [994, 30, 290, 290]},
    {"name": "T_HUD_MiniMap_Bg", "source": "UiFight/Atlas/T_TPI_UiFight1_UIAtlas.png", "box": [1774, 170, 254, 253]},
    {"name": "T_HUD_MiniMap_PlayerArrow", "source": "UiFight/Atlas/T_TPI_UiFight1_UIAtlas.png", "box": [763, 1576, 78, 83]},
    {"name": "T_HUD_QuestTrack_Icon", "source": "Common/Atlas/TabIcon/T_TPI_Common_TabIcon2_UIAtlas.png", "box": [1032, 699, 67, 62]},
    {"name": "T_HUD_Circle", "source": "UiFight/Atlas/T_TPI_UiFight1_UIAtlas.png", "box": [1471, 1994, 46, 46]},
    {"name": "T_HUD_Chat", "source": "UiFight/Atlas/T_TPI_UiFight1_UIAtlas.png", "box": [1213, 432, 69, 59]},
    {"name": "T_HUD_Subtitle_Bg", "source": "UiFight/Atlas/T_TPI_UiFight1_UIAtlas.png", "box": [1356, 786, 176, 40]},
    {"name": "T_HUD_Status_Emblem", "source": "UiFight/Atlas/T_TPI_UiFight1_UIAtlas.png", "box": [1916, 480, 128, 117]},
    {"name": "T_HUD_Status_Line", "source": "UiFight/Atlas/T_TPI_UiFight1_UIAtlas.png", "box": [412, 928, 451, 57]},
    {"name": "T_HUD_Skill_Bg", "source": "UiFight/Atlas/T_TPI_UiFight1_UIAtlas.png", "box": [1932, 904, 92, 91]},
    {"name": "T_HUD_Skill_KeyFrame", "source": "UiFight/Atlas/T_TPI_UiFight2_UIAtlas.png", "box": [1205, 608, 62, 64]},
    {"name": "T_HUD_Skill_ReadyRing", "source": "UiFight/Atlas/T_TPI_UiFight1_UIAtlas.png", "box": [1223, 1903, 112, 113]},
    {"name": "T_HUD_Team_Frame", "source": "UiFight/Atlas/T_TPI_UiFight2_UIAtlas.png", "box": [501, 695, 143, 143]},
    {"name": "T_HUD_Square", "source": "UiFight/Atlas/T_TPI_UiFight1_UIAtlas.png", "box": [1990, 2006, 36, 36]},
    {"name": "T_HUD_Skill_Changli_00", "source": "Common/Atlas/SkillIcon/SkillIconChangli/T_TPI_Common_SkillIconChangli1_UIAtlas.png", "box": [10, 11, 116, 115]},
    {"name": "T_HUD_Skill_Changli_01", "source": "Common/Atlas/SkillIcon/SkillIconChangli/T_TPI_Common_SkillIconChangli1_UIAtlas.png", "box": [148, 13, 107, 114]},
    {"name": "T_HUD_Skill_Changli_02", "source": "Common/Atlas/SkillIcon/SkillIconChangli/T_TPI_Common_SkillIconChangli1_UIAtlas.png", "box": [275, 9, 114, 118]},
    {"name": "T_HUD_Skill_Changli_03", "source": "Common/Atlas/SkillIcon/SkillIconChangli/T_TPI_Common_SkillIconChangli1_UIAtlas.png", "box": [405, 10, 118, 115]},
    {"name": "T_HUD_Skill_Changli_04", "source": "Common/Atlas/SkillIcon/SkillIconChangli/T_TPI_Common_SkillIconChangli1_UIAtlas.png", "box": [538, 9, 116, 118]},
    {"name": "T_HUD_Skill_Changli_05", "source": "Common/Atlas/SkillIcon/SkillIconChangli/T_TPI_Common_SkillIconChangli1_UIAtlas.png", "box": [671, 12, 114, 113]},
    {"name": "T_HUD_Skill_Changli_06", "source": "Common/Atlas/SkillIcon/SkillIconChangli/T_TPI_Common_SkillIconChangli1_UIAtlas.png", "box": [801, 15, 118, 106]},
    {"name": "T_HUD_Skill_Changli_07", "source": "Common/Atlas/SkillIcon/SkillIconChangli/T_TPI_Common_SkillIconChangli1_UIAtlas.png", "box": [933, 10, 118, 117]},
    {"name": "T_HUD_Skill_Changli_08", "source": "Common/Atlas/SkillIcon/SkillIconChangli/T_TPI_Common_SkillIconChangli1_UIAtlas.png", "box": [1066, 16, 116, 107]},
    {"name": "T_HUD_Skill_Changli_09", "source": "Common/Atlas/SkillIcon/SkillIconChangli/T_TPI_Common_SkillIconChangli1_UIAtlas.png", "box": [1198, 9, 116, 117]},
    {"name": "T_HUD_HpBar_Fill", "source": "UiFight/Image/HPBar/T_HpFBar.png"},
    {"name": "T_HUD_HpBar_Frame", "source": "UiFight/Image/HPBar/T_HpFBarEffect.png"},
    {"name": "T_HUD_HpBar_Light", "source": "UiFight/Image/HPBar/T_HpFBarLight.png"},
    {"name": "T_HUD_HpBar_Handle", "source": "UiFight/Image/HPBar/T_HpFBarHandle.png"},
    {"name": "T_HUD_Signal_1", "source": "Common/Atlas/_SpriteTextures/SP_CommonSignal01.png"},
    {"name": "T_HUD_Signal_2", "source": "Common/Atlas/_SpriteTextures/SP_CommonSignal02.png"},
    {"name": "T_HUD_Signal_3", "source": "Common/Atlas/_SpriteTextures/SP_CommonSignal03.png"},
    {"name": "T_HUD_Signal_4", "source": "Common/Atlas/_SpriteTextures/SP_CommonSignal04.png"},
    {"name": "T_HUD_Team_Head_Changli", "source": "Common/Image/IconRoleHeadCircle256/T_IconRoleHeadCircle256_26_UI.png"}
]


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_ROOT
    out_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Textures')
    os.makedirs(out_dir, exist_ok=True)
    for entry in ENTRIES:
        image = Image.open(os.path.join(root, entry['source'])).convert('RGBA')
        if 'box' in entry:
            x, y, w, h = entry['box']
            image = image.crop((x, y, x + w, y + h))
        image.save(os.path.join(out_dir, entry['name'] + '.png'))
    print(f'{len(ENTRIES)} sprites -> {out_dir}')


if __name__ == '__main__':
    main()
