#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimInstance.h"

#include "Animation/AnimNode_StateMachine.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimDataLibrary.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimLogicParams.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaLocomotionMath.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "GameFramework/Character.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"
#include "Kismet/KismetSystemLibrary.h"

void UWuwaAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	CachedCharacter.Reset();
	CachedMovement.Reset();
	if (!AnimLogicParams)
	{
		AnimLogicParams = NewObject<UWuwaAnimLogicParams>(this);
	}
	ResetAnimationData();
}

void UWuwaAnimInstance::NativeUninitializeAnimation()
{
	CachedCharacter.Reset();
	CachedMovement.Reset();
	ResetAnimationData();
	Super::NativeUninitializeAnimation();
}

void UWuwaAnimInstance::ResetAnimationData()
{
	FootIKFrame = FWuwaFootIKFrame{};
	bFootIKPlatformMoving = false;
	FootIKPlatformAlpha = 1.f;
	FootIKSlopeDiminish = 0.f;
	LocomotionData = FWuwaLocomotionAnimData{};
	bCachedStopFootPositionValid = false;
	CachedStopFootRelation = FVector2D::ZeroVector;
	RoleStopStartTime = 0.f;
	bRoleStopEntryMapped = false;
	RoleStopEntrySerial = 0;
	AppliedStopEntrySerials.Reset();
	bJumpRequested = bJumpRightFoot = bJumpCanLand = bAirFall = bLandSkip = bLandHeavy = false;
	bJumpToGround = bJumpToLand = bFallToGround = bFallToLand = false;
	bLandFinish = bLandExitHigh = bLandExitMid = bLandExitWalk = bLandExitLow = false;
	JumpBlend = 0.f;
	LandSpeedIndex = 3;
	bJumpLandSerialsSeeded = false;
	if (AnimLogicParams)
	{
		AnimLogicParams->Reset();
	}
}

void UWuwaAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	check(IsInGameThread());

	// NativeUpdateAnimation 在游戏线程采集 UObject 数据。
	// 不把这些直接访问挪到 NativeThreadSafeUpdateAnimation。
	ACharacter* Character = Cast<ACharacter>(TryGetPawnOwner());
	UWuwaMovementComponent* Movement = IsValid(Character)
		? Cast<UWuwaMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	//切换角色就更新,但是其实一般是不会切换的,因为一个角色对应一个BP
	if (CachedCharacter.Get() != Character || CachedMovement.Get() != Movement)
	{
		CachedCharacter = Character;
		CachedMovement = Movement;
		ResetAnimationData();
	}

	if (!AnimLogicParams)
	{
		AnimLogicParams = NewObject<UWuwaAnimLogicParams>(this);
	}
	if (!UWuwaAnimDataLibrary::UpdateAnimationData(Movement, AnimLogicParams))
	{
		ResetAnimationData();
		return;
	}

	// 一次性提交本次更新结果；动画节点只消费快照，不反写 Movement/输入。
	// UE 随后调用 BlueprintUpdateAnimation，蓝图可以读到本次 NativeUpdate 的结果。
	LocomotionData = WuwaLocomotion::BuildAnimationData(AnimLogicParams->GetMoveData(),
		AnimLogicParams->GetStateData(), LocomotionData.bHasMovingSpeed,
		MovingEnterThreshold, MovingExitThreshold, MoveIntentThreshold);
	UpdateFootIK(DeltaSeconds, Character, Movement);
	// 在世界空间投影到角色前向/上向；导入骨架的局部 Y 不能直接当作角色前向。
	USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	const FName LeftFoot(TEXT("Bip001LFoot")), RightFoot(TEXT("Bip001RFoot"));
	bCachedStopFootPositionValid = Mesh && Character
		&& Mesh->GetBoneIndex(LeftFoot) != INDEX_NONE && Mesh->GetBoneIndex(RightFoot) != INDEX_NONE;
	if (bCachedStopFootPositionValid)
	{
		const FVector Difference = Mesh->GetSocketLocation(RightFoot) - Mesh->GetSocketLocation(LeftFoot);
		CachedStopFootRelation = FVector2D(FVector::DotProduct(Difference, Character->GetActorForwardVector()),
			FVector::DotProduct(Difference, Character->GetActorUpVector()));
		bCachedStopFootPositionValid = !CachedStopFootRelation.ContainsNaN();
	}
	UpdateJumpLand(Movement);
}

