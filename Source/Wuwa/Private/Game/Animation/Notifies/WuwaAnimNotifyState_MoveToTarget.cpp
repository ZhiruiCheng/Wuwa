#include "Game/Animation/Notifies/WuwaAnimNotifyState_MoveToTarget.h"

#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "WuwaSkillNotifyContext.h"

void UWuwaAnimNotifyState_MoveToTarget::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	AWuwaCharacter* Character = WuwaSkillNotifyContext::ResolveCharacter(MeshComp);
	UWuwaMovementComponent* Movement = Character ? Cast<UWuwaMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	AActor* Target = Character ? Character->SkillComponent->GetSkillTarget() : nullptr;
	// 原作：开始时没有技能目标，整段不生效
	if (!Movement || !IsValid(Target)) return;
	Movement->BeginMoveToTarget(this, Config, Target, Character->SkillComponent->GetSkillTargetSocket(), TotalDuration);
}

void UWuwaAnimNotifyState_MoveToTarget::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (AWuwaCharacter* Character = WuwaSkillNotifyContext::ResolveCharacter(MeshComp))
	{
		if (UWuwaMovementComponent* Movement = Cast<UWuwaMovementComponent>(Character->GetCharacterMovement())) Movement->EndMoveToTarget(this);
	}
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UWuwaAnimNotifyState_MoveToTarget::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Move To Target %.0f%s"), Config.Distance, Config.MaxSpeed > 0.f ? *FString::Printf(TEXT(" @%.0f"), Config.MaxSpeed) : TEXT(" (hold)"));
}
