// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementTypes.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateTypes.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementSettings.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaSkillMotion.h"
#include "WuwaMovementComponent.generated.h"

class UWuwaRoleGaitBridgeComponent;
class UWuwaUnifiedStateBridgeComponent;
class UWuwaInputIntentComponent;

/**
 *
 */
//可以在蓝图中看到这个ENUM
UENUM(BlueprintType)
namespace ECustomMoveMode
{
	enum Type
	{
		MOVE_Climb UMETA(DisplayName = "Climb Mode")
	};
}



UCLASS()
class WUWA_API UWuwaMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:

	/** 原作 CanWalkPress：运动状态的位置为 Ground 才能切换走跑偏好；本项目另外要求未蹲伏且由本端驱动状态。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Locomotion")
	bool CanToggleWalkPreference() const;

	/** 运动状态组件保存的走跑偏好，以步态表示（Walk / Run）。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Locomotion")
	EWuwaGait GetDesiredGait() const;
	/** 出生时的走跑偏好配置，运动状态组件初始化时读取。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Locomotion")
	EWuwaGait GetInitialDesiredGait() const { return DesiredGait; }

	/** 运动状态当前采用的步态（决定速度配置）。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Locomotion")
	EWuwaGait GetAllowedGait() const;

	/** 由角色组装时注入；CMC 每帧物理更新前从这里读取移动意图，不去找角色或控制器。 */
	void BindInputIntent(UWuwaInputIntentComponent* InInputIntent);
	/** Ground handoff after possession; seed physical facts without simulating an extra movement frame. */
	void RestoreSwitchMotion(const FVector& InVelocity, const FVector& InLastInput, const FVector& InAcceleration);

	/** 由角色组装时注入运动状态与步态组件；RoleGait 传空表示不再驱动步态（角色结束）。 */
	void BindMovementState(UWuwaUnifiedStateBridgeComponent* InUnifiedState, UWuwaRoleGaitBridgeComponent* InRoleGait);

	/** 物理模式事实：移动模式变化时交给运动状态组件同步位置。 */
	UFUNCTION(BlueprintPure, Category="Wuwa|State") EWuwaPositionState ReadPositionState() const;
	UFUNCTION(BlueprintPure, Category="Wuwa|State") FWuwaUnifiedStateData GetUnifiedStateData() const;
	/** 原作 CMC 监听 CharOnUnifiedMoveStateChanged 更新速度配置；步态维度是本项目扩展。 */
	UFUNCTION() void HandleMoveStateChanged(EWuwaMoveState OldState, EWuwaMoveState NewState);
	UFUNCTION() void HandleGaitChanged(EWuwaGait OldGait, EWuwaGait NewGait);
	UFUNCTION(BlueprintCallable, Category="Wuwa|Movement") void RefreshMovementSettings();
	/** 可选统一配置；未指定时沿用现有 Walk/Run/Sprint 速度与 CMC 原有加速度/摩擦。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wuwa|Movement") TObjectPtr<UWuwaMovementSettings> MovementSettings;

#pragma region Override Function
	virtual void OnMovementModeChanged(EMovementMode PrevMode, uint8 PrevCustomMode) override;
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
#pragma endregion


#pragma region Common
	bool PlayerisInputing();
	/** 当前移动输入的世界方向（水平、单位长度）；没有输入返回 false。 */
	bool GetMoveInputDirection(FVector& OutDirection) const;
#pragma endregion

