#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimTypes.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaLocomotionTypes.h"

/** Values only: never retain the outgoing instance or its animation node pointers. */
struct FWuwaAnimPlayerSnapshot
{
	FName NodeName;
	bool bBlendSpace = false;
	bool bLooping = true;
	FVector BlendPosition = FVector::ZeroVector;
	float NormalizedTime = 0.f;
	FMarkerSyncAnimPosition MarkerPosition;
};

/** Shared role graph's ground StartMove / Move handoff (walk/run/sprint).
 * Not a serialized engine state machine, montage, or network prediction snapshot.
 */
struct WUWA_API FWuwaAnimSyncSnapshot
{
	TWeakObjectPtr<UClass> GraphOwner;
	FName GroundState = TEXT("Move");
	FWuwaLocomotionAnimData Locomotion;
	TArray<FWuwaAnimPlayerSnapshot> Players;
	TMap<FName, double> BlendParameters;

	bool IsValid() const { return GraphOwner.IsValid() && !Players.IsEmpty(); }
};
