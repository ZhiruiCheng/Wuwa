#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimDataLibrary.h"
#include "Animation/AnimSequence.h"

#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimLogicParams.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "GameFramework/Character.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateBridgeComponent.h"
#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"

bool UWuwaAnimDataLibrary::UpdateAnimationData(const UWuwaMovementComponent* Movement, UWuwaAnimLogicParams* Params)
{
	check(IsInGameThread());
	if (!IsValid(Params))
	{
		return false;
	}
	Params->Reset();
	if (!IsValid(Movement) || !IsValid(Movement->GetCharacterOwner())
		|| !IsValid(Movement->UpdatedComponent))
	{
		return false;
	}

	UpdateAnimInfoMove(*Movement, *Params);
	UpdateAnimInfoUnifiedState(*Movement, *Params);
	Params->bHasValidData = true;
	return true;
}

void UWuwaAnimDataLibrary::UpdateAnimInfoMove(const UWuwaMovementComponent& Movement, UWuwaAnimLogicParams& Params)
{
	FWuwaAnimMoveData& Data = Params.MoveData;
	Data.ActorRotation = Movement.GetCharacterOwner()->GetActorQuat();
	Data.Velocity = Movement.Velocity;
	Data.Acceleration = Movement.GetCurrentAcceleration();
	// 与 Acceleration 分开：有输入但未产生速度/加速度仍然能被动画观察到。
	Data.InputVector = Movement.GetLastInputVector().GetClampedToMaxSize(1.0);
	Data.MaxSpeed = Movement.GetMaxSpeed();
}

void UWuwaAnimDataLibrary::UpdateAnimInfoUnifiedState(const UWuwaMovementComponent& Movement, UWuwaAnimLogicParams& Params)
{
	FWuwaAnimStateData& Data = Params.StateData;
	Data.MovementMode = Movement.MovementMode;
	Data.CustomMovementMode = Movement.CustomMovementMode;
	Data.DesiredGait = Movement.GetDesiredGait();
	Data.AllowedGait = Movement.GetAllowedGait();
	// 冲刺需求和停步步态属于 RoleGait，直接从角色身上的 RoleGait 读，不经 CMC 转发。
	const AWuwaCharacter* Character = Cast<AWuwaCharacter>(Movement.GetCharacterOwner());
	const UWuwaRoleGaitBridgeComponent* Gait = Character ? Character->RoleGaitComponent.Get() : nullptr;
	Data.SprintDesire = IsValid(Gait) ? Gait->ReadSprintDesire() : EWuwaSprintDesire::None;
	Data.bStateGround = Movement.IsMovingOnGround();
	Data.bStateAir = Movement.IsFalling();
	Data.bStateClimb = Movement.IsClimbing();
	Data.bIsCrouching = Movement.IsCrouching();
	Data.bHasUnifiedState = Character && IsValid(Character->UnifiedStateComponent);
	if (Data.bHasUnifiedState)
	{
		const FWuwaUnifiedStateData State = Movement.GetUnifiedStateData();
		Data.PositionState = State.PositionState;
		Data.MoveState = State.MoveState;
		Data.DirectionState = State.DirectionState;
		Data.bHasActionOverride = State.bHasActionOverride;
		Data.StopGait = IsValid(Gait) ? Gait->StopGait : EWuwaGait::Run;
		Data.bStateGround = State.PositionState == EWuwaPositionState::Ground;
		Data.bStateAir = State.PositionState == EWuwaPositionState::Air;
		Data.bStateClimb = State.PositionState == EWuwaPositionState::Climb;
	}
}

//蓝图函数
float UWuwaAnimDataLibrary::GetStartTimeFromSyncPosition(const UAnimSequence* Sequence,
                                                     const FMarkerSyncAnimPosition& SyncPosition)
{
    if (!Sequence || SyncPosition.PreviousMarkerName.IsNone() || SyncPosition.NextMarkerName.IsNone())
    {
        return 0.f; // 没拿到有效同步位置时从头播
    }
    return Sequence->GetFirstMatchingPosFromMarkerSyncPos(SyncPosition);
}

float UWuwaAnimDataLibrary::GetStopEntryTime(const UAnimSequence* Sequence,
	const FMarkerSyncAnimPosition& SyncPosition, bool& bMatched)
{
	bMatched = false;
	if (!Sequence || !FMath::IsFinite(SyncPosition.PositionBetweenMarkers)) return 0.f;
	const FName Left(TEXT("Left")), Right(TEXT("Right"));
	if (!((SyncPosition.PreviousMarkerName == Left && SyncPosition.NextMarkerName == Right)
		|| (SyncPosition.PreviousMarkerName == Right && SyncPosition.NextMarkerName == Left))) return 0.f;
	const FAnimSyncMarker* First = nullptr;
	for (const FAnimSyncMarker& Marker : Sequence->AuthoredSyncMarkers)
	{
		if ((Marker.MarkerName != Left && Marker.MarkerName != Right) || !FMath::IsFinite(Marker.Time)
			|| Marker.Time < 0.f || Marker.Time >= Sequence->GetPlayLength()) continue;
		if (!First) { First = &Marker; continue; }
		// 首对不同脚的接触标记定义入场区间；不向后寻找其他匹配区间，也不跨尾帧循环。
		if (Marker.Time <= First->Time) return 0.f;
		if (Marker.MarkerName == First->MarkerName) continue;
		if (First->MarkerName != SyncPosition.PreviousMarkerName || Marker.MarkerName != SyncPosition.NextMarkerName) return 0.f;
		bMatched = true;
		return FMath::Clamp(FMath::Lerp(First->Time, Marker.Time,
			FMath::Clamp(SyncPosition.PositionBetweenMarkers, 0.f, 1.f)), 0.f, FMath::Max(0.f, Sequence->GetPlayLength() - 0.05f));
	}
	return 0.f;
}
