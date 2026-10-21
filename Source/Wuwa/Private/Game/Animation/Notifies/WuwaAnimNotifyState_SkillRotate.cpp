#include "Game/Animation/Notifies/WuwaAnimNotifyState_SkillRotate.h"

#include "Curves/CurveFloat.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "WuwaSkillNotifyContext.h"

void UWuwaAnimNotifyState_SkillRotate::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	AWuwaCharacter* Character = nullptr;
	UWuwaMovementComponent* Movement = WuwaSkillNotifyContext::ResolveMovement(MeshComp, &Character);
	if (!Movement) return;
	Movement->SkillRotateNotifyTime = 0.f;
	Movement->SkillRotateNotifyDuration = TotalDuration;
	Movement->SetSkillRotateTarget(Character->SkillComponent->GetSkillTarget(), Character->SkillComponent->GetSkillTargetSocket());
	Movement->SetSkillCanRotate(true);
	Movement->SetSkillRotateToTarget(bAutoFaceTarget, bApplyOffset, Offset, PauseThreshold, ResumeThreshold);
}

void UWuwaAnimNotifyState_SkillRotate::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	UWuwaMovementComponent* Movement = WuwaSkillNotifyContext::ResolveMovement(MeshComp);
	if (!Movement || FrameDeltaTime <= 0.f) return;
	const float Before = Movement->SkillRotateNotifyTime;
	Movement->SkillRotateNotifyTime += FrameDeltaTime;
	float Speed = RotateSpeed;
	if (bSmooth && Movement->SkillRotateNotifyDuration > 0.f)
	{
		// 原作：转速 = 剩余角度 × 这一帧的进度比例 / 帧时间，不超过 旋转速度
		float From = Before / Movement->SkillRotateNotifyDuration;
		float To = Movement->SkillRotateNotifyTime / Movement->SkillRotateNotifyDuration;
		if (Curve)
		{
			From = Curve->GetFloatValue(FMath::Clamp(From, 0.f, 1.f));
			To = Curve->GetFloatValue(FMath::Clamp(To, 0.f, 1.f));
		}
		const float Ratio = 1.f - From > KINDA_SMALL_NUMBER ? FMath::Clamp((To - From) / (1.f - From), 0.f, 1.f) : 1.f;
		const FVector Direction = Movement->GetSkillRotateDirection();
		const FVector Forward = Movement->UpdatedComponent ? Movement->UpdatedComponent->GetForwardVector() : FVector::ForwardVector;
		const float Angle = Direction.IsNearlyZero() ? 180.f
			: FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(FVector(Direction.X, Direction.Y, 0.0).GetSafeNormal(),
				FVector(Forward.X, Forward.Y, 0.0).GetSafeNormal()), -1.0, 1.0)));
		Speed = FMath::Clamp(Angle * Ratio / FrameDeltaTime, 0.f, RotateSpeed);
	}
	Movement->SetSkillRotateSpeed(Speed);
}

void UWuwaAnimNotifyState_SkillRotate::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (UWuwaMovementComponent* Movement = WuwaSkillNotifyContext::ResolveMovement(MeshComp))
	{
		Movement->SetSkillCanRotate(false);
		Movement->SetSkillRotateTarget(nullptr, NAME_None);
	}
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UWuwaAnimNotifyState_SkillRotate::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Skill Rotate %.0f%s"), RotateSpeed, bAutoFaceTarget ? TEXT(" (target)") : TEXT(" (input)"));
}
