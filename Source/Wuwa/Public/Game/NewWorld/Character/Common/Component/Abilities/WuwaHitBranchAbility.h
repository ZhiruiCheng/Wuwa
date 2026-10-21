#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaSkillMontageAbility.h"
#include "WuwaHitBranchAbility.generated.h"

/**
 * 命中变招（按原作 GA_Jump_MoveAttack_onlystandard 字节码移植；嘉贝 E 跳踢 1208007 / 变身后 1208027）：
 * - 播 [0]，同时等 BranchEventTag 事件（只等一次）。事件由子弹命中发（FWuwaBulletInfo::HitEventToAttacker，原作 触发变招）。
 * - 收到事件：立刻播 [1]（顶掉 [0]）。[0] 之后怎么结束都不管。
 * - 没收到事件：[0] 播完 / 淡出 / 打断 / 取消就结束。
 * - [1] 播完 / 淡出 / 打断 / 取消都结束。
 * 原作被打断时若在受击里只返回（受击流程结束技能），项目受击同样会结束技能，统一按"结束"处理。
 * 原作结束时还销毁"普通E-踢人后延迟生效爆炸"子弹（0.25 秒的无伤害子弹），项目不做。
 */
UCLASS()
class WUWA_API UWuwaHitBranchAbility : public UWuwaSkillMontageAbility
{
	GENERATED_BODY()

public:
	/** 收到这个 GameplayEvent 就接 [1]（原作 角色.2e985a12.ab2b6d02.2c4cc3bc → Role.Jiabei.Event.SkillBranch）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Skill")
	FGameplayTag BranchEventTag;

	/** 已经变招、在播 [1]（原作变量 触发变招）。 */
	bool HasBranched() const { return bBranched; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void OnSkillMontageCompleted(int32 Index) override;
	virtual void OnSkillMontageBlendOut(int32 Index) override;
	virtual void OnSkillMontageInterrupted(int32 Index) override;
	virtual void OnSkillMontageCancelled(int32 Index) override;

private:
	UFUNCTION()
	void OnBranchEvent(FGameplayEventData Payload);

	/** [0] 结束：变招了就不管，否则结束技能。 */
	void OnFirstMontageEnded(bool bWasCancelled);

	bool bBranched = false;
};
