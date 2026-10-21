#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "WuwaRoleDataAsset.generated.h"

class AWuwaCharacter;

/** 技能按钮的类型；数值和原作 SkillButton 表的按钮类型一致（4 普攻、6 共鸣技能、7 探索工具、8 共鸣解放、9 声骸）。 */
UENUM(BlueprintType)
enum class EWuwaSkillButtonType : uint8
{
	None = 0,
	Attack = 4,
	Dodge = 5,
	ResonanceSkill = 6 UMETA(DisplayName = "Resonance Skill (E)"),
	ExploreTool = 7,
	Liberation = 8 UMETA(DisplayName = "Resonance Liberation (R)"),
	Echo = 9,
};

/** 角色自己的一个技能按钮（≈ 原作 SkillButton 表里这个角色的一行 + DT_SkillInfo 的 SkillIcon）。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaRoleSkillButton
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role|UI")
	EWuwaSkillButtonType ButtonType = EWuwaSkillButtonType::None;

	/** 原作技能 Id（技能表的行名）；项目的技能表还没有这些技能时只用来对照。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role|UI")
	int32 SkillId = 0;

	/** HUD 技能按钮上的图标。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role|UI")
	TSoftObjectPtr<UTexture2D> Icon;
};

/** 角色的共享配置；实际等级、生命值和 Buff 等运行时数据由角色实例或存档保存。 */
UCLASS(BlueprintType)
class WUWA_API UWuwaRoleDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 角色身份，例如 Role.Changli，供 UI、存档和队伍识别角色。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role|Identity", meta = (Categories = "Role"))
	FGameplayTag RoleTag;

	/** 界面显示的角色名称，支持本地化。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role|Identity")
	FText DisplayName;

	/** 生成角色时加载的类；软引用避免仅加载配置时就加载整套角色资源。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role|Resource")
	TSoftClassPtr<AWuwaCharacter> CharacterClass;

	/** 元素，例如 Element.Fusion、Glacio、Electro、Aero、Spectro、Havoc。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role|Identity", meta = (Categories = "Element"))
	FGameplayTag ElementTag;

	/** 新建角色时使用的初始等级；升级时修改角色运行时数据，不修改这个共享资产。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role|Defaults", meta = (ClampMin = "1", UIMin = "1"))
	int32 Level = 1;

	/**
	 * 基础属性：UWuwaRoleSettings::BasePropertyTable 的行名（≈ 原作 RoleInfo.PropertyId，长离 1205）。
	 * 角色生成时按它和 Level 算出血量、攻击、能量上限、元素等，写进 ASC 的属性（UWuwaRoleAttributeInit）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role|Defaults")
	int32 BasePropertyId = 0;
	
	/** 队伍栏里的头像。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role|UI")
	TSoftObjectPtr<UTexture2D> HeadIcon;

	/** 这个角色自己的技能按钮（E、R……）；切到这个角色时 HUD 技能栏换成这里的图标。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role|UI")
	TArray<FWuwaRoleSkillButton> SkillButtons;

	/** 特殊能量条（协奏条上面那条，每个角色不一样）：UWuwaUISettings::SpecialEnergyBarTable 的行名；0 表示没有。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Role|UI")
	int32 SpecialEnergyBarId = 0;

	const FWuwaRoleSkillButton* FindSkillButton(EWuwaSkillButtonType Type) const
	{
		return SkillButtons.FindByPredicate([Type](const FWuwaRoleSkillButton& Button) { return Button.ButtonType == Type; });
	}
};
