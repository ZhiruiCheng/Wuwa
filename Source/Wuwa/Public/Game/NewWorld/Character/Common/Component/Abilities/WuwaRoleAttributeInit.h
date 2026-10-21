#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "Engine/DataTable.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayEffect.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaElement.h"
#include "WuwaRoleAttributeInit.generated.h"

class UAbilitySystemComponent;
class UWuwaRoleDataAsset;

/**
 * 角色 1 级的基础属性（≈ 原作 db_property 的 BaseProperty 表），UWuwaRoleSettings::BasePropertyTable 的一行，行名是原作 PropertyId
 * （= UWuwaRoleDataAsset::BasePropertyId，长离 1205、嘉贝莉娜 1208）。由 -run=WuwaRoleAttributeSetup 从原作数据生成。
 * 比率类换成了小数（原作万分比），能量类保持原作单位。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaRoleBaseProperty : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes") EWuwaElement Element = EWuwaElement::None;
	/** 下面三项会按等级乘成长倍率（FWuwaRolePropertyGrowth）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes") float MaxHealth = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes") float Attack = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes") float Defense = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes") float CritRate = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes") float CritDamage = 1.f;
	/** 共鸣效率，小数（1 = 100%）：命中回能乘它。原作客户端表里是空的，默认 100%。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes", meta = (ClampMin = "0.0")) float EnergyEfficiency = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes") float MaxEnergy = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes") float MaxConcertoEnergy = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes") float MaxSpecialEnergy1 = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes") float MaxSpecialEnergy2 = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes") float MaxSpecialEnergy3 = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes") float MaxSpecialEnergy4 = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes") float MaxSpecialEnergy5 = 0.f;
};

/**
 * 角色属性的等级成长（≈ 原作 RolePropertyGrowth 表），UWuwaRoleSettings::PropertyGrowthTable 的一行，行名 "<等级>_<突破>"。
 * 某一级的数值 = 向下取整(1 级基础值 × 倍率)（长离 90 级 831 × 12.5 → 10387，和游戏里一致）。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaRolePropertyGrowth : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth") int32 Level = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth") int32 BreachLevel = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth") float MaxHealthRatio = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth") float AttackRatio = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Growth") float DefenseRatio = 1.f;
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Wuwa Role"))
class WUWA_API UWuwaRoleSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, meta = (RequiredAssetDataTags = "RowStructure=/Script/Wuwa.WuwaRoleBaseProperty"))
	TSoftObjectPtr<UDataTable> BasePropertyTable;

	UPROPERTY(Config, EditAnywhere, meta = (RequiredAssetDataTags = "RowStructure=/Script/Wuwa.WuwaRolePropertyGrowth"))
	TSoftObjectPtr<UDataTable> PropertyGrowthTable;
};

/**
 * 初始化角色属性的 GE：Instant，每个属性一个 Override 修改器，数值走 SetByCaller（标签 SetByCaller.RoleInit.<属性名>）。
 * 由 UWuwaRoleAttributeInit::Apply 填数值后对角色自己应用（服务端）。
 */
UCLASS()
class WUWA_API UWuwaInitRoleAttributesEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UWuwaInitRoleAttributesEffect();
};

/** 按角色资产算出初始属性并写进 ASC（≈ 原作角色生成时按 BaseProperty × 成长初始化 AttributeComponent）。 */
UCLASS()
class WUWA_API UWuwaRoleAttributeInit : public UObject
{
	GENERATED_BODY()

public:
	/** 初始属性：属性 → 值。找不到表或行时返回 false（OutError 说明原因）。 */
	static bool Compute(const UObject* WorldContext, const UWuwaRoleDataAsset* Role, TArray<TPair<FGameplayAttribute, float>>& OutValues, FString& OutError);

	/** 只在服务端调用：算出初始属性，用 UWuwaInitRoleAttributesEffect 应用到 ASC。 */
	static bool Apply(UAbilitySystemComponent* ASC, const UWuwaRoleDataAsset* Role);
};
