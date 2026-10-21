// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaPlayerInputState.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateTypes.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"
#include "WuwaGameplayAbilityBase.generated.h"

class UWuwaSkillBridgeComponent;

/**
 * 
 */
UCLASS()
class WUWA_API UWuwaGameplayAbilityBase : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	/** 这次执行对应的技能 ID（技能表行名）；不是按技能表授予的能力返回 0。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Skill")
	int32 GetSkillId() const;

	/** 本项目主动动作默认属于主动技能；被动/辅助 GA 应关闭。与移动占用独立。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Combat|Skill")
	bool bIsMainSkill = true;

	//打断等级
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Combat|Skill", meta = (EditCondition = "bIsMainSkill", ClampMin = "0", ClampMax = "255"))
	int32 InterruptLevel = 100;

	//被打断等级
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Combat|Skill", meta = (EditCondition = "bIsMainSkill"))
	EWuwaSkillOverrideType SkillOverrideType = EWuwaSkillOverrideType::None;

	//申请为主技能时返回的句柄
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Skill")
	int32 GetSkillHandle() const { return FightStateHandle; }


	/** 脚本管理器使用的 GAS 执行接口；不包含技能优先级规则。 */
	
	//是否正在被正常执行
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Skill")
	bool IsSkillExecutionActive() const;
	
	//是否正在为输入的Avatar执行
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Skill")
	bool IsSkillExecutionFor(AActor* ExpectedAvatar) const;
	
	//是否可以现在就结束,前提是当前的GA正在执行,并且没有被其他的程序上锁
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Skill")
	bool CanEndSkillExecutionNow() const;
	
	//判断是否能够终止,从引擎层和脚本的技能句柄的双重判断
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Combat|Skill")
	bool TryEndSkillExecution(int32 ExpectedHandle);

	/**
	 * 技能开始时写入运动状态的移动状态（如 Dash → Dodge），结束后让 RoleGait 立即重算一次；Other 表示不写入。
	 * 与原作一样只写入、不占用：之后 RoleGait 按普通规则覆盖（有方向输入时）或在停稳后转为 Stand。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wuwa|State")
	EWuwaMoveState StartMoveState = EWuwaMoveState::Other;

	/**
	 * 在 GA 实例中读取当前 Avatar 的移动输入，无角色上下文时返回零值。
	 * 本地输入不自动复制。CanActivateAbility 中应通过其 ActorInfo 参数查询角色，
	 * 不依赖这个实例入口；需要持续输入时重新读取，不缓存返回值。
	 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Input")
	FWuwaPlayerInputState GetPlayerInputState() const;
	
	/**
	 * 仅在移动取消已经获准时调用；窗口判定仍由 GA 负责。
	 * 有移动输入时，先禁用本 GA 正在播放的蒙太奇实例的根运动，再开始姿势混出。
	 * 成功返回 true 后由调用方结束 GA；无输入或播放归属不匹配时不作任何修改。
	 * 保留 Root Motion from Everything，其他动画（如 Run_End）不受影响。
	 * 当前供本地验证；额外的根运动禁用状态不自动参与网络预测/复制。
	 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Movement", meta = (ClampMin = "0.0", AdvancedDisplay = "BlendOutTime"))
	bool StopMontageForMovement(float BlendOutTime = 0.1f);

	
	/** 先查询当前 Avatar 的主技能让位规则和动作状态的位置合法性，成功后保留 GAS 自身的激活检查。 */
	virtual  bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
		const FGameplayEventData* TriggerEventData = nullptr) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	/** 技能开始：技能表配置了锁定就挑技能目标（存到技能组件），再按 SkillDirection 瞬间转向。ActivateAbility 里、播蒙太奇之前调用。 */
	void BeginSkillTarget();
	/** 技能表 SkillStartBuffs：技能开始时给自己加的 buff。 */
	void ApplySkillStartBuffs();
	/** 技能表 SkillEndBuffs：技能结束时（正常结束、被打断都算）给自己加的 buff。 */
	void ApplySkillEndBuffs();
	/** 技能表 bWalkOffLedge：技能期间开边缘保护（原作 CharacterSkillComponent 技能开始 / DoSkillEndMoveAction）。 */
	void BeginSkillWalkOffLedge();
	void EndSkillWalkOffLedge();
	/** 技能表 ActionTags：技能期间加到自己身上（松散 Tag 计数 +1，结束 -1）。 */
	void BeginSkillActionTags();
	void EndSkillActionTags();

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	bool WritesStartMoveState() const { return StartMoveState != EWuwaMoveState::Other; }

	// 本次主技能登记的技能组件和 FightState 句柄（原作 Skill.FightStateHandle），结束时用它回报 EndSkill。
	TWeakObjectPtr<UWuwaSkillBridgeComponent> SkillOwner;
	int32 FightStateHandle = 0;
	
	//防止重入导致的重复激活
	uint64 ActivationSerial = 0;

	/** 本次执行申请过边缘保护（结束时归还给同一个移动组件）。 */
	TWeakObjectPtr<class UWuwaMovementComponent> WalkOffLedgeMovement;
	/** 本次执行加过的动作标识（结束时从同一个 ASC 上减掉）。 */
	TWeakObjectPtr<UAbilitySystemComponent> ActionTagsASC;
	FGameplayTagContainer AppliedActionTags;
};
