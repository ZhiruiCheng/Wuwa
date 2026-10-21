#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaHitBranchAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"

void UWuwaHitBranchAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bBranched = false;
	// 先挂事件再播 [0]（原作 PlayMontageAndWait 后 WaitGameplayEvent，同一帧）
	UWuwaGameplayAbilityBase::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive()) return;
	if (BranchEventTag.IsValid())
	{
		UAbilityTask_WaitGameplayEvent* Wait = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, BranchEventTag, nullptr, true, true);
		Wait->EventReceived.AddDynamic(this, &UWuwaHitBranchAbility::OnBranchEvent);
		Wait->ReadyForActivation();
	}
	if (!PlaySkillMontage(0)) EndSkill(true);
}

void UWuwaHitBranchAbility::OnBranchEvent(FGameplayEventData Payload)
{
	if (bBranched || !IsActive()) return;
	bBranched = true;
	if (!PlaySkillMontage(1)) EndSkill(false);
}

void UWuwaHitBranchAbility::OnFirstMontageEnded(bool bWasCancelled)
{
	if (!bBranched) EndSkill(bWasCancelled);
}

void UWuwaHitBranchAbility::OnSkillMontageCompleted(int32 Index)
{
	if (Index == 0) OnFirstMontageEnded(false);
	else EndSkill(false);
}

void UWuwaHitBranchAbility::OnSkillMontageBlendOut(int32 Index)
{
	if (Index == 0) OnFirstMontageEnded(false);
	else EndSkill(false);
}

void UWuwaHitBranchAbility::OnSkillMontageInterrupted(int32 Index)
{
	if (Index == 0) OnFirstMontageEnded(true);
	else EndSkill(true);
}

void UWuwaHitBranchAbility::OnSkillMontageCancelled(int32 Index)
{
	if (Index == 0) OnFirstMontageEnded(true);
	else EndSkill(true);
}
