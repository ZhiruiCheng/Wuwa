#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputIntentComponent.h"
#include "Engine/World.h"

//策略生命周期
void UWuwaRoleGaitBridgeComponent::InitializePolicy_Implementation() {}
void UWuwaRoleGaitBridgeComponent::RefreshPolicy_Implementation() {}
void UWuwaRoleGaitBridgeComponent::ResetRuntime_Implementation() {}
//冲刺请求
bool UWuwaRoleGaitBridgeComponent::RequestSprint_Implementation() { return false; }
//步态封锁
void UWuwaRoleGaitBridgeComponent::SetGaitBlocked_Implementation(UObject*, EWuwaGait, bool) {}
bool UWuwaRoleGaitBridgeComponent::IsGaitAllowed_Implementation(EWuwaGait) const { return false; }

//冲刺窗口
void UWuwaRoleGaitBridgeComponent::OpenSprintWindow_Implementation(UObject*) {}
void UWuwaRoleGaitBridgeComponent::SampleSprintWindow_Implementation(UObject*, float, float) {}
void UWuwaRoleGaitBridgeComponent::CloseSprintWindow_Implementation(UObject*) {}
void UWuwaRoleGaitBridgeComponent::ResetSprintRequest_Implementation() {}
EWuwaSprintDesire UWuwaRoleGaitBridgeComponent::ReadSprintDesire_Implementation() const { return EWuwaSprintDesire::None; }
float UWuwaRoleGaitBridgeComponent::ReadSprintRemainingSeconds_Implementation() const { return 0.f; }
void UWuwaRoleGaitBridgeComponent::RestoreSwitchGait_Implementation(EWuwaSprintDesire, float, EWuwaGait) {}

void UWuwaRoleGaitBridgeComponent::BindDependencies(UWuwaUnifiedStateBridgeComponent* InUnifiedState,
	UCharacterMovementComponent* InMovement, UWuwaInputIntentComponent* InInputIntent)
{
	UnifiedState = InUnifiedState;
	Movement = InMovement;
	InputIntent = InInputIntent;
}

FWuwaMovementStateContext UWuwaRoleGaitBridgeComponent::ReadMovementContext() const
{
	check(IsInGameThread());
	FWuwaMovementStateContext Context;
	// 没有注入物理事实（尚未装配或角色正在结束）时返回空上下文，脚本不会据此驱动状态。
	if (!IsValid(Movement) || !GetWorld() || !GetWorld()->IsGameWorld()) return Context;
	// 与原作 RoleGait 一样读运动状态组件的位置；位置由它随移动模式事件同步，这里不再从 CMC 推导。
	if (IsValid(UnifiedState))
	{
		Context.PositionState = UnifiedState->GetStateData().PositionState;
	}
	Context.bHasMoveInput = IsValid(InputIntent) && InputIntent->GetMoveIntent().bHasInput;
	Context.bIsCrouching = Movement->IsCrouching();
	Context.bCanDriveState = GetOwnerRole() != ROLE_SimulatedProxy;
	Context.bIsFlying = Movement->MovementMode == MOVE_Flying;
	Context.GroundSpeed = Movement->Velocity.Size2D();
	Context.VerticalSpeed = Movement->Velocity.Z;
	Context.GameTimeSeconds = GetWorld()->GetTimeSeconds();
	Context.TemporarySprintDuration = TemporarySprintDuration;
	return Context;
}

void UWuwaRoleGaitBridgeComponent::PublishGait(EWuwaSprintDesire Desire, EWuwaGait LastMovingGait)
{
	check(IsInGameThread());
	SprintDesire = Desire;
	StopGait = LastMovingGait;
}
