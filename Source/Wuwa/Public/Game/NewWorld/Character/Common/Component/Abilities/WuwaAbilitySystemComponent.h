// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"
#include "WuwaAbilitySystemComponent.generated.h"

/**
 * 
 */

struct FWuwaInputEvent;
class UWuwaGameplayAbilityBase;
class AWuwaCharacter;
DECLARE_MULTICAST_DELEGATE_OneParam(FAttributeEffectApplied, const FGameplayEffectSpec&);

UENUM(BlueprintType)
enum class EWuwaAbilityRequestResult : uint8
{
	InvalidHandle,        // 这个技能已不存在或正在移除
	AlreadyActive,        // 这个技能已经在执行
	ActivationRequested,  // GAS 接受了激活请求
	Rejected              // GAS 拒绝了激活请求
};

UCLASS()
class WUWA_API UWuwaAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
public:
	
	/** 当前负责 ASC 蒙太奇播放的 GA 实例；没有播放归属时返回 nullptr。供脚本通知查询。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Animation")
	UWuwaGameplayAbilityBase* GetAnimatingWuwaAbility() const;

	#pragma region EffectApplied
	//当有 GameplayEffect 被成功应用到自己身上时执行这个函数
	void InitAbilitySystemCompoent();
	//收到影响触发的回调
	void EffectApplied(UAbilitySystemComponent* ASC, const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle GameplayEffectHandle);
	//广播函数
	FAttributeEffectApplied AttributeEffectAppliedDelegate;
	#pragma endregion
	
	//C++层面基本不作逻辑判断,具体业务逻辑都交给热更脚本书写
	#pragma region Skill
	/**
	 * 按技能表的一行授予技能（只在服务器调用）。技能 ID 是身份：同一个 ASC 上一个 ID 只对应一个 Spec，重复授予被拒绝。
	 * 返回新 Spec 的句柄；失败返回无效句柄。
	 */
	FGameplayAbilitySpecHandle GiveSkill(int32 SkillId, const FWuwaSkillInfo& Info, UObject* SourceObject);

	/** 按技能 ID 找已授予、可用的 Spec（0 或 1 个；数组方便脚本判空）。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Combat|Skill")
	TArray<FGameplayAbilitySpecHandle> FindSkillHandles(int32 SkillId) const;

	/** Spec → 技能 ID；不是按技能表授予的能力返回 0。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Skill")
	int32 GetSkillIdForHandle(FGameplayAbilitySpecHandle AbilityHandle) const;

	/** 技能的分类 Tag（技能表的 SkillTags）；未授予的 ID 返回空容器。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Skill")
	FGameplayTagContainer GetSkillTags(int32 SkillId) const;

	/** 技能的分类 Tag 是否满足查询：空查询恒为 true；ID 为 0 或未授予时（非空查询）为 false。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Skill")
	bool SkillMatchesTagQuery(int32 SkillId, const FGameplayTagQuery& Query) const;
	#pragma endregion

	#pragma region GA
	//判断这个 ASC 当前身上拥有的 GameplayTag，是否满足给定的 Tag 查询条件
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Input")
	bool MatchesOwnedTagQuery(const FGameplayTagQuery& Query) const;

	/** 输入解析所需的配置；只返回仍有效的已授予能力，不创建执行实例。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat")
	UWuwaGameplayAbilityBase* GetAbilityForInput(FGameplayAbilitySpecHandle AbilityHandle) const;
	
	/** 这个Spec能不能接收一次新的请求 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat")
	bool IsSpecAvailableForActivation(FGameplayAbilitySpecHandle AbilityHandle) const;
	UFUNCTION(BlueprintPure, Category = "Wuwa|Input")
	AWuwaCharacter* GetInputAvatar() const;
	UFUNCTION(BlueprintPure, Category = "Wuwa|Input")
	double GetInputTimeSeconds() const;

	/** 请求执行。主技能的 InstancedPerExecution 允许同一 Spec 在接招窗口重开，由 Skill 结束旧实例。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Combat")
	EWuwaAbilityRequestResult RequestAbilityActivation(FGameplayAbilitySpecHandle AbilityHandle);
	#pragma endregion

protected:
	virtual void OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec) override;

private:
	struct FGrantedSkill
	{
		FGameplayAbilitySpecHandle Handle;
		FGameplayTagContainer Tags;
	};
	/** 技能 ID → 授予记录（运行时数据，只在授予它的机器上有）。 */
	TMap<int32, FGrantedSkill> GrantedSkills;
};
