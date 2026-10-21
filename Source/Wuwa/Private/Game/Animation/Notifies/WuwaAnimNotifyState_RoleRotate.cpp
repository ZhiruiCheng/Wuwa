#include "Game/Animation/Notifies/WuwaAnimNotifyState_RoleRotate.h"

#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "WuwaSkillNotifyContext.h"

void UWuwaAnimNotifyState_RoleRotate::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (UWuwaMovementComponent* Movement = WuwaSkillNotifyContext::ResolveMovement(MeshComp)) Movement->SetSkillRotateToTarget(bAutoFaceTarget);
}

void UWuwaAnimNotifyState_RoleRotate::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	AWuwaCharacter* Character = nullptr;
	UWuwaMovementComponent* Movement = WuwaSkillNotifyContext::ResolveMovement(MeshComp, &Character);
	if (!Movement) return;
	if (bAutoFaceTarget)
	{
		AActor* Target = Character->SkillComponent->GetSkillTarget();
		Movement->SetSkillRotateTarget(Target, Character->SkillComponent->GetSkillTargetSocket());
		Movement->SetSkillRotateToTarget(IsValid(Target));
		Movement->SetSkillRotateSpeed(RotateSpeed);
		return;
	}
	// 直接转向输入方向：偏离的角度越大转得越快（角度 / 180 × 转速）
	FVector Input;
	if (!Movement->GetMoveInputDirection(Input)) return;
	const FVector Forward = Character->GetActorForwardVector();
	const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(FVector(Forward.X, Forward.Y, 0.0).GetSafeNormal(), Input), -1.0, 1.0)));
	FRotator Rotation = Character->GetActorRotation();
	Rotation.Yaw = WuwaSkillMotion::InterpYawConstantTo(Rotation.Yaw, Input.Rotation().Yaw, FrameDeltaTime, Angle / 180.f * RotateSpeed);
	Character->SetActorRotation(Rotation);
}

void UWuwaAnimNotifyState_RoleRotate::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (UWuwaMovementComponent* Movement = WuwaSkillNotifyContext::ResolveMovement(MeshComp)) Movement->SetSkillCanRotate(false);
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UWuwaAnimNotifyState_RoleRotate::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Role Rotate %.0f%s"), RotateSpeed, bAutoFaceTarget ? TEXT(" (target)") : TEXT(" (input)"));
}