#pragma region JumpLand

FName UWuwaAnimInstance::GetMachineState(FName Machine)
{
	int32 MachineIndex = INDEX_NONE;
	const FBakedAnimationStateMachine* Description = nullptr;
	GetStateMachineIndexAndDescription(Machine, MachineIndex, &Description);
	return MachineIndex != INDEX_NONE && GetStateMachineInstance(MachineIndex) ? GetCurrentStateName(MachineIndex) : NAME_None;
}

bool UWuwaAnimInstance::GetStateAnimTime(FName Machine, FName State, float& OutTime, float& OutRemaining, float& OutFraction)
{
	int32 MachineIndex = INDEX_NONE;
	const FBakedAnimationStateMachine* Description = nullptr;
	GetStateMachineIndexAndDescription(Machine, MachineIndex, &Description);
	const int32 StateIndex = Description ? Description->FindStateIndex(State) : INDEX_NONE;
	if (MachineIndex == INDEX_NONE || StateIndex == INDEX_NONE) return false;
	OutTime = GetRelevantAnimTime(MachineIndex, StateIndex);
	OutRemaining = GetRelevantAnimTimeRemaining(MachineIndex, StateIndex);
	OutFraction = GetRelevantAnimTimeRemainingFraction(MachineIndex, StateIndex);
	return true;
}

bool UWuwaAnimInstance::HasJumpLandGraph()
{
	return GetStateMachineInstanceDesc(WuwaJumpLand::JumpMachine) != nullptr;
}

bool UWuwaAnimInstance::IsJumpTakeoffPlaying()
{
	return GetMachineState(WuwaJumpLand::Locomotion) == WuwaJumpLand::JumpStart
		&& GetMachineState(WuwaJumpLand::JumpMachine) == WuwaJumpLand::JumpTakeoff;
}

