#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"
#include "WuwaCombatSettings.generated.h"

class UDataTable;

/** 项目设置 > Game > Wuwa Combat。 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Wuwa Combat"))
class WUWA_API UWuwaCombatSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** 阵营表（行结构 FWuwaCampRow）。 */
	UPROPERTY(Config, EditAnywhere, Category = "Camp", meta = (RequiredAssetDataTags = "RowStructure=/Script/Wuwa.WuwaCampRow"))
	TSoftObjectPtr<UDataTable> CampTable;

	/** 伤害表（行结构 FWuwaDamageInfo，行名 = 伤害 ID）。 */
	UPROPERTY(Config, EditAnywhere, Category = "Damage", meta = (RequiredAssetDataTags = "RowStructure=/Script/Wuwa.WuwaDamageInfo"))
	TSoftObjectPtr<UDataTable> DamageTable;

	/** buff 表（行结构 FWuwaBuffRow，行名 = 原作 buff ID → GE）。UWuwaBuffLibrary::AddBuff 等按 buff ID 查它。 */
	UPROPERTY(Config, EditAnywhere, Category = "Buff", meta = (RequiredAssetDataTags = "RowStructure=/Script/Wuwa.WuwaBuffRow"))
	TSoftObjectPtr<UDataTable> BuffTable;

	/** buff 表现表（行结构 FWuwaBuffCueRow，行名 = 原作 GameplayCue ID）：buff 图标等。 */
	UPROPERTY(Config, EditAnywhere, Category = "Buff", meta = (RequiredAssetDataTags = "RowStructure=/Script/Wuwa.WuwaBuffCueRow"))
	TSoftObjectPtr<UDataTable> BuffCueTable;

	/** 锁定参数表（行结构 FWuwaLockOnConfig，行名 = 原作锁定配置 ID）。 */
	/**
	 * 测试用伤害倍率（不是原作数据）：最终伤害再乘它。项目角色还没有武器、声骸、技能树，攻击只有角色本体的，
	 * 打 90 级 Boss 几乎不掉血；正式数值接上后改回 1。DefaultGame.ini 里配，控制台 wuwa.Damage.Scale 可临时改。
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Damage", meta = (ClampMin = "0.0"))
	float DamageScale = 1.f;

	/**
	 * 测试用命中回能倍率（不是原作数据）：共鸣能量回复 = 伤害表 Energy × 共鸣效率 × 它。项目没有变奏 / 延奏 / 声骸 / 武器回能，
	 * 按原作数值要打很多轮才能放大招；正式数值接上后改回 1。DefaultGame.ini 里配，控制台 wuwa.Damage.EnergyScale 可临时改。
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Damage", meta = (ClampMin = "0.0"))
	float EnergyGainScale = 1.f;

	UPROPERTY(Config, EditAnywhere, Category = "Lock On", meta = (RequiredAssetDataTags = "RowStructure=/Script/Wuwa.WuwaLockOnConfig"))
	TSoftObjectPtr<UDataTable> LockOnConfigTable;

	/**
	 * 游戏设置"技能锁敌模式"：技能目标优先级为"系统设置"时用哪种挑法。原作只有三档：摇杆方向优先 / 镜头方向优先 / 距离优先，
	 * 默认摇杆方向优先（原作 db_menu MenuConfig SkillLockEnemyMode 默认选项 0）。
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Lock On")
	EWuwaSkillTargetPriority SkillLockEnemyMode = EWuwaSkillTargetPriority::InputDirection;

	/** 原作通用参数 LockOnOffset（厘米）：算夹角时起点沿参考方向往回退这么远，贴身的目标夹角不会被放大（精准模式不退）。 */
	UPROPERTY(Config, EditAnywhere, Category = "Lock On")
	float LockOnOffset = 200.f;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