#pragma region SkillMotion
	/**
	 * 技能期间对根运动的修正：位移吸附到目标（原作 TsAnimNotifyStatePositionBranchTarget）和技能转向（原作 UpdateAllSkillRotator）。
	 * 在根运动生效之前改（ProcessRootMotionPostConvertToWorld），走 CMC 正常的移动和网络预测；这一帧没有动画根运动时在 Tick 末尾补一次。
	 */

	/** 位移吸附窗口开始；Owner 是开启它的通知，结束时只清自己开的。 */
	void BeginMoveToTarget(const void* Owner, const FWuwaMoveToTargetConfig& Config, AActor* Target, FName Socket, float TotalTime);
	void EndMoveToTarget(const void* Owner);
	bool IsMovingToTarget() const { return MoveToTargetOwner != nullptr; }
	/** 最近一帧位移吸附走的分支（测试 / 调试用）。 */
	FWuwaMoveToTargetResult::EBranch GetLastMoveToTargetBranch() const { return LastMoveToTargetBranch; }

	/** 原作 SetSkillCanRotate：关掉时清掉转向参数和转向目标。 */
	void SetSkillCanRotate(bool bCanRotate);
	/** 原作 SetSkillRotateToTarget（转向偏移会取反并夹在 ±180，同原作）。 */
	void SetSkillRotateToTarget(bool bToTarget, bool bUseOffset = false, float Offset = 0.f, float PauseThreshold = 0.f, float ResumeThreshold = 0.f);
	/** 原作 SetSkillRotateSpeed（度 / 秒）。 */
	void SetSkillRotateSpeed(float Speed) { SkillRotate.Speed = Speed; }
	/** 转向的目标（技能目标及其骨骼）。 */
	void SetSkillRotateTarget(AActor* Target, FName Socket);
	/** 当前要转向的方向（技能目标方向，含偏移和停转阈值）；没有时为零向量。 */
	FVector GetSkillRotateDirection();
	const FWuwaSkillRotateState& GetSkillRotateState() const { return SkillRotate; }
	/** 平滑旋转用的窗口进度（原作 AnsRotateParam.NowTime / 总时长）。 */
	float SkillRotateNotifyTime = 0.f;
	float SkillRotateNotifyDuration = 0.f;
#pragma endregion


#pragma region Jump
	/**
	 * 原作 OnJump：不给起跳初速度，直接切到下落模式；整段起跳（向前 + 升起再落回）都来自起跳动画的根运动。
	 * 起跳期间动画实例必须是 RootMotionFromEverything（状态机里的起跳动画才会被提取根运动；角色动画本来就是，不是的话临时切过去），
	 * 并且根运动的 Z 不再被下落模式丢掉；起跳动画播完（进入空中循环）或落地后恢复原模式，之后由 CMC 正常的重力和空中控制接手。
	 * 只检查物理条件，打断规则由调用方先判断。
	 * 原作 CanJumpPress：落地后 2 帧内不能跳；OnJump：立刻停掉所有蒙太奇（Montage_Stop(0)）。
	 * bLimitSpeed：原作打断了技能时调用 LimitMaxSpeed，把速度限制在冲刺速度以内。
	 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Movement|Jump")
	bool BeginRootMotionJump(bool bLimitSpeed = false);
	UFUNCTION(BlueprintPure, Category = "Wuwa|Movement|Jump")
	bool IsJumpRootMotionActive() const { return bJumpRootMotion; }
	/** 每次起跳递增；动画实例据此识别一次新的起跳请求。 */
	int32 GetJumpSerial() const { return JumpSerial; }
	/** 每次落地递增；落地瞬间的速度（Z 还没清零）供动画选落地档位（原作 FallingSpeed / AirRelativeVelocity）。 */
	int32 GetLandSerial() const { return LandSerial; }
	const FVector& GetLastLandingVelocity() const { return LastLandingVelocity; }
	/**
	 * 原作"落地运动"（有移动输入时落地直接接着跑，不从静止起步）：水平速度 = 输入方向 × min(这次滞空里的最大水平速度, MaxCarrySpeed)。
	 * 用滞空最大速度而不是落地那一帧的速度：起跳动画的根运动在最后几帧收尾到接近 0（跑跳落地那一帧只剩 144 cm/s），照搬会从静止重新加速。
	 * 没有移动输入、或这次滞空本来就没水平速度（原地跳）时不改。普通落地在 ProcessLanded 里按当前步态最大速度调用；空中闪避落地按冲刺速度调用。
	 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Movement|Jump")
	bool CarryLandingSpeed(float MaxCarrySpeed);
	float GetAirPeakHorizontalSpeed() const { return AirPeakHorizontalSpeed; }
	/** 冲刺步态的最大速度（移动设置里的 Sprint，没有设置时用 SprintSpeed）。 */
	float GetSprintMaxSpeed() const;
