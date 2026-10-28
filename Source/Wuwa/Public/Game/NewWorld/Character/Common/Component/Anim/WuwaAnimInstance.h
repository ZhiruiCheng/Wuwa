#pragma once

#include "CoreMinimal.h"
#include "Game/Animation/IK/WuwaFootIK.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimExecutionContext.h"
#include "Animation/AnimNodeReference.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaLocomotionTypes.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimSyncSnapshot.h"
#include "WuwaAnimInstance.generated.h"

class ACharacter;
class UWuwaMovementComponent;
class UWuwaAnimLogicParams;
class UBlendSpace;
class UAnimSequence;
struct FAnimNode_SequencePlayer;

/** 跳跃 / 落地在动画蓝图里的状态机和状态名（模板 ABP_RoleBase，由 UWuwaRoleJumpEditorLibrary 生成）。 */
namespace WuwaJumpLand
{
	inline const FName Locomotion(TEXT("Locmotion"));
	inline const FName Ground(TEXT("Ground"));
	inline const FName JumpStart(TEXT("JumpStart"));
	inline const FName Fall(TEXT("Fall"));
	inline const FName Land(TEXT("Land"));
	inline const FName JumpMachine(TEXT("SM_Jump"));
	inline const FName JumpTakeoff(TEXT("JumpTakeoff"));
	inline const FName JumpLoop(TEXT("JumpLoop"));
	inline const FName JumpFail(TEXT("JumpFail"));
}

#pragma region Debug

/** 单个状态在所属状态机内部的权重，不是最终全身姿势权重。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaDebugAnimStateWeight
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Wuwa|Animation|Debug")
	FName StateName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Wuwa|Animation|Debug")
	float Weight = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Wuwa|Animation|Debug")
	bool bIsCurrentState = false;
};

/** 最近一次状态机更新结果；在 Blueprint Update Animation 中读取可能滞后一帧。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaDebugAnimStateMachine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Wuwa|Animation|Debug")
	FName MachineName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Wuwa|Animation|Debug")
	bool bFound = false;

	UPROPERTY(BlueprintReadOnly, Category = "Wuwa|Animation|Debug")
	bool bInitialized = false;

	/** 状态机本身的记录权重。为零时，内部状态可能只是上次激活时留下的结果。 */
	UPROPERTY(BlueprintReadOnly, Category = "Wuwa|Animation|Debug")
	float MachineWeight = 0.f;

	/** 逻辑上的当前状态，不一定是权重最大的状态。 */
	UPROPERTY(BlueprintReadOnly, Category = "Wuwa|Animation|Debug")
	FName CurrentStateName = NAME_None;

	/** 所有权重大于零的非 Conduit 状态，按权重降序排列。 */
	UPROPERTY(BlueprintReadOnly, Category = "Wuwa|Animation|Debug")
	TArray<FWuwaDebugAnimStateWeight> ActiveStates;
};

#pragma endregion Debug

