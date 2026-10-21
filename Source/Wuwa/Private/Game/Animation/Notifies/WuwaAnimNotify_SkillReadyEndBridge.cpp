#include "Game/Animation/Notifies/WuwaAnimNotify_SkillReadyEndBridge.h"
#include "WuwaSkillNotifyContext.h"

void UWuwaAnimNotify_SkillReadyEndBridge::Notify(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	DispatchReadyEnd(MeshComp, Animation, EventReference.GetNotify(), WuwaSkillNotifyContext::GetMontageInstanceId(EventReference));
}

void UWuwaAnimNotify_SkillReadyEndBridge::BranchingPointNotify(FBranchingPointNotifyPayload& Payload)
{
	DispatchReadyEnd(Payload.SkelMeshComponent, Payload.SequenceAsset, Payload.NotifyEvent, Payload.MontageInstanceID);
}

void UWuwaAnimNotify_SkillReadyEndBridge::DispatchReadyEnd(USkeletalMeshComponent* MeshComp,UAnimSequenceBase* Animation, const FAnimNotifyEvent* NotifyEvent, int32 MontageInstanceId) const
{
	AWuwaCharacter* Character = WuwaSkillNotifyContext::ResolveCharacter(MeshComp);
	UAnimMontage* Montage = Cast<UAnimMontage>(Animation);
	int32 SkillHandle = 0;
	if (WuwaSkillNotifyContext::FindNotifyEventId(Montage, NotifyEvent, this) != INDEX_NONE
		&& WuwaSkillNotifyContext::ResolveActiveSkill(Character, MeshComp, Montage, MontageInstanceId, SkillHandle))
	{
		ReachSkillReadyEnd(Character->SkillComponent, SkillHandle);
	}
}

void UWuwaAnimNotify_SkillReadyEndBridge::ReachSkillReadyEnd_Implementation(
	UWuwaSkillBridgeComponent*, int32) const {}
