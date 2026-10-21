#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaSkillMontageAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Containers/Ticker.h"

void UWuwaSkillMontageAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	// 基类在主技能登记失败时会直接结束本次执行。
	if (!IsActive()) return;
	if (!PlaySkillMontage(0)) EndSkill(true);
}

UAbilityTask_PlayMontageAndWait* UWuwaSkillMontageAbility::PlaySkillMontage(int32 Index, bool bAllowInterruptAfterBlendOut)
{
	UAnimMontage* Montage = SkillMontages.IsValidIndex(Index) ? SkillMontages[Index].Get() : nullptr;
	if (!Montage || !IsActive()) return nullptr;
	// 先换序号再播：新蒙太奇顶掉旧的时，旧任务的 Interrupted / BlendOut 回调按旧序号被忽略。
	const uint32 Serial = ++MontageSerial;
	if (MontageTask) MontageTask->EndTask();
	UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, PlayRate, NAME_None, true, 1.f, 0.f, bAllowInterruptAfterBlendOut);
	if (!Task) return nullptr;
	Task->OnCompleted.AddDynamic(this, &UWuwaSkillMontageAbility::HandleCompleted);
	Task->OnBlendOut.AddDynamic(this, &UWuwaSkillMontageAbility::HandleBlendOut);
	Task->OnInterrupted.AddDynamic(this, &UWuwaSkillMontageAbility::HandleInterrupted);
	Task->OnCancelled.AddDynamic(this, &UWuwaSkillMontageAbility::HandleCancelled);
	MontageTask = Task;
	PlayingIndex = Index;
	CallbackSerial = Serial;
	Task->ReadyForActivation();
	// 原作每个播放任务都有 OnTick（落地检测等）；用核心 Ticker，技能结束时移除。
	if (!TickHandle.IsValid())
	{
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime)
		{
			if (IsActive() && PlayingIndex != INDEX_NONE) OnSkillMontageTick(PlayingIndex, DeltaTime);
			return true;
		}));
	}
	return Task;
}

void UWuwaSkillMontageAbility::HandleCompleted() { if (CallbackSerial == MontageSerial) OnSkillMontageCompleted(PlayingIndex); }
void UWuwaSkillMontageAbility::HandleBlendOut() { if (CallbackSerial == MontageSerial) OnSkillMontageBlendOut(PlayingIndex); }
void UWuwaSkillMontageAbility::HandleInterrupted() { if (CallbackSerial == MontageSerial) OnSkillMontageInterrupted(PlayingIndex); }
void UWuwaSkillMontageAbility::HandleCancelled() { if (CallbackSerial == MontageSerial) OnSkillMontageCancelled(PlayingIndex); }

void UWuwaSkillMontageAbility::EndSkill(bool bWasCancelled)
{
	if (IsActive()) EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, bWasCancelled);
}

void UWuwaSkillMontageAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	PlayingIndex = INDEX_NONE;
	++MontageSerial;
	MontageTask = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