/** Common 动画父类：采集角色数据，AnimBP 再组织走、跑、起停和动画层。 */
UCLASS(Transient, Blueprintable)
class WUWA_API UWuwaAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wuwa|Animation|Foot IK")
	FWuwaFootIKSettings FootIKSettings;
	/** Explicit action/notify override; locomotion and Stop do not toggle this flag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wuwa|Animation|Foot IK")
	bool bForceDisableFootIK = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Wuwa|Animation|Foot IK")
	FWuwaFootIKFrame FootIKFrame;
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override; 
	virtual void NativeUninitializeAnimation() override;

	/** Game thread; waits for this mesh's animation work before reading the completed frame. */
	FWuwaAnimSyncSnapshot CaptureRoleAnimation();
	/** Queue a one-shot restore for the destination graph's next update. False means normal entry. */
	bool QueueRoleAnimation(const FWuwaAnimSyncSnapshot& Snapshot);
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Animation|Stop")
	TObjectPtr<UAnimSequence> RoleStopWalkLeft;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Animation|Stop")
	TObjectPtr<UAnimSequence> RoleStopWalkRight;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Animation|Stop")
	TObjectPtr<UAnimSequence> RoleStopRunLeft;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Animation|Stop")
	TObjectPtr<UAnimSequence> RoleStopRunRight;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Animation|Stop")
	TObjectPtr<UAnimSequence> RoleStopSprintLeft;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Animation|Stop")
	TObjectPtr<UAnimSequence> RoleStopSprintRight;
	/** 冲刺停步首帧的双脚关系：右脚减左脚，投影到角色前向/上向，单位 cm。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Animation|Stop")
	FVector2D RoleSprintLeftEntryFeet = FVector2D::ZeroVector;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Animation|Stop")
	FVector2D RoleSprintRightEntryFeet = FVector2D::ZeroVector;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Animation|Stop")
	bool bRoleSprintEntryFeetConfigured = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Stop")
	bool bRoleStopLeft = true;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Stop")
	bool bRoleStopWalk = false;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Stop")
	bool bRoleStopSprint = false;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Stop")
	float RoleStopStartTime = 0.f;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Stop")
	bool bRoleStopEntryMapped = false;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Stop")
	FMarkerSyncAnimPosition RoleStopEntrySyncPosition;
	/** 每次进入 Stop 递增；短暂离开后快速再入也会重新应用起播时间。 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Stop")
	int32 RoleStopEntrySerial = 0;

	/** 状态入场时锁定步态、选脚和起播时间，随后独立播放。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Animation|Stop", meta = (BlueprintThreadSafe))
	void EnterRoleStop(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);
	/** 每帧检查入场序号，只在新入场时写一次时间，不依赖播放器是否重新变为相关。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Animation|Stop", meta = (BlueprintThreadSafe))
	void UpdateRoleStopPlayer(const FAnimUpdateContext& Context, const FAnimNodeReference& Node);

#pragma region JumpLand
	// 属性名不要和状态名重名（状态图名会和蓝图成员冲突）。
	/** 原作 BS_Jump_LF / BS_Jump_RF：一维，0 = Jump_Walk，1 = Jump_Run；起跳的位移（含升起和落回）都在动画根运动里。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	TObjectPtr<UBlendSpace> JumpLeftBlendSpace;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	TObjectPtr<UBlendSpace> JumpRightBlendSpace;
	/** 原作 跳跃状态机 跳跃空中循环（Jump_Loop，播完一遍转 Fail）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	TObjectPtr<UAnimSequence> JumpLoopSequence;
	/** 原作 跳跃状态机 Fail（Fall_Loop_Fast）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	TObjectPtr<UAnimSequence> JumpFailSequence;
	/** 原作 下落（Fall_Loop）：没有起跳直接离地（走下台阶、平台边缘）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	TObjectPtr<UAnimSequence> FallSequence;
	/** 原作 落地状态机 轻落地 / 重落地。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	TObjectPtr<UAnimSequence> LandLightSequence;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	TObjectPtr<UAnimSequence> LandHeavySequence;
	/** 原作 落地速度切割初始化（女性 XL 体型 1600 / 2000）：落地竖直速度 ≥ X 为中速档、≥ Y 为高速档，单位 cm/s。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Animation|Jump", meta = (Units = "cm/s"))
	FVector2D LandSpeedThresholds = FVector2D(1600.f, 2000.f);

	// 以下由 NativeUpdateAnimation 写入、状态机过渡读取；条件逐条对应原作 ABP_BaseRole 的过渡（见 UpdateJumpLand）。
	/** 原作 LogicParams.IsJump：起跳请求，状态机进入 JumpStart 后清掉。 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	bool bJumpRequested = false;
	/** 原作 InterfaceJumpPressed：起跳时的水平速度 / 900，作为 BS_Jump 的横轴。 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	float JumpBlend = 0.f;
	/** 原作 状态选择：Feet_Position > 0（右脚在前）用右脚起跳。 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	bool bJumpRightFoot = false;
	/** 原作 起跳 → 落地中转：已进入空中循环 / Fail，或起跳剩余比例 < 0.5（过了最高点）。 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	bool bJumpCanLand = false;
	/** 不是起跳的离地（地面 → 下落）。 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	bool bAirFall = false;
	/** 原作 落地中转 → 地面：|落地竖直速度| < 150，不播落地动画。 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	bool bLandSkip = false;
	/** 原作 中速落地 → 重落地（中速档、没有方向输入、不是走路模式）；否则轻落地。落地时锁定。 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	bool bLandHeavy = false;
	/** 状态机过渡用的组合：起跳 / 下落 → 地面（不播落地）或 → 落地（原作 落地中转 的两个出口，翻滚落地暂缺）。 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	bool bJumpToGround = false;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	bool bJumpToLand = false;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	bool bFallToGround = false;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	bool bFallToLand = false;
	/** 落地档位：1 高速、2 中速、3 低速（原作 LandSpeedIndex 的取值，见 LandSpeedThresholds）。 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	int32 LandSpeedIndex = 3;
	/** 原作 落地 → 地面：落地动画剩余 < 0.1 秒，或已离地。 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	bool bLandFinish = false;
	/** 原作 落地 → 落地运动 的四条过渡（混合 0.2 / 0.25 / 0.3 / 0.2），本项目落地运动并入地面。 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	bool bLandExitHigh = false;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	bool bLandExitMid = false;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	bool bLandExitWalk = false;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	bool bLandExitLow = false;
	/**
	 * SM_Ground 的 Idle → Move（优先于 Idle → StartMove）：落地后 0.3 秒内、有移动输入、已经有移动速度时为真。
	 * 带着速度落地（空中闪避接奔跑、空中技能落地）直接进移动循环，不从起步动画的 0 速开始（起步动画的根运动会先把速度刹到 0）。
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wuwa|Animation|Jump")
	bool bLandIntoMove = false;
	double LastLandWorldTime = -1.0;

	/** 动画蓝图里有没有跳跃 / 落地这一套（SM_Jump 状态机）。 */
	bool HasJumpLandGraph();
	/** 起跳动画还在播：Locmotion 在 JumpStart 且 SM_Jump 在 JumpTakeoff。游戏线程，读最近一次状态机更新的结果。 */
	bool IsJumpTakeoffPlaying();
