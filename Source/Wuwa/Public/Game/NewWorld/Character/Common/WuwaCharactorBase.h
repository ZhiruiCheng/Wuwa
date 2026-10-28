// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "AttributeSet.h"
#include "GameFramework/Character.h"
#include "WuwaCharactorBase.generated.h"

class UWuwaMovementComponent;
class UGameplayAbility;
class UWuwaAbilitySystemComponent;
class UWuwaAttributeSet;
class UWuwaCombatAttributeSet;
class UWuwaHitComponent;
class UAnimMontage;
class UAnimSequenceBase;
class UAbilitySystemComponent;
class UAttributeSet;

class UDataTable;

/** 一个属性的初始值（怪物等没有角色属性初始化流程的单位用）。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaInitialAttribute
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute")
	FGameplayAttribute Attribute;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute")
	float Value = 0.f;
};

/**
 * Boss 血条配置（≈ 原作怪物模板 MonsterComponent.BossViewConfig）。BossStateViewType 非 0 的怪物靠近时在屏幕上方显示大血条
 * （1 = 普通 Boss 血条 UiItem_BossState，2 双血条、3 多层血条还没做）。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaBossViewConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	int32 BossStateViewType = 0;

	/** 显示距离（厘米）；0 用战斗界面配置的默认距离（原作 BossStateShowDistance）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss", meta = (Units = "cm"))
	float ShowDistance = 0.f;

	/** 只在战斗中显示（原作 OnlyShowInBattleState，项目还没有战斗状态标签，暂不生效）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	bool bOnlyShowInBattleState = false;
};

UCLASS()
class WUWA_API AWuwaCharactorBase : public ACharacter,public IAbilitySystemInterface
{
	GENERATED_BODY()

#pragma  region LifyCycle
public:
	AWuwaCharactorBase(const FObjectInitializer& ObjectInitializer);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
#pragma endregion
	
#pragma region ReplicateOverride
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void OnRep_Controller() override;
#pragma endregion
	
#pragma region ASC
	UPROPERTY()
	TObjectPtr<UWuwaAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY()
	TObjectPtr<UWuwaAttributeSet> AttributeSet;

	/** 伤害公式用的加成 / 抗性属性。 */
	UPROPERTY()
	TObjectPtr<UWuwaCombatAttributeSet> CombatAttributeSet;
	
	void RefreshAbilityActorInfo() const;
	
	/** 技能表（行结构 FWuwaSkillInfo，行名 = 技能 ID）。角色的技能都从这里授予。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Skill", meta = (RequiredAssetDataTags = "RowStructure=/Script/Wuwa.WuwaSkillInfo"))
	TObjectPtr<UDataTable> SkillTable;

	/** 子弹表（行结构 FWuwaBulletInfo，行名 = 子弹 ID）。动画通知 Spawn Bullet 里的子弹 ID 在这张表里查。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Skill", meta = (RequiredAssetDataTags = "RowStructure=/Script/Wuwa.WuwaBulletInfo"))
	TObjectPtr<UDataTable> BulletTable;

	/** 受击效果表（行结构 FWuwaHitEffectInfo，≈ 原作 Data/DT_HitEffect）：这个角色的子弹打中别人时，按子弹的 被击效果 在这里查受击动作。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Hit", meta = (RequiredAssetDataTags = "RowStructure=/Script/Wuwa.WuwaHitEffectInfo"))
	TObjectPtr<UDataTable> HitEffectTable;

	/** 受击蒙太奇：受击动作（EWuwaHitAnim 的值）→ 蒙太奇（≈ 原作 AI 状态机里 TaskBeHitMontage 的 MontageMap）。没有的受击动作不播。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Hit")
	TMap<int32, TSoftObjectPtr<UAnimMontage>> BeHitMontages;

	/** 怪物的待机循环（UWuwaMonsterAnimInstance 用；还没有怪物动画蓝图时用它 + DefaultSlot 播蒙太奇）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Hit")
	TObjectPtr<UAnimSequenceBase> IdleAnimation;

	/** 受击：韧性、顿帧、受击动作。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Hit")
	TObjectPtr<UWuwaHitComponent> HitComponent;

	/**
	 * 阵营（阵营表 DT_Camp 的行名，≈ 原作实体的 Camp）。默认 Player（原作 TsBaseCharacter.Camp 默认 0 = Player）；怪物设 Monster。
	 * 敌我判断见 UWuwaCampLibrary::IsHostile。同一个蓝图的阵营固定，不复制。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat", meta = (GetOptions = "GetCampOptions"))
	FName Camp = TEXT("Player");

	UFUNCTION()
	static TArray<FName> GetCampOptions();

	/** 软锁定参数（锁定参数表行名）：技能表 LockOnConfigId 为 0 时挑技能目标用它。原作 RoleInfo.LockOnDefaultId（长离 / 嘉贝 = 1）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat")
	int32 SoftLockConfigId = 1;

	/** 硬锁定参数（玩家手动锁定，还没做）。原作 RoleInfo.LockOnLookOnId（长离 / 嘉贝 = 2）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat")
	int32 HardLockConfigId = 2;

	/** 显示名（原作模板 BaseInfoComponent.TidName 的文本），Boss 血条等界面用。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|UI")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|UI")
	FWuwaBossViewConfig BossViewConfig;

	/** 技能等级（伤害表倍率按它取，1~20）。技能树还没做，所有技能共用这一个。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat", meta = (ClampMin = "1", ClampMax = "20"))
	int32 SkillLevel = 1;

	/**
	 * 开始游戏时（服务器）直接写入的属性初始值。角色走 UWuwaRoleAttributeInit（按原作基础属性 × 成长），不填这里；
	 * 怪物用它（WuwaMonsterSetup 按原作基础属性 × 怪物成长曲线算好写进来）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat")
	TArray<FWuwaInitialAttribute> InitialAttributes;

	/** 不属于技能的其他能力（没有技能 ID，不参与输入选招）。技能请放进 SkillTable。 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category = "GAS|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> CharacterAbilities;

	UFUNCTION(BlueprintCallable, Category = "Wuwa|ASC")
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
#pragma endregion
	
	
#pragma region Init
	virtual void InitInitialAbilities();
#pragma endregion
	
#pragma region Movement

	//自定义移动组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Components")
	TObjectPtr<UWuwaMovementComponent> WuwaMovementComponent;
	
	FORCEINLINE TObjectPtr<UWuwaMovementComponent> GetWuwaMovementComponent() const { return WuwaMovementComponent; }
#pragma endregion

protected:
	
	
};
