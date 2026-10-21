#pragma once

#include "CoreMinimal.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimSyncSnapshot.h"

/** Local ground-locomotion handoff. Captured before unpossess/cancel clears the outgoing role. */
struct FWuwaRoleSwitchSnapshot
{
	bool bGroundMovement = false;
	FVector2D MoveAxis = FVector2D::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FVector LastInput = FVector::ZeroVector;
	FVector Acceleration = FVector::ZeroVector;
	bool bWalkPreferred = false;
	EWuwaSprintDesire SprintDesire = EWuwaSprintDesire::None;
	float SprintRemainingSeconds = 0.f;
	EWuwaGait LastMovingGait = EWuwaGait::Run;
	FWuwaAnimSyncSnapshot Animation;
};
