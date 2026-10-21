#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAirDodgeAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Game/Common/WuwaGameTags.h"
#include "Game/NewWorld/Buff/WuwaBuffLibrary.h"
#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "GameFramework/Controller.h"

void UWuwaAirDodgeAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 跳过 UWuwaSkillMontageAbility 的"直接播第 0 段"：原作先加 buff 再播
	UWuwaGameplayAbilityBase::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive()) return;
	if (StartBuffId != 0)
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo()) UWuwaBuffLibrary::AddBuff(ASC, StartBuffId);
	}
	if (!PlaySkillMontage(0))
	{
		EndSkill(true);
		return;
	}
	if (UAbilityTask_WaitDelay* Delay = UAbilityTask_WaitDelay::WaitDelay(this, MinAirTime))
	{
		Delay->OnFinish.AddDynamic(this, &UWuwaAirDodgeAbility::HandleMinAirTimeElapsed);
		Delay->ReadyForActivation();
	}
}

void UWuwaAirDodgeAbility::HandleMinAirTimeElapsed()
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const FWuwaGameTags& Tags = FWuwaGameTags::Get();
	if (!ASC || ASC->HasMatchingGameplayTag(Tags.State_Position_Ground))
	{
		EndSkill(false);
		return;
	}
	if (UAbilityTask_WaitGameplayTagAdded* Ground = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(this, Tags.State_Position_Ground, nullptr, true))
	{
		Ground->Added.AddDynamic(this, &UWuwaAirDodgeAbility::HandleGroundAdded);
		Ground->ReadyForActivation();
	}
	if (UAbilityTask_WaitGameplayTagAdded* Climb = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(this, Tags.State_Position_Climb, nullptr, true))
	{
		Climb->Added.AddDynamic(this, &UWuwaAirDodgeAbility::HandleClimbAdded);
		Climb->ReadyForActivation();
	}
}

void UWuwaAirDodgeAbility::HandleGroundAdded()
{
	// 原作：施法者是当前操作的角色 → 再等 0.1 秒结束；否则 0.1 秒淡出停掉蒙太奇（蒙太奇被打断 → 结束）
	const APawn* Pawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	if (Pawn && Pawn->IsPlayerControlled())
	{
		// 有移动输入：落地直接接冲刺（原作结束时 SprintPress），带着空中闪避的速度（上限冲刺速度）走，不在地上刹到 0 再起步。
		// 原作这 0.1 秒里蒙太奇还在放根运动；项目的 Jump_Second 落地时已经播完，等下去只剩刹车，起跑和冲刺动画就对不上速度了。
		const AWuwaCharacter* Character = Cast<AWuwaCharacter>(Pawn);
		if (Character && Character->GetPlayerInputState().bHasMoveInput)
		{
			if (UWuwaMovementComponent* Movement = Character->GetWuwaMovementComponent())
			{
				Movement->CarryLandingSpeed(Movement->GetSprintMaxSpeed());
				// 蒙太奇结束后的淡出期间别让它收尾的减速根运动把速度拖下来（多留 0.15 秒：淡出最后一帧权重≈0 的根运动也会把速度清零）
				const UAnimMontage* Montage = GetCurrentMontage();
				Movement->SuppressAnimRootMotion((Montage ? Montage->GetDefaultBlendOutTime() : 0.25f) + 0.15f);
			}
			// 步态组件发现 空中→地面 时会清空冲刺需求（位置变了就重置）。落地这一帧它可能还没刷新过：
			// 先让它记下"已在地面"，结束时的 RequestSprint（原作 SprintPress）才不会紧接着被清掉。
			if (UWuwaRoleGaitBridgeComponent* Gait = Character->RoleGaitComponent.Get()) Gait->RefreshPolicy();
			EndSkill(false);
			return;
		}
		if (UAbilityTask_WaitDelay* Delay = UAbilityTask_WaitDelay::WaitDelay(this, LandEndDelay))
		{
			Delay->OnFinish.AddDynamic(this, &UWuwaAirDodgeAbility::HandleLandEndDelay);
			Delay->ReadyForActivation();
			return;
		}
		EndSkill(false);
		return;
	}
	if (UAnimInstance* Anim = CurrentActorInfo ? CurrentActorInfo->GetAnimInstance() : nullptr) Anim->Montage_Stop(LandEndDelay, GetCurrentMontage());
}

bool UWuwaAirDodgeAbility::IsAirborne() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const FWuwaGameTags& Tags = FWuwaGameTags::Get();
	return ASC && !ASC->HasMatchingGameplayTag(Tags.State_Position_Ground) && !ASC->HasMatchingGameplayTag(Tags.State_Position_Climb);
}

void UWuwaAirDodgeAbility::OnSkillMontageCompleted(int32 Index)
{
	if (!IsAirborne()) Super::OnSkillMontageCompleted(Index);
}

void UWuwaAirDodgeAbility::OnSkillMontageBlendOut(int32 Index)
{
	if (!IsAirborne())
	{
		Super::OnSkillMontageBlendOut(Index);
		return;
	}
	// 还在空中：技能不结束，等落地（HandleGroundAdded 接冲刺 / 带速度）。淡出中的蒙太奇不再给根运动：
	// ABP 从整个姿势取根运动，它收尾的减速根运动会在空中把水平速度拖下来；关掉后按惯性 + 重力下落。
	UAnimInstance* Anim = CurrentActorInfo ? CurrentActorInfo->GetAnimInstance() : nullptr;
	if (FAnimMontageInstance* Instance = Anim && GetCurrentMontage() ? Anim->GetInstanceForMontage(GetCurrentMontage()) : nullptr)
	{
		Instance->PushDisableRootMotion();
	}
}

void UWuwaAirDodgeAbility::HandleClimbAdded() { EndSkill(false); }
void UWuwaAirDodgeAbility::HandleLandEndDelay() { EndSkill(false); }

void UWuwaAirDodgeAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 原作 OnEndAbility：有移动输入就 SprintPress
	if (IsActive())
	{
		if (const AWuwaCharacter* Character = Cast<AWuwaCharacter>(GetAvatarActorFromActorInfo());
			Character && Character->GetPlayerInputState().bHasMoveInput)
		{
			if (UWuwaRoleGaitBridgeComponent* Gait = Character->RoleGaitComponent.Get()) Gait->RequestSprint();
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
