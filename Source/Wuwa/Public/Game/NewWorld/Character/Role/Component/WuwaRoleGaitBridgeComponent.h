#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateTypes.h"
#include "WuwaRoleGaitBridgeComponent.generated.h"

class UWuwaInputIntentComponent;
class UWuwaUnifiedStateBridgeComponent;
class UCharacterMovementComponent;

/**
 * 步态决策（原作 RoleGaitComponent）的生命周期与访问桥；输入规则、计时、禁止来源均由 C# 持有。
 * 不订阅运动状态事件，由 CMC 每次物理更新前调用 RefreshPolicy（原作 OnTick）。
 */
UCLASS(Abstract, Blueprintable, ClassGroup=(Wuwa), meta=(BlueprintSpawnableComponent))
class WUWA_API UWuwaRoleGaitBridgeComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|Gait") void InitializePolicy();
	/** 原作 $in()：按移动输入、位置和冲刺需求决定移动状态，写入运动状态组件。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|Gait") void RefreshPolicy();
	/** 长期冲刺请求（旧 SetDesiredGait(Sprint) 的语义）；不修改走跑偏好。 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|Gait|Sprint") bool RequestSprint();
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|Gait") void SetGaitBlocked(UObject* Source, EWuwaGait Gait, bool bBlocked);
	/** 原作 EnableRoleGaitState。 */
	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category="Wuwa|Gait") bool IsGaitAllowed(EWuwaGait Gait) const;
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|Gait|Sprint") void OpenSprintWindow(UObject* Source);
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|Gait|Sprint") void SampleSprintWindow(UObject* Source, float InputHeldSeconds, float HoldThresholdSeconds);
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|Gait|Sprint") void CloseSprintWindow(UObject* Source);
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|Gait|Sprint") void ResetSprintRequest();
	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category="Wuwa|Gait|Sprint") EWuwaSprintDesire ReadSprintDesire() const;
	/** Snapshot remaining temporary sprint time; an open window is treated as ending at the switch. */
	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category="Wuwa|Gait|Switch") float ReadSprintRemainingSeconds() const;
	/** Restore values only, preserving this role's own gait restrictions. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|Gait|Switch") void RestoreSwitchGait(EWuwaSprintDesire Desire, float RemainingSeconds, EWuwaGait LastMovingGait);
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Wuwa|Gait") void ResetRuntime();

	UFUNCTION(BlueprintPure, Category="Wuwa|Gait") FWuwaMovementStateContext ReadMovementContext() const;
	UFUNCTION(BlueprintCallable, Category="Wuwa|Gait", meta=(BlueprintProtected))
	void PublishGait(EWuwaSprintDesire Desire, EWuwaGait LastMovingGait);

	/**
	 * 由角色组装时注入，本组件不查找、也不读取角色。
	 * Movement 只用引擎基类读取物理事实（速度、蹲伏、移动模式），不依赖本项目的 CMC；传空表示不再读取。
	 */
	void BindDependencies(UWuwaUnifiedStateBridgeComponent* InUnifiedState, UCharacterMovementComponent* InMovement,
		UWuwaInputIntentComponent* InInputIntent);

	/** 窗口结束后暂时冲刺保留的游戏秒数，不包含窗口自身的时长。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Wuwa|Gait|Sprint", meta=(ClampMin="0", Units="s"))
	float TemporarySprintDuration = 1.f;

	//这两个给动画读取；走跑偏好在运动状态组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Wuwa|Gait") EWuwaSprintDesire SprintDesire = EWuwaSprintDesire::None;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Wuwa|Gait") EWuwaGait StopGait = EWuwaGait::Run;

private:
	/** 脚本读取它决定位置、写入移动状态。 */
	UPROPERTY(Transient, BlueprintReadOnly, Category="Wuwa|Gait", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UWuwaUnifiedStateBridgeComponent> UnifiedState;
	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> Movement;
	UPROPERTY(Transient)
	TObjectPtr<UWuwaInputIntentComponent> InputIntent;
};
