#include "Game/Animation/Notifies/WuwaAnimNotify_ChangeSkillPriority.h"
#include "WuwaSkillNotifyContext.h"

FString UWuwaAnimNotify_ChangeSkillPriority::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Skill Priority %d"), Priority);
}

void UWuwaAnimNotify_ChangeSkillPriority::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Apply(MeshComp, Animation, EventReference.GetNotify(), WuwaSkillNotifyContext::GetMontageInstanceId(EventReference));
}

void UWuwaAnimNotify_ChangeSkillPriority::BranchingPointNotify(FBranchingPointNotifyPayload& Payload)
{
	Apply(Payload.SkelMeshComponent, Payload.SequenceAsset, Payload.NotifyEvent, Payload.MontageInstanceID);
}

void UWuwaAnimNotify_ChangeSkillPriority::Apply(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEvent* NotifyEvent, int32 MontageInstanceId) const
{
	AWuwaCharacter* Character = WuwaSkillNotifyContext::ResolveCharacter(MeshComp);
	UAnimMontage* Montage = Cast<UAnimMontage>(Animation);
	int32 SkillHandle = 0;
	if (WuwaSkillNotifyContext::FindNotifyEventId(Montage, NotifyEvent, this) != INDEX_NONE
		&& WuwaSkillNotifyContext::ResolveActiveSkill(Character, MeshComp, Montage, MontageInstanceId, SkillHandle))
	{
		Character->SkillComponent->SetSkillInterruptLevel(SkillHandle, Priority);
	}
}