void UWuwaAnimInstance::UpdateJumpLand(const UWuwaMovementComponent* Movement)
{
	// 这里读到的状态机结果是上一次动画更新的；过渡在本次更新里读这些值。
	if (!bJumpLandSerialsSeeded)
	{
		// 新角色 / 重新初始化：不把初始化之前的起跳、落地当成新事件。
		SeenJumpSerial = Movement->GetJumpSerial();
		SeenLandSerial = Movement->GetLandSerial();
		bJumpLandSerialsSeeded = true;
	}
	const FName State = GetMachineState(WuwaJumpLand::Locomotion);
	const bool bInput = LocomotionData.bHasMoveInput;
	const bool bWalk = LocomotionData.bStateGroundWalk;

	if (Movement->GetJumpSerial() != SeenJumpSerial)
	{
		SeenJumpSerial = Movement->GetJumpSerial();
		bJumpRequested = Movement->IsJumpRootMotionActive();
		// 原作 InterfaceJumpPressed：跳跃速度 / 900 → BS_Jump 横轴（0 = Jump_Walk，1 = Jump_Run）。
		JumpBlend = FMath::Clamp(LocomotionData.GroundSpeed / 900.f, 0.f, 1.f);
		// 原作 跳跃状态机 状态选择：Feet_Position > 0 → 右脚起跳，否则左脚（原作站立动画没有这条曲线，取 0 → 左脚）。
		// 本项目地面动画没有导入这条曲线时按双脚前后判断：对过原作 Run_F，曲线为 +1 的区间正好是右脚在前（右脚减左脚的前向距离 > 0）。
		float FeetPosition = 0.f;
		if (GetCurveValue(TEXT("Feet_Position"), FeetPosition) && FMath::IsFinite(FeetPosition)) bJumpRightFoot = FeetPosition > 0.f;
		else bJumpRightFoot = LocomotionData.bHasMovingSpeed && bCachedStopFootPositionValid && CachedStopFootRelation.X > 0.f;
	}
	// 进入起跳后清掉；起跳根运动被取消（没进得去）也清掉。
	if (bJumpRequested && (State == WuwaJumpLand::JumpStart || !Movement->IsJumpRootMotionActive())) bJumpRequested = false;

	// 原作 起跳 → 落地中转：跳跃状态机在 跳跃空中循环 / Fail，或起跳动画剩余比例 < 0.5。
	const FName JumpState = GetMachineState(WuwaJumpLand::JumpMachine);
	float Time = 0.f, Remaining = 0.f, Fraction = 1.f;
	bJumpCanLand = State == WuwaJumpLand::JumpStart && (JumpState == WuwaJumpLand::JumpLoop || JumpState == WuwaJumpLand::JumpFail
		|| (GetStateAnimTime(WuwaJumpLand::JumpMachine, WuwaJumpLand::JumpTakeoff, Time, Remaining, Fraction) && Fraction < 0.5f));
	bAirFall = LocomotionData.bStateAir && !bJumpRequested;

	if (Movement->GetLandSerial() != SeenLandSerial)
	{
		SeenLandSerial = Movement->GetLandSerial();
		// 原作 FallingSpeed / LandSpeedIndex 是 KuroAnimInstanceRole 原生值，看不到实现：这里取落地瞬间的竖直速度，
		// 按 落地速度切割初始化 的两个阈值分三档（中速落地 = 2，高速落地 ≤ 1，其余低速）。
		const float FallingSpeed = FMath::Abs(Movement->GetLastLandingVelocity().Z);
		LandSpeedIndex = FallingSpeed >= LandSpeedThresholds.Y ? 1 : FallingSpeed >= LandSpeedThresholds.X ? 2 : 3;
		bLandSkip = FallingSpeed < 150.f;
		// 原作 落地状态机：中速落地 → (KeepSprint 冲刺落地) / (有输入或走路 轻落地) / 否则重落地；低速落地 → 轻落地。
		// 高速落地 的过渡排在最后，前面的 低速落地 恒为真，所以走不到。冲刺落地（中低段冲刺）缺 Climb_Boost_OnTopToSprint 动画，暂按 KeepSprint = false。
		bLandHeavy = LandSpeedIndex == 2 && !bInput && !bWalk;
		LastLandWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
	}
	bLandIntoMove = LastLandWorldTime >= 0.0 && GetWorld() && GetWorld()->GetTimeSeconds() - LastLandWorldTime < 0.3
		&& bInput && LocomotionData.bStateGround && LocomotionData.bHasMovingSpeed;

	// 原作 落地中转（进入条件 bStateGround）：|竖直速度| < 150 直接回地面，否则进落地。
	const bool bGround = LocomotionData.bStateGround;
	bJumpToGround = bJumpCanLand && bGround && bLandSkip;
	bJumpToLand = bJumpCanLand && bGround && !bLandSkip;
	bFallToGround = bGround && bLandSkip;
	bFallToLand = bGround && !bLandSkip;

	// 原作 落地 的出口（落地动画时间取上一帧）。
	bLandFinish = bLandExitHigh = bLandExitMid = bLandExitWalk = bLandExitLow = false;
	if (State == WuwaJumpLand::Land && GetStateAnimTime(WuwaJumpLand::Locomotion, WuwaJumpLand::Land, Time, Remaining, Fraction))
	{
		const bool bJump = bJumpRequested;
		const bool bLow = LandSpeedIndex == 3 || LandSpeedIndex == 0;
		// 落地 → 地面：剩余 < 0.1 或离地。
		bLandFinish = Remaining < 0.1f || !LocomotionData.bStateGround;
		// 高速档：有输入立刻走，没输入剩 0.3 秒走；起跳在剩 0.3 秒内可走（混合 0.2）。
		bLandExitHigh = (LandSpeedIndex == 1 && (bInput || Remaining <= 0.3f)) || (bJump && Remaining <= 0.3f);
		// 中速档：有输入且不是走路、播过 0.05 秒；没输入剩 1.1 秒；按跳跃随时（混合 0.25）。
		bLandExitMid = (LandSpeedIndex == 2 && bInput && !bWalk && Time >= 0.05f) || (LandSpeedIndex == 2 && !bInput && Remaining <= 1.1f) || bJump;
		// 走路模式：有输入（混合 0.3）。
		bLandExitWalk = (bLow || LandSpeedIndex == 2) && bInput && bWalk;
		// 低速档：有输入或跳跃且不是走路（混合 0.2）。
		bLandExitLow = bLow && (bInput || bJump) && !bWalk;
	}
}

#pragma endregion