#pragma endregion

#pragma region WalkOffLedge
	/**
	 * 原作 BaseMoveComponent.SetWalkOffLedgeRecord（边缘保护，可叠加）：false = 申请保护，计数 +1，第一个申请立刻禁止走出边缘；
	 * true = 归还，计数 -1，归零后延迟 1 帧（原作 WALK_OFF_LEDGE_DELAY_FRAME）再允许。技能（技能表 bWalkOffLedge）和以后的"边缘保护"通知都走这里。
	 */
	void SetWalkOffLedgeRecord(bool bAllow);
	int32 GetWalkOffProtectCount() const { return WalkOffCount; }
#pragma endregion

#pragma region ForceSpeed
	/**
	 * 原作 BaseMoveComponent.SetForceSpeed：立刻把速度设成 Speed，本帧移动更新之后再设一次（原作 ApplyForceSpeedAndRecordSpeed），
	 * 之后照常受重力 / 根运动影响（不是持续锁速）。下落攻击用它往下冲（-5000），技能结束时设 0。
	 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Movement")
	void SetForceSpeed(const FVector& Speed);

	/** 原作 Kuro_GetBlockActorWhenMove：最近一次移动被挡住（撞墙、落在不能站的地方）时挡住的 Actor；上一帧以来没被挡住返回空。 */
	AActor* GetBlockActorWhenMove() const;

	/**
	 * 空中不让动画根运动改速度，地面上也不按根运动离地（由设置的技能在结束时清除）。角色 ABP 是 RootMotionFromEverything：上一段蒙太奇淡出时它的根运动还在，
	 * 对蒙太奇实例 PushDisableRootMotion 也没用（那只管 FromMontagesOnly）。下落攻击的下落循环要靠 SetForceSpeed 往下冲，
	 * 不然起手段淡出的那 0.25 秒根运动把速度盖掉，变成从 0 开始自由落体（原作 Kuro 只取蒙太奇根运动，没有这个问题）。
	 */
	void SetIgnoreAirRootMotion(bool bIgnore) { bIgnoreAirRootMotion = bIgnore; }
	bool IsIgnoringAirRootMotion() const { return bIgnoreAirRootMotion; }

	/**
	 * 接下来 Seconds 秒（游戏时间）动画根运动不改速度，速度归输入 / 重力管。用在技能落地直接结束、接着跑的时候：
	 * ABP 从整个姿势取根运动，结束的蒙太奇淡出期间它收尾的减速根运动会把带过来的速度拖到 0。
	 */
	void SuppressAnimRootMotion(float Seconds);
#pragma endregion

#pragma region Climb
	bool IsClimbing() const;
	bool CanEnterClimbState();
	bool ProcessClimbSurfaces();
	bool IsFacingClimbableSurface();
	FQuat GetClimbRotation(float Deltatime);
	void SnapMovementToClimbableSurface(float Deltatime);
	bool CheckShouldStopClimbing();
	void StopClimbing();
#pragma endregion

#pragma region ClimbTraces

	bool ClimbableSurfacesTrace();
	bool ClimbableEyeSiteTrace();

	TArray<FHitResult> DoCapsuleMultipleforObjectTrace(const FVector& StartPos, const FVector& EndPos, bool bDrawTraceOutSide);
	FHitResult DoEyeSideTrace(const FVector& StartPos, const FVector& EndPos, bool bDrawTraceOutSide);
#pragma endregion

