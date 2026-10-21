#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateTypes.h"
#include "WuwaUnifiedStateBridgeComponent.generated.h"

// 与原作一样按维度分别广播：CharOnPositionStateChanged / CharOnUnifiedMoveStateChanged / CharOnDirectionStateChanged。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWuwaPositionStateChanged,
	EWuwaPositionState, OldState, EWuwaPositionState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWuwaMoveStateChanged,
	EWuwaMoveState, OldState, EWuwaMoveState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWuwaDirectionStateChanged,
	EWuwaDirectionState, OldState, EWuwaDirectionState, NewState);
// 本项目扩展：与 MoveState 一起提交的速度配置维度。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWuwaGaitChanged,
	EWuwaGait, OldGait, EWuwaGait, NewGait);
// 走跑偏好变化（原作 OnChangeWalkOrRun）。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWuwaWalkPreferenceChanged,
	bool, bWasWalk, bool, bIsWalk);

/**
 * 运动状态（原作 CharacterUnifiedStateComponent）的反射、快照和事件桥。
 * 三个维度的设置规则、合法组合表和走跑偏好由 C# 子类实现；这里没有占用或优先级。
 */
UCLASS(Abstract, Blueprintable, ClassGroup=(Wuwa), meta=(BlueprintSpawnableComponent))
class WUWA_API UWuwaUnifiedStateBridgeComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	/** 原作 InitCharState。初始位置和出生走跑偏好由组装者（角色）从 CMC 读出后传入，本组件不依赖 CMC。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|State") void InitializeState(EWuwaPositionState InitialPosition, EWuwaGait InitialWalkPreference);

	/** 原作 SetPositionState：位置变化时进入 Ground 执行 OnLand，然后广播位置变化。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|State") bool SetPositionState(EWuwaPositionState NewPosition);
	/** 原作 SetMoveState：当前位置下不合法的移动状态被拒绝。NewGait 是本项目的速度配置维度。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|State") bool SetMoveState(EWuwaMoveState NewState, EWuwaGait NewGait);
	/** 原作 SetDirectionState。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|State") bool SetDirectionState(EWuwaDirectionState NewDirection);
	/** 原作 legalMoveStates。 */
	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category="Wuwa|State") bool IsMoveStateLegal(EWuwaPositionState Position, EWuwaMoveState Move) const;
	/** 原作监听 CharMovementModeChanged：NewPosition 由 CMC 从物理模式读出，NewMode 决定附带写入的移动状态。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|State") void HandleMovementModeChanged(EWuwaPositionState NewPosition, EMovementMode NewMode);

	/** 玩家保存的走跑偏好是不是"走"（原作 IsWalkBaseMode）。 */
	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category="Wuwa|State|Walk") bool IsWalkPreferred() const;
	/** 直接设定走跑偏好（原作 MarkWalkOrRun）：只修改偏好并广播 OnWalkPreferenceChanged；移动状态由 RoleGait 按偏好决定。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|State|Walk") bool SetWalkPreference(bool bWalk);
	/** 在走/跑偏好之间切换（原作 WalkPress）。原作内部先问 CMC 的 CanWalkPress，这里由调用方 CMC 先判断 CanToggleWalkPreference。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|State|Walk") bool ToggleWalkPreference();

	UFUNCTION(BlueprintPure, Category="Wuwa|State") FWuwaUnifiedStateData GetStateData() const { return StateData; }
	/** 原作只允许主控端修改运动状态；本项目按非 SimulatedProxy 判断。 */
	UFUNCTION(BlueprintPure, Category="Wuwa|State") bool CanDriveState() const;

	/** C# 写快照的唯一出口；有变化时 Revision + 1。只写数据不广播，由调用方按原作顺序广播。 */
	UFUNCTION(BlueprintCallable, Category="Wuwa|State", meta=(BlueprintProtected)) void CommitStateData(FWuwaUnifiedStateData NewState);
	/** 已提交的位置 → 角色身上的 State.Position.* loose tag（原作 行为状态.位置状态.*）。 */
	void SyncPositionTags();
	UFUNCTION(BlueprintCallable, Category="Wuwa|State", meta=(BlueprintProtected)) void BroadcastPositionStateChanged(EWuwaPositionState OldState, EWuwaPositionState NewState);
	UFUNCTION(BlueprintCallable, Category="Wuwa|State", meta=(BlueprintProtected)) void BroadcastMoveStateChanged(EWuwaMoveState OldState, EWuwaMoveState NewState);
	UFUNCTION(BlueprintCallable, Category="Wuwa|State", meta=(BlueprintProtected)) void BroadcastGaitChanged(EWuwaGait OldGait, EWuwaGait NewGait);
	UFUNCTION(BlueprintCallable, Category="Wuwa|State", meta=(BlueprintProtected)) void BroadcastDirectionStateChanged(EWuwaDirectionState OldState, EWuwaDirectionState NewState);
	UFUNCTION(BlueprintCallable, Category="Wuwa|State", meta=(BlueprintProtected)) void BroadcastWalkPreferenceChanged(bool bWasWalk, bool bIsWalk);

	UPROPERTY(BlueprintAssignable, Category="Wuwa|State") FWuwaPositionStateChanged OnPositionStateChanged;
	UPROPERTY(BlueprintAssignable, Category="Wuwa|State") FWuwaMoveStateChanged OnMoveStateChanged;
	UPROPERTY(BlueprintAssignable, Category="Wuwa|State") FWuwaGaitChanged OnGaitChanged;
	UPROPERTY(BlueprintAssignable, Category="Wuwa|State") FWuwaDirectionStateChanged OnDirectionStateChanged;
	UPROPERTY(BlueprintAssignable, Category="Wuwa|State") FWuwaWalkPreferenceChanged OnWalkPreferenceChanged;
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Wuwa|State", meta=(AllowPrivateAccess="true"))
	FWuwaUnifiedStateData StateData;
private:
	bool bPositionTagsSynced = false;
};
