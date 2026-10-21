#pragma once

#include "CoreMinimal.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "WuwaSkillMontageAbility.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;

/**
 * 按技能表播蒙太奇的通用技能（原作 SkillMode = 0：技能系统直接播 Animations[0]，没有专门的 GA）。
 * 和手做的 GA_AttackN 一样：播第 0 个蒙太奇，播完 / 淡出 / 被打断 / 被取消就结束技能。
 * 多段技能（原作 SkillMode = 1，有专门 GA 的）继承它，用 PlaySkillMontage(Index) 播后面几段并重写回调。
 * 每个技能一个蓝图子类（WuwaRoleSkillSetup 生成），CDO 上填蒙太奇和打断等级。
 */
UCLASS()
class WUWA_API UWuwaSkillMontageAbility : public UWuwaGameplayAbilityBase
{
	GENERATED_BODY()

public:
	/** 原作技能表 Animations（项目技能表 Montages），按原作顺序。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Skill")
	TArray<TObjectPtr<UAnimMontage>> SkillMontages;

	/** 原作「获取当前技能攻速」，项目暂时没有攻速属性，固定 1。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Skill", meta = (ClampMin = "0.01"))
	float PlayRate = 1.f;

	/** 正在播的是第几个蒙太奇（-1 = 没有）。 */
	int32 GetPlayingMontageIndex() const { return PlayingIndex; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** 播第 Index 个蒙太奇（原作 CreatePlayMontageAndWaitProxy：StopWhenAbilityEnds = 真）；没有这个蒙太奇返回空。 */
	UAbilityTask_PlayMontageAndWait* PlaySkillMontage(int32 Index, bool bAllowInterruptAfterBlendOut = false);

	/** 回调都带蒙太奇序号；默认：完成 / 淡出 / 打断 / 取消都结束技能。 */
	virtual void OnSkillMontageCompleted(int32 Index) { EndSkill(false); }
	virtual void OnSkillMontageBlendOut(int32 Index) { EndSkill(false); }
	virtual void OnSkillMontageInterrupted(int32 Index) { EndSkill(true); }
	virtual void OnSkillMontageCancelled(int32 Index) { EndSkill(true); }
	virtual void OnSkillMontageTick(int32 Index, float DeltaTime) {}

	void EndSkill(bool bWasCancelled);

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UFUNCTION() void HandleCompleted();
	UFUNCTION() void HandleBlendOut();
	UFUNCTION() void HandleInterrupted();
	UFUNCTION() void HandleCancelled();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
	int32 PlayingIndex = INDEX_NONE;
	/** 换下一段时旧任务的回调要忽略（它会在被新蒙太奇顶掉时报 Interrupted / BlendOut）。 */
	uint32 MontageSerial = 0;
	uint32 CallbackSerial = 0;
	FTSTicker::FDelegateHandle TickHandle;
};