#pragma region ClimbVaribles
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WuwaMoveComp|Climb Capsule Trace")
	float ClimbCapsuleTraceRadius = 50.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WuwaMoveComp|Climb Capsule Trace")
	float ClimbCapsuleTraceHeight = 72.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WuwaMoveComp|Climb Capsule Trace")
	float ClimbTraceDistance = 55.f;  

	/** 攀爬检测打到哪些物体类型（空数组时引擎每次都报 "Invalid object types" 并返回没命中，攀爬永远进不去）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WuwaMoveComp|Climb Capsule Trace")
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes = {
		UEngineTypes::ConvertToObjectType(ECC_WorldStatic), UEngineTypes::ConvertToObjectType(ECC_WorldDynamic) };

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WuwaMoveComp|Climb Capsule Trace")
	TArray<AActor*> ActorsToIgnore;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WuwaMoveComp|ClimbableAngleLimit")
	float MinClimbSurfaceAngle = 20;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WuwaMoveComp|ClimbableAngleLimit")
	float MaxClimbSurfaceAngle = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WuwaMoveComp|ClimbableAngleLimit")
	float MaxClimbEntryAngle = 30;

	TArray<FHitResult> ClimbableSurfaceResults;
	FHitResult ClimbableEyesiteResult;

	FVector ProcessedSurfaceResults;
	FVector ProcessedSurfaceNomal;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "WuwaMoveComp|ClimbableAngleLimit")
	float MaxClimbBrakingDeceleration = 500.f;

#pragma endregion

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void InitializeComponent() override;

	void PhysCustom(float deltaTime, int32 Iterations) override;
	/**
	 * 地面上动画根运动往上走就离地（原作：E 起手从地面腾空约 3.5 米、心眼·跃，蒙太奇上没有额外的离地通知）。
	 * UE 行走模式会把根运动的竖直分量扔掉；这里在行走物理开始前，向上速度超过这一帧重力的量（同 UE 根运动源的离地判断）就切到下落，
	 * 下落中按动画根运动的完整速度走（ConstrainAnimRootMotionVelocity）。
	 */
	virtual void PhysWalking(float deltaTime, int32 Iterations) override;

	void PhysClimbing(float deltaTime, int32 Iterations);

