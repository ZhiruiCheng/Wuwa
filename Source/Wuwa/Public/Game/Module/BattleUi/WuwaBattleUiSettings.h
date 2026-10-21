#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/DeveloperSettings.h"
#include "WuwaBattleUiSettings.generated.h"

class ULGUIPrefab;

/**
 * 战斗界面（Boss 血条、伤害数字）的配置。数值来自原作 ConfigDB db_common_param.db（CommonParam），单位换成项目的（厘米、秒）。
 * DefaultGame.ini 的 [/Script/Wuwa.WuwaBattleUiSettings]。
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Wuwa Battle UI"))
class WUWA_API UWuwaBattleUiSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** 伤害数字预制体（原作 UiItem_DamageView_Prefab）。 */
	UPROPERTY(Config, EditAnywhere, Category = "Damage")
	TSoftObjectPtr<ULGUIPrefab> DamageViewPrefab;

	/** 伤害飘字配置表（原作 DamageText），行结构 FWuwaDamageTextInfo。 */
	UPROPERTY(Config, EditAnywhere, Category = "Damage", meta = (RequiredAssetDataTags = "RowStructure=/Script/Wuwa.WuwaDamageTextInfo"))
	TSoftObjectPtr<UDataTable> DamageTextTable;

	/** buff 限时图标（原作 UiItem_BuffItem，BattleUiPool buffItemConfig）。 */
	UPROPERTY(Config, EditAnywhere, Category = "Buff")
	TSoftObjectPtr<ULGUIPrefab> BuffItemPrefab;

	/** 角色专属 buff 栏（血条上方）最多显示几个图标（原作 CommonParam RoleUniqueBuffItemCount）。 */
	UPROPERTY(Config, EditAnywhere, Category = "Buff", meta = (ClampMin = "1"))
	int32 RoleUniqueBuffItemCount = 10;

	/** Boss 血条预制体（原作 UiItem_BossState_Prefab，BossStateViewType 1）。 */
	UPROPERTY(Config, EditAnywhere, Category = "Boss")
	TSoftObjectPtr<ULGUIPrefab> BossStatePrefab;

	/** 离 Boss 多近开始显示血条（原作 BossStateShowDistance，存的是平方 56250000 → 7500 厘米）。 */
	UPROPERTY(Config, EditAnywhere, Category = "Boss", meta = (Units = "cm"))
	float BossStateShowDistance = 7500.f;

	/** 已显示的血条超过这个距离才收起（原作 BossStateShowMaxDistance，同样是平方）。 */
	UPROPERTY(Config, EditAnywhere, Category = "Boss", meta = (Units = "cm"))
	float BossStateShowMaxDistance = 7500.f;

	/** 受击残影：掉血后残影停多久再开始缩（原作 HitDelayTime 250 毫秒）。 */
	UPROPERTY(Config, EditAnywhere, Category = "Boss", meta = (Units = "s"))
	float HitDelayTime = 0.25f;

	/** 停顿期间连续受击超过这么多次就立刻开始缩（原作 HitRefreshTime）。 */
	UPROPERTY(Config, EditAnywhere, Category = "Boss")
	int32 HitRefreshTime = 8;

	/** 残影缩到当前血量用多久（原作 HitBufferDisappearTime 250 毫秒）。 */
	UPROPERTY(Config, EditAnywhere, Category = "Boss", meta = (Units = "s"))
	float HitBufferDisappearTime = 0.25f;

	/** 一次掉血超过血量上限的这个比例播大受击动画，否则小受击（原作 HitLargeBufferPercent 1500 万分比）。 */
	UPROPERTY(Config, EditAnywhere, Category = "Boss")
	float HitLargeBufferPercent = 0.15f;

	/** 等级文字的威胁色（原作 ThreatLevel1/3、ThreatLevelColor1~3）：最高角色等级 − 怪物等级 ≥ Level1 白色，≥ Level3 黄色，更低红色。 */
	UPROPERTY(Config, EditAnywhere, Category = "Boss")
	int32 ThreatLevel1 = 0;
	UPROPERTY(Config, EditAnywhere, Category = "Boss")
	int32 ThreatLevel3 = -5;
	UPROPERTY(Config, EditAnywhere, Category = "Boss")
	FColor ThreatLevelColor1 = FColor(0xE2, 0x52, 0x4C);
	UPROPERTY(Config, EditAnywhere, Category = "Boss")
	FColor ThreatLevelColor2 = FColor(0xDA, 0xB2, 0x44);
	UPROPERTY(Config, EditAnywhere, Category = "Boss")
	FColor ThreatLevelColor3 = FColor::White;

	/** 等级文字格式（原作文本 LevelShow = "Lv. {0}"）。 */
	UPROPERTY(Config, EditAnywhere, Category = "Boss")
	FString LevelFormat = TEXT("Lv. {0}");
};
