#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimInstance.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"

void UWuwaAnimInstance::UpdateFootIK(float DeltaSeconds, ACharacter* Character, UWuwaMovementComponent* Movement)
{
	check(IsInGameThread());
	const bool bGround = Character && Movement && Movement->IsMovingOnGround();
	// Original native IKMode implementation is unavailable. Airborne/actions use this project's conservative policy.
	const bool bAllowed = bGround && FootIKSettings.bEnabled && !bForceDisableFootIK
		&& !LocomotionData.bHasActionOverride && !IsAnyMontagePlaying();
	FootIKFrame.bGrounded = bAllowed;
	FootIKFrame.bSprint = LocomotionData.bStateGroundSprint || (bRoleStopSprint && GetDebugStateName(TEXT("SM_Ground")) == TEXT("Stop"));
	FootIKFrame.bRun = LocomotionData.bStateGroundRun;
	if (!bGround)
	{
		FootIKFrame.Alpha = 0.f;
		FootIKPlatformAlpha = 1.f;
		FootIKSlopeDiminish = 0.f;
		bFootIKPlatformMoving = false;
		return;
	}
	FootIKFrame.GroundZ = Character->GetActorLocation().Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (Movement->CurrentFloor.IsWalkableFloor())
		FootIKFrame.GroundZ -= Movement->CurrentFloor.GetDistanceToFloor();
	const auto* Base = Character->GetMovementBase();
	const FVector BaseVelocity = MovementBaseUtility::GetMovementBaseVelocity(Base, Character->GetBasedMovement().BoneName)
		+ MovementBaseUtility::GetMovementBaseTangentialVelocity(Base, Character->GetBasedMovement().BoneName, Character->GetActorLocation());
	// Exported BP: moving-base hysteresis 25/5 cm/s, attenuation interpolation speed 3.
	bFootIKPlatformMoving = BaseVelocity.Size() > (bFootIKPlatformMoving ? 5.f : 25.f);
	FootIKPlatformAlpha = FMath::FInterpTo(FootIKPlatformAlpha, bFootIKPlatformMoving ? 0.f : 1.f, DeltaSeconds, 3.f);
	float Diminish = 0.f;
	if (Movement->CurrentFloor.IsWalkableFloor() && LocomotionData.bHasMovingSpeed)
	{
		const FVector Normal = Movement->CurrentFloor.HitResult.ImpactNormal;
		const FVector Direction = Movement->Velocity.GetSafeNormal2D();
		// Signed travel slope: rising along travel is positive. The original export's sign convention is unknown.
		const float Slope = FMath::RadiansToDegrees(FMath::Atan2(-FVector::DotProduct(Normal, Direction), Normal.Z));
		Diminish = FMath::GetMappedRangeValueClamped(FVector2D(40.f, 50.f), FVector2D(0.f, 1.f), FMath::Abs(Slope));
	}
	FootIKSlopeDiminish = FMath::FInterpTo(FootIKSlopeDiminish, Diminish, DeltaSeconds, 2.f);
	const float Target = bAllowed ? (1.f - FootIKSlopeDiminish) * FootIKPlatformAlpha : 0.f;
	FootIKFrame.Alpha = FMath::FInterpTo(FootIKFrame.Alpha, Target, DeltaSeconds, 12.f);
}
