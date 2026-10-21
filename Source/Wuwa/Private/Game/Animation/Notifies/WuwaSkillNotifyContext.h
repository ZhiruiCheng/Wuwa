#pragma once

#include "AbilitySystemComponent.h"
#include "Animation/ActiveMontageInstanceScope.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

namespace WuwaSkillNotifyContext
{
	inline int32 GetMontageInstanceId(const FAnimNotifyEventReference& EventReference)
	{
		const auto* Context = EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
		return Context ? Context->MontageInstanceID : INDEX_NONE;
	}

	inline AWuwaCharacter* ResolveCharacter(USkeletalMeshComponent* Mesh)
	{
		if (!IsValid(Mesh) || !Mesh->GetWorld() || !Mesh->GetWorld()->IsGameWorld()) return nullptr;
		AWuwaCharacter* Character = Cast<AWuwaCharacter>(Mesh->GetOwner());
		return IsValid(Character) && Character->GetMesh() == Mesh && IsValid(Character->SkillComponent)
			? Character : nullptr;
	}

	/** 角色的移动组件（技能位移 / 转向通知用）；不是游戏世界里的角色时为空。 */
	inline UWuwaMovementComponent* ResolveMovement(USkeletalMeshComponent* Mesh, AWuwaCharacter** OutCharacter = nullptr)
	{
		AWuwaCharacter* Character = ResolveCharacter(Mesh);
		if (OutCharacter) *OutCharacter = Character;
		return Character ? Cast<UWuwaMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	}

	inline bool BelongsToNotify(const FAnimNotifyEvent& Event, const UObject* Notify)
	{
		return Event.Notify.Get() == Notify || Event.NotifyStateClass.Get() == Notify;
	}

	inline int32 FindNotifyEventId(const UAnimMontage* Montage, const FAnimNotifyEvent* Event, const UObject* Notify)
	{
		if (!IsValid(Montage) || !Event || !BelongsToNotify(*Event, Notify)) return INDEX_NONE;
		for (int32 Index = 0; Index < Montage->Notifies.Num(); ++Index)
		{
			if (&Montage->Notifies[Index] == Event) return Index;
		}
		int32 Result = INDEX_NONE;
		for (int32 Index = 0; Index < Montage->Notifies.Num(); ++Index)
		{
			const FAnimNotifyEvent& Candidate = Montage->Notifies[Index];
			// Queued NotifyEnd uses an event copy in AnimInstance, not an address in
			// Montage.Notifies. Match its authored occurrence, rejecting ambiguous data.
			if (BelongsToNotify(Candidate, Notify) && Candidate.GetTime() == Event->GetTime()
				&& Candidate.GetDuration() == Event->GetDuration() && Candidate.TrackIndex == Event->TrackIndex)
			{
				if (Result != INDEX_NONE) return INDEX_NONE;
				Result = Index;
			}
		}
		return Result;
	}

	inline bool ResolveActiveSkill(AWuwaCharacter* Character, USkeletalMeshComponent* Mesh,
		UAnimMontage* Montage, int32 MontageInstanceId, int32& OutSkillHandle)
	{
		OutSkillHandle = 0;
		if (!Character || MontageInstanceId < 0 || !IsValid(Montage)) return false;
		const FWuwaSkillData Skill = Character->SkillComponent->GetCurrentSkillData();
		UWuwaGameplayAbilityBase* Ability = Skill.ActiveAbility.Get();
		if (Skill.FightStateHandle <= 0 || !IsValid(Ability) || !Ability->IsSkillExecutionFor(Character)
			|| Ability->GetSkillHandle() != Skill.FightStateHandle || Ability->GetCurrentMontage() != Montage) return false;
		UAbilitySystemComponent* ASC = Ability->GetAbilitySystemComponentFromActorInfo();
		UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
		if (!IsValid(ASC) || ASC->GetAvatarActor() != Character || ASC->GetAnimatingAbility() != Ability
			|| ASC->GetCurrentMontage() != Montage || !IsValid(AnimInstance)
			|| !ASC->AbilityActorInfo.IsValid() || ASC->AbilityActorInfo->GetAnimInstance() != AnimInstance) return false;
		const FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(Montage);
		if (!Instance || !Instance->IsActive() || Instance->GetInstanceID() != MontageInstanceId) return false;
		OutSkillHandle = Skill.FightStateHandle;
		return true;
	}
}