#pragma endregion

	/** 在 Event Graph/状态进入事件中调用：清除角色冲刺需求和开放窗口，并同步本实例的动画快照。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Locomotion|Sprint", meta = (NotBlueprintThreadSafe))
	void ResetSprintDesire();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Wuwa|Locomotion")
	FWuwaLocomotionAnimData LocomotionData;

	/** 每个 AnimInstance 独占的采样对象；游戏线程更新。AnimGraph 请读 LocomotionData。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Wuwa|Animation")
	TObjectPtr<UWuwaAnimLogicParams> AnimLogicParams;


#pragma region Debug

	/** 游戏线程调试接口：指定状态机的当前逻辑状态；找不到或尚未初始化时返回 None。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Animation|Debug")
	FName GetDebugStateName(FName MachineName);

	/** 返回指定状态的内部权重 0..1；找不到、未初始化或没有贡献时返回 0。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Animation|Debug")
	float GetDebugStateWeight(FName MachineName, FName StateName);

	/** 一次采集当前状态和所有混合状态；每个嵌套状态机需使用自己的名称查询。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Animation|Debug")
	FWuwaDebugAnimStateMachine GetDebugStateMachineData(FName MachineName);

	/** 可直接接 Print String 或 UMG；包含状态机权重、当前状态和各状态百分比。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Animation|Debug")
	FString GetDebugStateMachineText(FName MachineName);

	/** 主动调用才显示；同一实例和状态机替换同一条屏幕消息，不刷日志。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Animation|Debug", meta = (DevelopmentOnly))
	void PrintDebugStateMachine(FName MachineName, float Duration = 0.1f);

#pragma endregion Debug

protected:
	/** 以下阈值是 Demo 起点，不是从原作还原的数值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Locomotion|Tuning", meta = (ClampMin = "0", Units = "cm/s"))
	float MovingEnterThreshold = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Locomotion|Tuning", meta = (ClampMin = "0", Units = "cm/s"))
	float MovingExitThreshold = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Locomotion|Tuning", meta = (ClampMin = "0", ClampMax = "1"))
	float MoveIntentThreshold = 0.01f;

private:
	void UpdateFootIK(float DeltaSeconds, ACharacter* Character, UWuwaMovementComponent* Movement);
	bool bFootIKPlatformMoving = false;
	float FootIKPlatformAlpha = 1.f;
	float FootIKSlopeDiminish = 0.f;
	// 游戏线程采样。几何原型回退是本项目适配，并非 Kuro 原生公式的还原。
	FVector2D CachedStopFootRelation = FVector2D::ZeroVector;
	bool bCachedStopFootPositionValid = false;
	// 仅动画更新回调访问；初始化/清理时清空，节点指针不跨实例生命周期保留。
	TMap<const FAnimNode_SequencePlayer*, int32> AppliedStopEntrySerials;
	/** 不长期强引用旧 Pawn；切人、重新初始化和预览无 Pawn 时会清空快照。 */
	TWeakObjectPtr<ACharacter> CachedCharacter;
	TWeakObjectPtr<UWuwaMovementComponent> CachedMovement;
	void ResetAnimationData();
	void UpdateJumpLand(const UWuwaMovementComponent* Movement);
	/** 状态机 / 状态名 → 当前状态名、某状态的相关动画时间（找不到返回 false）。 */
	FName GetMachineState(FName Machine);
	bool GetStateAnimTime(FName Machine, FName State, float& OutTime, float& OutRemaining, float& OutFraction);
	int32 SeenJumpSerial = 0;
	int32 SeenLandSerial = 0;
	bool bJumpLandSerialsSeeded = false;
};
