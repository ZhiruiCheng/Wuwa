#include "Game/Animation/Notifies/WuwaAnimNotifyState_AbsoluteTimeStop.h"

#include "Components/SkeletalMeshComponent.h"
#include "Game/NewWorld/Combat/WuwaTimeStopSubsystem.h"

void UWuwaAnimNotifyState_AbsoluteTimeStop::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (UWuwaTimeStopSubsystem* TimeStop = UWuwaTimeStopSubsystem::Get(Owner)) TimeStop->BeginAbsoluteTimeStop(Owner, bStopMove);
}

void UWuwaAnimNotifyState_AbsoluteTimeStop::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (UWuwaTimeStopSubsystem* TimeStop = UWuwaTimeStopSubsystem::Get(Owner)) TimeStop->EndAbsoluteTimeStop(Owner);
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}
