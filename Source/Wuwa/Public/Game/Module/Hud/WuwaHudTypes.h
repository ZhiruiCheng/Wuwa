#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "WuwaHudTypes.generated.h"

class ULGUIPrefab;
class UTexture2D;
class UWuwaSpecialEnergyBarBase;

/** 元素的界面配置（≈ 鸣潮 ElementInfo 表），UWuwaUISettings::ElementTable 的一行，行名是元素标签。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaElementUiInfo : public FTableRowBase
{
	GENERATED_BODY()

	/** 战斗界面上的元素图标（角色状态左边的圆里）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Element")
	FColor Color = FColor::White;
};

/** 特殊能量条预制体的种类（≈ 鸣潮 ESpecialEnergyBarPrefabType）。 */
UENUM(BlueprintType)
enum class EWuwaSpecialEnergyBarType : uint8
{
	/** 一排格子（UiItem_BarSlotN）。 */
	Slot,
	/** 变身外框 + 格子（UiItem_BarPointMorp）。 */
	Morph,
	/** 角色专属的预制体。 */
	Custom,
};

/** 角色的特殊能量条（≈ 鸣潮 DT_SpecialEnergyBarInfo），UWuwaUISettings::SpecialEnergyBarTable 的一行，行名是能量条 Id。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaSpecialEnergyBarInfo : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EnergyBar")
	EWuwaSpecialEnergyBarType Type = EWuwaSpecialEnergyBarType::Slot;

	/**
	 * 能量条的逻辑类（≈ 原作 RoleSpecialEnergyBar：先按 ExtraType 找角色专属类，找不到按 PrefabType 用通用类）。
	 * 通用：UWuwaSpecialEnergyBarSlot（格子）、UWuwaSpecialEnergyBarMorph（变身外框 + 格子）；专属：UWuwaSpecialEnergyBarJiaBeiLiNa ……
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EnergyBar")
	TSubclassOf<UWuwaSpecialEnergyBarBase> BarClass;

	/** 读哪一对特殊能量属性：1~5 → UWuwaAttributeSet::SpecialEnergyN / MaxSpecialEnergyN（≈ 原作 AttributeId / MaxAttributeId）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EnergyBar", meta = (ClampMin = "1", ClampMax = "5"))
	int32 SpecialEnergyIndex = 1;

	/** 能量比例低于这个百分比时按键不可用（≈ 原作 DisableKeyOnPercent），满格效果、变身外框的"可用"贴图都看它。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EnergyBar")
	float DisableKeyOnPercent = 100.f;

	/** 能量条预制体，生成在 HUD 角色状态的 PnlEnergyBar 下面。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EnergyBar")
	TSoftObjectPtr<ULGUIPrefab> Prefab;

	/** 格数。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EnergyBar")
	int32 SlotNum = 0;

	/** 外框贴图（Morph：平时）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EnergyBar")
	TSoftObjectPtr<UTexture2D> Texture;

	/** 外框贴图（Morph：能量满、可以释放时）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EnergyBar")
	TSoftObjectPtr<UTexture2D> EnableTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EnergyBar")
	FColor EffectColor = FColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EnergyBar")
	FColor PointColor = FColor::White;

	/** 专属预制体用的参数（比如嘉贝莉娜的 [60, 100]），含义由预制体决定。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EnergyBar")
	TArray<float> ExtraFloatParams;
};
