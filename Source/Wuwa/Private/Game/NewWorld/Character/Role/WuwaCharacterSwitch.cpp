#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/Formation/WuwaRoleSwitchSnapshot.h"
#include "Components/SkeletalMeshComponent.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimInstance.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputIntentComponent.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"

FWuwaRoleSwitchSnapshot AWuwaCharacter::CaptureRoleSwitch()
{
	check(IsInGameThread());
	FWuwaRoleSwitchSnapshot Result;
	UWuwaMovementComponent* Movement = GetWuwaMovementComponent();
	UWuwaAnimInstance* Anim = GetMesh() ? Cast<UWuwaAnimInstance>(GetMesh()->GetAnimInstance()) : nullptr;
	if (!Movement || !Movement->IsMovingOnGround() || Movement->IsCrouching()
		|| !EnsureMovementStateSystem() || (Anim && Anim->IsAnyMontagePlaying())) return Result;
	Result.bGroundMovement = true;
	Result.MoveAxis = InputIntent ? InputIntent->GetMoveIntent().Axis : FVector2D::ZeroVector;
	Result.Velocity = Movement->Velocity;
	Result.LastInput = Movement->GetLastInputVector();
	Result.Acceleration = Movement->GetCurrentAcceleration();
	Result.bWalkPreferred = UnifiedStateComponent->IsWalkPreferred();
	Result.SprintDesire = RoleGaitComponent->ReadSprintDesire();
	Result.SprintRemainingSeconds = RoleGaitComponent->ReadSprintRemainingSeconds();
	Result.LastMovingGait = RoleGaitComponent->StopGait;
	if (Anim) Result.Animation = Anim->CaptureRoleAnimation();
	return Result;
}

void AWuwaCharacter::RestoreRoleSwitch(const FWuwaRoleSwitchSnapshot& Snapshot, bool bMotion)
{
	check(IsInGameThread());
	if (!Snapshot.bGroundMovement || !EnsureMovementStateSystem()) return;
	if (!bMotion)
	{
		if (InputIntent) InputIntent->SetMoveAxis(Snapshot.MoveAxis);
		UnifiedStateComponent->SetWalkPreference(Snapshot.bWalkPreferred);
		return;
	}
	UWuwaMovementComponent* Movement = GetWuwaMovementComponent();
	if (!Movement || !Movement->IsMovingOnGround()) return;
	if (InputIntent) InputIntent->SetMoveAxis(Snapshot.MoveAxis);
	Movement->RestoreSwitchMotion(Snapshot.Velocity, Snapshot.LastInput, Snapshot.Acceleration);
	UnifiedStateComponent->SetWalkPreference(Snapshot.bWalkPreferred);
	RoleGaitComponent->RestoreSwitchGait(Snapshot.SprintDesire, Snapshot.SprintRemainingSeconds, Snapshot.LastMovingGait);
	Movement->RefreshMovementSettings();
	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		if (UWuwaAnimInstance* Anim = Cast<UWuwaAnimInstance>(CharacterMesh->GetAnimInstance()))
		{
			// Queue after initialization: the first NativeUpdate may clear its owner cache.
			Anim->QueueRoleAnimation(Snapshot.Animation);
			CharacterMesh->TickAnimation(0.f, false);
			CharacterMesh->RefreshBoneTransforms();
		}
	}
}