protected:
	/** 下落模式下有动画根运动时用根运动的完整速度，包括 Z（原作：起跳、空中技能的竖直位移都在动画里）；UE 默认会把 Z 换成重力的结果。 */
	virtual FVector ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const override;
	/** 起跳还在上升（或刚离地、根运动 Z 还是 0）时不算落地，贴着斜坡滑过去，免得起跳第一帧就被上坡"接住"。 */
	virtual bool IsValidLandingSpot(const FVector& CapsuleLocation, const FHitResult& Hit) const override;
	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;
	/** 记下挡住移动的 Actor（GetBlockActorWhenMove）。 */
	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.f, const FVector& MoveDelta = FVector::ZeroVector) override;

	/** 仅为出生时的走跑偏好配置；运行值由 C# 运动状态组件维护。 */
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Locomotion")
	EWuwaGait DesiredGait = EWuwaGait::Run;

	/** 可调 Demo 默认值，不是原作速度参数。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Locomotion", meta = (ClampMin = "0", Units = "cm/s"))
	float WalkSpeed = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Locomotion", meta = (ClampMin = "0", Units = "cm/s"))
	float SprintSpeed = 900.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Locomotion", meta = (ClampMin = "0", Units = "cm/s"))
	float RunSpeed = 500.f;

private:
	/** 原作 MoveComponent 每帧读取输入方向：地面按世界方向移动，攀爬沿墙面移动。 */
	void ApplyMoveIntent();

	UPROPERTY(Transient)
	TObjectPtr<UWuwaInputIntentComponent> InputIntent;
	UPROPERTY(Transient)
	TObjectPtr<UWuwaUnifiedStateBridgeComponent> UnifiedState;
	UPROPERTY(Transient)
	TObjectPtr<UWuwaRoleGaitBridgeComponent> RoleGait;

	/** 根运动转成世界空间后、生效之前的修正（绑在 ProcessRootMotionPostConvertToWorld 上）。 */
	FTransform ProcessSkillRootMotion(const FTransform& WorldRootMotion, UCharacterMovementComponent* Movement, float DeltaSeconds);
	/** 按本帧根运动（世界空间）算修正后的位移和朝向；返回是否改了朝向。 */
	bool ComputeSkillMotion(const FVector& RootMotion, float DeltaSeconds, FVector& OutDisplacement, FRotator& OutFacing);
	/** 目标点：有骨骼用骨骼位置（不加目标半径），否则用目标位置和目标胶囊半径。 */
	static bool ResolveTargetPoint(const AActor* Target, FName Socket, FVector& OutLocation, float& OutRadius);

	UPROPERTY(Transient)
	FWuwaMoveToTargetConfig MoveToTargetConfig;
	const void* MoveToTargetOwner = nullptr;
	TWeakObjectPtr<AActor> MoveToTargetActor;
	FName MoveToTargetSocket;
	float MoveToTargetTime = 0.f;
	float MoveToTargetDuration = 0.f;
	FWuwaMoveToTargetResult::EBranch LastMoveToTargetBranch = FWuwaMoveToTargetResult::EBranch::None;
	FWuwaSkillRotateState SkillRotate;
	TWeakObjectPtr<AActor> SkillRotateActor;
	FName SkillRotateSocket;
	/** 本次 Tick 里根运动修正已经执行过（没有就在 Tick 末尾补）。 */
	bool bSkillMotionAppliedThisTick = false;

	/** 结束起跳根运动：恢复动画实例原来的根运动模式。 */
	void EndRootMotionJump();
	/** 起跳动画是否还在播（问动画实例）；动画实例不是 Wuwa 的时候按这一帧有没有提取到根运动判断。 */
	bool IsJumpTakeoffPlaying() const;
	bool bJumpRootMotion = false;
	/** 这次 Tick 里起跳根运动被用过（ConstrainAnimRootMotionVelocity 是 const，所以 mutable）。 */
	mutable bool bJumpRootMotionUsedThisTick = false;
	TEnumAsByte<ERootMotionMode::Type> SavedRootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
	/**
	 * 技能蒙太奇的根运动不按淡入权重打折（原作 Kuro 只取蒙太奇根运动，淡入期间也是全额）：
	 * ABP 配的是 RootMotionFromEverything（停步 / 起跳要用状态机里的根运动），这个模式下蒙太奇根运动乘淡入权重，
	 * 空中闪避这种前 0.2 秒爆发的位移会被吃掉大半。有根运动蒙太奇在播（没开始淡出）时切成 FromMontagesOnly，播完 / 淡出时切回 ABP 原来的模式。
	 */
	void UpdateMontageRootMotionMode();
	TEnumAsByte<ERootMotionMode::Type> DefaultRootMotionMode = ERootMotionMode::RootMotionFromEverything;
	bool bCapturedDefaultRootMotionMode = false;
	/** 原作 SetWalkOffLedge：允许 = bCanWalkOffLedges、PerchRadiusThreshold 0、PerchAdditionalHeight 40；禁止 = 不能走出边缘，悬空判定用胶囊半径 / 两倍半径。 */
	void SetWalkOffLedge(bool bAllow);
	int32 WalkOffCount = 0;
	bool bWalkOffRestorePending = false;
	uint64 WalkOffRestoreFrame = 0;
	int32 JumpSerial = 0;
	int32 LandSerial = 0;
	/** 进入地面的帧号（原作 GroundedFrame）。 */
	uint64 GroundedFrame = 0;
	FVector LastLandingVelocity = FVector::ZeroVector;
	/** 这次滞空里的最大水平速度（CarryLandingSpeed 用）；落地后的第一次滞空重新开始记。 */
	float AirPeakHorizontalSpeed = 0.f;
	bool bAirPeakReset = true;
	/** SetForceSpeed 等着本帧移动更新后再设一次。 */
	bool bForceSpeedPending = false;
	FVector ForceSpeed = FVector::ZeroVector;
	bool bIgnoreAirRootMotion = false;
	double RootMotionSuppressedUntil = -1.0;
	TWeakObjectPtr<AActor> BlockActorWhenMove;
	uint64 BlockActorFrame = 0;

	UWuwaRoleGaitBridgeComponent* ResolveGaitComponent() const;
	UWuwaUnifiedStateBridgeComponent* ResolveUnifiedState() const;
	bool bCapturedDefaultMovementSettings = false;
	FWuwaGaitMovementSettings DefaultMovementSettings;

};
