#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateBridgeComponent.h"
#include "Game/Common/WuwaGameTags.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"

// C++ 基类不复制脚本规则。未装配托管子类时请求失败，而不是偷偷运行另一套状态机。
//初始化
void UWuwaUnifiedStateBridgeComponent::InitializeState_Implementation(EWuwaPositionState, EWuwaGait) {}
//三个维度的设置（原作 SetPositionState / SetMoveState / SetDirectionState）
bool UWuwaUnifiedStateBridgeComponent::SetPositionState_Implementation(EWuwaPositionState) { return false; }//切换位置状态，比如地面、空中、攀爬、水中
bool UWuwaUnifiedStateBridgeComponent::SetMoveState_Implementation(EWuwaMoveState, EWuwaGait) { return false; }//切换移动状态和步态，比如"跑步，Run 配置"
bool UWuwaUnifiedStateBridgeComponent::SetDirectionState_Implementation(EWuwaDirectionState) { return false; }//切换朝向模式，比如跟随速度方向或者锁定目标
//合法性检查（const，只查询不修改）
bool UWuwaUnifiedStateBridgeComponent::IsMoveStateLegal_Implementation(EWuwaPositionState, EWuwaMoveState) const { return false; }//某个位置状态下，是否允许某个移动状态。比如在空中时不允许 Sprint。
//物理移动模式变化（原作 CharMovementModeChanged 的监听）
void UWuwaUnifiedStateBridgeComponent::HandleMovementModeChanged_Implementation(EWuwaPositionState, EMovementMode) {}
//走跑偏好
bool UWuwaUnifiedStateBridgeComponent::IsWalkPreferred_Implementation() const { return false; }
bool UWuwaUnifiedStateBridgeComponent::SetWalkPreference_Implementation(bool) { return false; }
bool UWuwaUnifiedStateBridgeComponent::ToggleWalkPreference_Implementation() { return false; }

bool UWuwaUnifiedStateBridgeComponent::CanDriveState() const
{
	const AActor* OwningActor = GetOwner();
	return IsValid(OwningActor) && OwningActor->GetLocalRole() != ROLE_SimulatedProxy;
}

void UWuwaUnifiedStateBridgeComponent::CommitStateData(FWuwaUnifiedStateData NewState)
{
	check(IsInGameThread());
	//如果和之前的状态一样就不写入，也不增加版本
	if (StateData.PositionState == NewState.PositionState && StateData.MoveState == NewState.MoveState
		&& StateData.DirectionState == NewState.DirectionState && StateData.Gait == NewState.Gait
		&& StateData.bHasActionOverride == NewState.bHasActionOverride)
	{
		if (!bPositionTagsSynced) SyncPositionTags();
		return;
	}
	NewState.Revision = StateData.Revision + 1;
	const bool bPositionChanged = StateData.PositionState != NewState.PositionState || !bPositionTagsSynced;
	StateData = NewState;
	if (bPositionChanged) SyncPositionTags();
}

void UWuwaUnifiedStateBridgeComponent::SyncPositionTags()
{
	// 原作 行为状态.位置状态.* 是角色身上的 Tag（GE 的移除条件、输入蓝图都在查它）；这里按已提交的位置维护一个 loose tag。
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	if (!ASC) return;
	const FWuwaGameTags& Tags = FWuwaGameTags::Get();
	const TPair<EWuwaPositionState, FGameplayTag> Map[] = { {EWuwaPositionState::Ground, Tags.State_Position_Ground}, {EWuwaPositionState::Air, Tags.State_Position_Air},
		{EWuwaPositionState::Climb, Tags.State_Position_Climb}, {EWuwaPositionState::Water, Tags.State_Position_Water} };
	for (const auto& [Position, Tag] : Map)
		ASC->SetLooseGameplayTagCount(Tag, StateData.PositionState == Position ? 1 : 0);
	bPositionTagsSynced = true;
}

void UWuwaUnifiedStateBridgeComponent::BroadcastPositionStateChanged(EWuwaPositionState OldState, EWuwaPositionState NewState)
{
	OnPositionStateChanged.Broadcast(OldState, NewState);
}

void UWuwaUnifiedStateBridgeComponent::BroadcastMoveStateChanged(EWuwaMoveState OldState, EWuwaMoveState NewState)
{
	OnMoveStateChanged.Broadcast(OldState, NewState);
}

void UWuwaUnifiedStateBridgeComponent::BroadcastGaitChanged(EWuwaGait OldGait, EWuwaGait NewGait)
{
	OnGaitChanged.Broadcast(OldGait, NewGait);
}

void UWuwaUnifiedStateBridgeComponent::BroadcastDirectionStateChanged(EWuwaDirectionState OldState, EWuwaDirectionState NewState)
{
	OnDirectionStateChanged.Broadcast(OldState, NewState);
}

void UWuwaUnifiedStateBridgeComponent::BroadcastWalkPreferenceChanged(bool bWasWalk, bool bIsWalk)
{
	OnWalkPreferenceChanged.Broadcast(bWasWalk, bIsWalk);
}