void UWuwaAnimInstance::EnterRoleStop(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	// 状态再次进入时，即使旧 Stop 还在混合淡出、节点仍然相关，也必须重新设置时间。
	RoleStopEntrySerial = RoleStopEntrySerial == MAX_int32 ? 1 : RoleStopEntrySerial + 1;
	bRoleStopWalk = LocomotionData.StopGait == EWuwaGait::Walk;
	bRoleStopSprint = LocomotionData.StopGait == EWuwaGait::Sprint;
	RoleStopEntrySyncPosition = GetSyncGroupPosition(TEXT("Locomotion"));
	float FeetPosition = 0.f;
	const bool bHasFeetCurve = GetCurveValue(TEXT("Feet_Position"), FeetPosition) && FMath::IsFinite(FeetPosition);
	// 原作导出 ABP_BaseRole：Feet_Position >= 0 选冲刺右停，否则左停。
	// 无该曲线时比较首帧双脚原型距离，这是本项目适配，并非原作曲线公式。
	bool bGeometryLeft = true;
	if (bHasFeetCurve) bGeometryLeft = FeetPosition < 0.f;
	else if (bRoleSprintEntryFeetConfigured && bCachedStopFootPositionValid)
	{
		bGeometryLeft = (CachedStopFootRelation - RoleSprintLeftEntryFeet).SizeSquared()
			<= (CachedStopFootRelation - RoleSprintRightEntryFeet).SizeSquared();
	}
	bRoleStopLeft = bGeometryLeft;
	RoleStopStartTime = 0.f;
	bRoleStopEntryMapped = false;
	if (!bRoleStopSprint)
	{
		bool bLeftMatches = false, bRightMatches = false;
		const float LeftTime = UWuwaAnimDataLibrary::GetStopEntryTime(
			bRoleStopWalk ? RoleStopWalkLeft.Get() : RoleStopRunLeft.Get(), RoleStopEntrySyncPosition, bLeftMatches);
		const float RightTime = UWuwaAnimDataLibrary::GetStopEntryTime(
			bRoleStopWalk ? RoleStopWalkRight.Get() : RoleStopRunRight.Get(), RoleStopEntrySyncPosition, bRightMatches);
		// 原作 BaseAnim3 的 L 对应 Left->Right；当前 BaseAnim1 同名动作的首脚不同。
		// 因此按各资产首对标记选择，不能按文件 L/R 名称推断相位，也不匹配收尾恢复步。
		if (bLeftMatches || bRightMatches)
		{
			bRoleStopLeft = bLeftMatches && (!bRightMatches || bGeometryLeft);
			RoleStopStartTime = bRoleStopLeft ? LeftTime : RightTime;
			bRoleStopEntryMapped = true;
		}
	}
}

void UWuwaAnimInstance::UpdateRoleStopPlayer(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	if (FAnimNode_SequencePlayer* Player = Node.GetAnimNodePtr<FAnimNode_SequencePlayer>())
	{
		int32& AppliedSerial = AppliedStopEntrySerials.FindOrAdd(Player);
		if (AppliedSerial != RoleStopEntrySerial)
		{
			Player->SetAccumulatedTime(RoleStopStartTime);
			AppliedSerial = RoleStopEntrySerial;
		}
	}
}

void UWuwaAnimInstance::ResetSprintDesire()
{
	check(IsInGameThread());
	// 每次解析当前 Owner，避免切换角色后清到缓存里的旧 CMC；预览无角色时也可安全调用。
	ACharacter* Character = Cast<ACharacter>(TryGetPawnOwner());
	UWuwaMovementComponent* Movement = IsValid(Character)
		? Cast<UWuwaMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	// 冲刺需求由 RoleGait 保存，直接让它清掉。
	const AWuwaCharacter* WuwaCharacter = Cast<AWuwaCharacter>(Character);
	if (WuwaCharacter && IsValid(WuwaCharacter->RoleGaitComponent))
	{
		WuwaCharacter->RoleGaitComponent->ResetSprintRequest();
	}
	LocomotionData.SprintDesire = EWuwaSprintDesire::None;
	if (AnimLogicParams)
	{
		if (UWuwaAnimDataLibrary::UpdateAnimationData(Movement, AnimLogicParams))
		{
			// 清除需求可能已同步把 Sprint 改回 Walk/Run；本次快照必须一起刷新。
			LocomotionData = WuwaLocomotion::BuildAnimationData(AnimLogicParams->GetMoveData(),
				AnimLogicParams->GetStateData(), LocomotionData.bHasMovingSpeed,
				MovingEnterThreshold, MovingExitThreshold, MoveIntentThreshold);
		}
		else
		{
			ResetAnimationData();
		}
	}
}

#pragma region Debug

FWuwaDebugAnimStateMachine UWuwaAnimInstance::GetDebugStateMachineData(FName MachineName)
{
	check(IsInGameThread());
	FWuwaDebugAnimStateMachine Result;
	Result.MachineName = MachineName;

	int32 MachineIndex = INDEX_NONE;
	const FBakedAnimationStateMachine* Description = nullptr;
	GetStateMachineIndexAndDescription(MachineName, MachineIndex, &Description);
	const FAnimNode_StateMachine* Machine = MachineIndex != INDEX_NONE
		? GetStateMachineInstance(MachineIndex) : nullptr;
	if (!Machine || !Description)
	{
		return Result;
	}

	Result.bFound = true;
	const int32 CurrentStateIndex = Machine->GetCurrentState();
	if (!Description->States.IsValidIndex(CurrentStateIndex))
	{
		// 预览、初始化之前可能还没有当前状态。不要调用依赖内部描述已初始化的 GetStateIndex。
		return Result;
	}

	Result.bInitialized = true;
	Result.MachineWeight = GetInstanceMachineWeight(MachineIndex);
	Result.CurrentStateName = Description->States[CurrentStateIndex].StateName;
	for (int32 StateIndex = 0; StateIndex < Description->States.Num(); ++StateIndex)
	{
		const FBakedAnimationState& State = Description->States[StateIndex];
		if (State.bIsAConduit)
		{
			continue;
		}

		const float Weight = Machine->GetStateWeight(StateIndex);
		if (Weight > 0.f)
		{
			FWuwaDebugAnimStateWeight& Entry = Result.ActiveStates.AddDefaulted_GetRef();
			Entry.StateName = State.StateName;
			Entry.Weight = Weight;
			Entry.bIsCurrentState = StateIndex == CurrentStateIndex;
		}
	}
	Result.ActiveStates.StableSort([](const FWuwaDebugAnimStateWeight& A, const FWuwaDebugAnimStateWeight& B)
	{
		return A.Weight > B.Weight;
	});
	return Result;
}


FName UWuwaAnimInstance::GetDebugStateName(FName MachineName)
{
	return GetDebugStateMachineData(MachineName).CurrentStateName;
}

float UWuwaAnimInstance::GetDebugStateWeight(FName MachineName, FName StateName)
{
	const FWuwaDebugAnimStateMachine Data = GetDebugStateMachineData(MachineName);
	for (const FWuwaDebugAnimStateWeight& State : Data.ActiveStates)
	{
		if (State.StateName == StateName)
		{
			return State.Weight;
		}
	}
	return 0.f;
}

FString UWuwaAnimInstance::GetDebugStateMachineText(FName MachineName)
{
	const FWuwaDebugAnimStateMachine Data = GetDebugStateMachineData(MachineName);
	if (!Data.bFound)
	{
		return FString::Printf(TEXT("[%s] State machine not found"), *MachineName.ToString());
	}
	if (!Data.bInitialized)
	{
		return FString::Printf(TEXT("[%s] State machine not initialized"), *MachineName.ToString());
	}

	FString Text = FString::Printf(TEXT("[%s] Machine: %.4f%% | Current: %s\nState weights (inside this machine):"),
		*MachineName.ToString(), Data.MachineWeight * 100.f, *Data.CurrentStateName.ToString());
	if (Data.MachineWeight <= 0.f)
	{
		Text += TEXT("\nMachine inactive; internal state data may be stale.");
	}
	for (const FWuwaDebugAnimStateWeight& State : Data.ActiveStates)
	{
		Text += FString::Printf(TEXT("\n%s %s: %.4f%%"),
			State.bIsCurrentState ? TEXT("[*]") : TEXT("   "), *State.StateName.ToString(), State.Weight * 100.f);
	}
	return Text;
}

void UWuwaAnimInstance::PrintDebugStateMachine(FName MachineName, float Duration)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// 实例路径隔离预览/不同角色，状态机名隔离嵌套状态机的输出。
	const FName MessageKey(*FString::Printf(TEXT("Wuwa.AnimDebug.%s.%s"), *GetPathName(), *MachineName.ToString()));
	UKismetSystemLibrary::PrintString(this, GetDebugStateMachineText(MachineName),
		true, false, FLinearColor(0.f, 0.8f, 1.f), FMath::Max(Duration, 0.f), MessageKey);
#endif
}

#pragma endregion Debug
