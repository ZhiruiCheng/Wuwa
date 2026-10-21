// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Core/Utilities/DebugHelper.h"
#include "Game/Common/Debug/WuwaDebug.h"
#include "Engine/World.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputIntentComponent.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimInstance.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

#pragma region Common

void UWuwaMovementComponent::BindMovementState(UWuwaUnifiedStateBridgeComponent* InUnifiedState, UWuwaRoleGaitBridgeComponent* InRoleGait)
{
    UnifiedState = InUnifiedState;
    RoleGait = InRoleGait;
}

UWuwaRoleGaitBridgeComponent* UWuwaMovementComponent::ResolveGaitComponent() const
{
    return IsValid(RoleGait) ? RoleGait.Get() : nullptr;
}

UWuwaUnifiedStateBridgeComponent* UWuwaMovementComponent::ResolveUnifiedState() const
{
    // 与 RoleGait 同时装配；步态不再驱动（角色结束）后，这里也不再修改运动状态。
    return ResolveGaitComponent() && IsValid(UnifiedState) ? UnifiedState.Get() : nullptr;
}

bool UWuwaMovementComponent::CanToggleWalkPreference() const
{
    const auto* State = ResolveUnifiedState();
    return State && State->CanDriveState() && !IsCrouching()
        && State->GetStateData().PositionState == EWuwaPositionState::Ground;
}

EWuwaGait UWuwaMovementComponent::GetDesiredGait() const
{
    const auto* State = ResolveUnifiedState();
    return State ? (State->IsWalkPreferred() ? EWuwaGait::Walk : EWuwaGait::Run) : DesiredGait;
}

EWuwaGait UWuwaMovementComponent::GetAllowedGait() const
{
    return GetUnifiedStateData().Gait;
}

void UWuwaMovementComponent::BindInputIntent(UWuwaInputIntentComponent* InInputIntent)
{
    InputIntent = InInputIntent;
}

void UWuwaMovementComponent::RestoreSwitchMotion(const FVector& InVelocity, const FVector& InLastInput, const FVector& InAcceleration)
{
    check(IsInGameThread());
    if (!IsMovingOnGround()) return;
    Velocity = InVelocity.ContainsNaN() ? FVector::ZeroVector : FVector(InVelocity.X, InVelocity.Y, 0.0);
    Acceleration = InAcceleration.ContainsNaN() ? FVector::ZeroVector : InAcceleration.GetClampedToMaxSize(GetMaxAcceleration());
    ConsumeInputVector();
    AddInputVector(InLastInput.ContainsNaN() ? FVector::ZeroVector : InLastInput.GetClampedToMaxSize(1.0), true);
    ConsumeInputVector(); // Seeds LastInputVector for the animation's first sample; no duplicate input next tick.
    UpdateComponentVelocity();
}

void UWuwaMovementComponent::ApplyMoveIntent()
{
    if (!IsValid(InputIntent) || !PawnOwner) return;
    const FWuwaMoveIntent Move = InputIntent->GetMoveIntent();
    if (IsClimbing())
    {
        // 攀爬：前后沿墙面上下，左右沿墙面横向，保留轴的幅度。
        const FVector ForwardDirection = FVector::CrossProduct(-ProcessedSurfaceNomal, PawnOwner->GetActorRightVector());
        const FVector RightDirection = FVector::CrossProduct(-ProcessedSurfaceNomal, -PawnOwner->GetActorUpVector());
        AddInputVector(ForwardDirection * Move.Axis.Y);
        AddInputVector(RightDirection * Move.Axis.X);
    }
    else if (Move.bHasInput)
    {
        AddInputVector(Move.WorldDirection);
    }
}

EWuwaPositionState UWuwaMovementComponent::ReadPositionState() const
{
    if (IsClimbing()) return EWuwaPositionState::Climb;
    if (IsMovingOnGround()) return EWuwaPositionState::Ground;
    if (IsFalling() || MovementMode == MOVE_Flying) return EWuwaPositionState::Air;
    if (IsSwimming()) return EWuwaPositionState::Water;
    return EWuwaPositionState::None;
}

FWuwaUnifiedStateData UWuwaMovementComponent::GetUnifiedStateData() const
{
    return IsValid(UnifiedState) ? UnifiedState->GetStateData() : FWuwaUnifiedStateData{};
}

void UWuwaMovementComponent::HandleMoveStateChanged(EWuwaMoveState, EWuwaMoveState)
{
    RefreshMovementSettings();
}

void UWuwaMovementComponent::HandleGaitChanged(EWuwaGait, EWuwaGait)
{
    RefreshMovementSettings();
}

void UWuwaMovementComponent::RefreshMovementSettings()
{
    // 只按已接受状态选配置。这里没有输入判断、冲刺计时，也不修改 Velocity。
    if (!bCapturedDefaultMovementSettings)
    {
        DefaultMovementSettings.MaxSpeed = MaxWalkSpeed;
        DefaultMovementSettings.MaxAcceleration = MaxAcceleration;
        DefaultMovementSettings.GroundFriction = GroundFriction;
        DefaultMovementSettings.BrakingDeceleration = BrakingDecelerationWalking;
        bCapturedDefaultMovementSettings = true;
    }
    const EWuwaGait Gait = GetUnifiedStateData().Gait;
    FWuwaGaitMovementSettings Settings = DefaultMovementSettings;
    // 兼容原 GetMaxSpeed 的速度上限语义；MaxWalkSpeed 现在是执行输出，不能再用它作配置输入。
    Settings.MaxSpeed = Gait == EWuwaGait::Walk ? FMath::Min(WalkSpeed, DefaultMovementSettings.MaxSpeed)
        : FMath::Max(Gait == EWuwaGait::Sprint ? SprintSpeed : RunSpeed, DefaultMovementSettings.MaxSpeed);
    if (MovementSettings) Settings = MovementSettings->ForGait(Gait);
    MaxWalkSpeed = FMath::Max(0.f, Settings.MaxSpeed);
    MaxAcceleration = FMath::Max(0.f, Settings.MaxAcceleration);
    GroundFriction = FMath::Max(0.f, Settings.GroundFriction);
    BrakingDecelerationWalking = FMath::Max(0.f, Settings.BrakingDeceleration);
}

void UWuwaMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    // 原作 RoleGait 在自己的 OnTick 中决策。UE 默认让 CMC 先于 Character Tick，
    // 这里固定在本帧物理前调度脚本，避免形成 Tick 依赖环。
    if (auto* Gait = ResolveGaitComponent()) Gait->RefreshPolicy();
    // 输入在本帧物理前写入，Super 中立刻消费，不依赖输入事件和角色 Tick 的先后。
    ApplyMoveIntent();
    UpdateMontageRootMotionMode();
    bSkillMotionAppliedThisTick = false;
    bJumpRootMotionUsedThisTick = false;
    // 原作 BaseMoveComponent 每帧：边缘保护计数归零 1 帧后再恢复允许走出边缘。
    if (bWalkOffRestorePending && WalkOffRestoreFrame + 1 <= GFrameCounter)
    {
        WalkOffRestoreFrame = GFrameCounter;
        SetWalkOffLedge(WalkOffCount <= 0);
        bWalkOffRestorePending = false;
    }
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    // 滞空最大水平速度（落地接着跑用，见 CarryLandingSpeed）
    if (IsFalling())
    {
        const float Horizontal = static_cast<float>(ProjectToGravityFloor(Velocity).Size());
        AirPeakHorizontalSpeed = bAirPeakReset ? Horizontal : FMath::Max(AirPeakHorizontalSpeed, Horizontal);
        bAirPeakReset = false;
    }
    else
    {
        bAirPeakReset = true;
    }
    // 原作 ApplyForceSpeedAndRecordSpeed：移动更新之后再设一次强制速度，然后作废
    if (bForceSpeedPending)
    {
        bForceSpeedPending = false;
        Velocity = ForceSpeed;
        UpdateComponentVelocity();
    }
    // 起跳根运动在本帧物理里用完后再判断：起跳动画已经进入空中循环（或已落地）就交回重力。
    if (bJumpRootMotion && (!IsFalling() || !IsJumpTakeoffPlaying())) EndRootMotionJump();
    // 这一帧没有动画根运动（修正没执行）但窗口开着：按零根运动补一次（往目标推 / 转向；没有根运动可抵消）
    if (!bSkillMotionAppliedThisTick && (IsMovingToTarget() || SkillRotate.bCanRotate) && UpdatedComponent && DeltaTime > 0.f)
    {
        FVector Displacement;
        FRotator Facing;
        const bool bFace = ComputeSkillMotion(FVector::ZeroVector, DeltaTime, Displacement, Facing);
        if (bFace || !Displacement.IsNearlyZero())
        {
            FHitResult Hit;
            SafeMoveUpdatedComponent(Displacement, bFace ? Facing.Quaternion() : UpdatedComponent->GetComponentQuat(), true, Hit);
        }
    }
    if (CanEnterClimbState())
    {
        if (UWuwaDebugLibrary::IsDebugChannelEnabled(EWuwaDebugChannel::Climb)) Debug::Print(FString("Player Enter Climbing State"));
        SetMovementMode(MOVE_Custom, (uint8)ECustomMoveMode::MOVE_Climb);
    }
}

bool UWuwaMovementComponent::PlayerisInputing()
{
    return !Acceleration.IsNearlyZero();
}

bool UWuwaMovementComponent::GetMoveInputDirection(FVector& OutDirection) const
{
    if (!IsValid(InputIntent)) return false;
    const FWuwaMoveIntent Move = InputIntent->GetMoveIntent();
    OutDirection = FVector(Move.WorldDirection.X, Move.WorldDirection.Y, 0.0).GetSafeNormal();
    return Move.bHasInput && !OutDirection.IsNearlyZero();
}

#pragma endregion


#pragma region WalkOffLedge

void UWuwaMovementComponent::SetWalkOffLedgeRecord(bool bAllow)
{
    if (bAllow)
    {
        if (--WalkOffCount == 0)
        {
            bWalkOffRestorePending = true;
            WalkOffRestoreFrame = GFrameCounter;
        }
    }
    else if (++WalkOffCount == 1)
    {
        bWalkOffRestorePending = false;
        SetWalkOffLedge(false);
    }
}

void UWuwaMovementComponent::SetWalkOffLedge(bool bAllow)
{
    bCanWalkOffLedges = bAllow;
    const float Radius = CharacterOwner && CharacterOwner->GetCapsuleComponent() ? CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;
    PerchRadiusThreshold = bAllow ? 0.f : Radius;
    PerchAdditionalHeight = bAllow ? 40.f : 2.f * Radius;
}

#pragma endregion


#pragma region Jump

bool UWuwaMovementComponent::BeginRootMotionJump(bool bLimitSpeed)
{
    if (bJumpRootMotion || !CharacterOwner || !UpdatedComponent || !IsMovingOnGround() || IsCrouching()) return false;
    // 原作 CanJumpPress：GroundedFrame > Frame - 2 时不能跳。
    if (GroundedFrame + 2 > GFrameCounter) return false;
    UAnimInstance* Anim = CharacterOwner->GetMesh() ? CharacterOwner->GetMesh()->GetAnimInstance() : nullptr;
    if (!Anim) return false;
    if (bLimitSpeed)
    {
        // 原作 LimitMaxSpeed：速度超过冲刺速度就按比例缩到冲刺速度。
        const float Sprint = MovementSettings ? MovementSettings->ForGait(EWuwaGait::Sprint).MaxSpeed : SprintSpeed;
        if (Velocity.Size() > Sprint) Velocity = Velocity.GetSafeNormal() * Sprint;
    }
    // 原作 OnJump：Montage_Stop(0)。
    Anim->Montage_Stop(0.f);
    // 状态机里的起跳动画只有 FromEverything 才提取根运动。角色动画本来就是 FromEverything（停步动画的根运动也靠它），不是的话临时切过去；
    // 起跳动画开着 ForceRootLock，结束时切回也不会露出根骨位移。
    SavedRootMotionMode = Anim->RootMotionMode;
    Anim->SetRootMotionMode(ERootMotionMode::RootMotionFromEverything);
    bJumpRootMotion = true;
    ++JumpSerial;
    // 原作不给初速度：竖直速度清零，水平速度在第一帧根运动时被起跳动画的速度替换。
    Velocity = ProjectToGravityFloor(Velocity);
    SetMovementMode(MOVE_Falling);
    return true;
}

void UWuwaMovementComponent::EndRootMotionJump()
{
    if (!bJumpRootMotion) return;
    bJumpRootMotion = false;
    UAnimInstance* Anim = CharacterOwner && CharacterOwner->GetMesh() ? CharacterOwner->GetMesh()->GetAnimInstance() : nullptr;
    if (Anim) Anim->SetRootMotionMode(SavedRootMotionMode);
}

bool UWuwaMovementComponent::IsJumpTakeoffPlaying() const
{
    USkeletalMeshComponent* Mesh = CharacterOwner ? CharacterOwner->GetMesh() : nullptr;
    UWuwaAnimInstance* Anim = Mesh ? Cast<UWuwaAnimInstance>(Mesh->GetAnimInstance()) : nullptr;
    // 起跳中放了技能：蒙太奇盖住状态机（Slot 满权重时状态机不更新，会一直停在 JumpTakeoff），根运动也换成了蒙太奇的。
    // 这时起跳就算结束（不再拒绝落地），之后空中的位移按蒙太奇根运动走（含 Z，见 ConstrainAnimRootMotionVelocity）。
    if (Anim && Anim->IsAnyMontagePlaying()) return false;
    return Anim && Anim->HasJumpLandGraph() ? Anim->IsJumpTakeoffPlaying() : bJumpRootMotionUsedThisTick;
}

void UWuwaMovementComponent::SuppressAnimRootMotion(float Seconds)
{
    if (const UWorld* World = GetWorld()) RootMotionSuppressedUntil = World->GetTimeSeconds() + FMath::Max(Seconds, 0.f);
}

void UWuwaMovementComponent::UpdateMontageRootMotionMode()
{
    USkeletalMeshComponent* Mesh = CharacterOwner ? CharacterOwner->GetMesh() : nullptr;
    UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
    if (!Anim) return;
    if (!bCapturedDefaultRootMotionMode)
    {
        DefaultRootMotionMode = Anim->RootMotionMode;
        bCapturedDefaultRootMotionMode = true;
    }
    // 只处理"从整个姿势取根运动"的 ABP
    if (DefaultRootMotionMode != ERootMotionMode::RootMotionFromEverything) return;
    const FAnimMontageInstance* Instance = Anim->GetRootMotionMontageInstance();
    const bool bMontagePlaying = Instance && Instance->IsActive() && !Instance->IsStopped();
    if (bJumpRootMotion)
    {
        // 起跳根运动窗口里放了带根运动的技能（空中闪避等）：起跳窗口现在就结束（本来要等这一帧移动完才收），技能根运动从第一帧全额
        if (!bMontagePlaying) return;
        EndRootMotionJump();
    }
    const ERootMotionMode::Type Wanted = bMontagePlaying ? ERootMotionMode::RootMotionFromMontagesOnly : ERootMotionMode::RootMotionFromEverything;
    if (Anim->RootMotionMode != Wanted) Anim->SetRootMotionMode(Wanted);
}

FVector UWuwaMovementComponent::ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const
{
    // SuppressAnimRootMotion 窗口内：速度保持（地面上本帧也不做 CalcVelocity，窗口过后照常加速 / 刹到当前步态上限）
    if (const UWorld* World = GetWorld(); World && World->GetTimeSeconds() < RootMotionSuppressedUntil) return CurrentVelocity;
    // 原作（Kuro 根运动）：空中有动画根运动时，速度完全由根运动决定，包括 Z。
    // 起跳的抛物线、空中普攻的滞空 / 上挑 / 下落（AirAttack04 根骨 Z：-69 → +20 → -97）都在动画里；
    // UE 默认会在下落时把 Z 换成重力的结果，这里不这么做。没有根运动的帧（下落循环、蒙太奇淡出完）照常走重力。
    if (IsFalling())
    {
        // 下落攻击的下落循环：速度归 SetForceSpeed + 重力管（见 SetIgnoreAirRootMotion）
        if (bIgnoreAirRootMotion) return CurrentVelocity;
        if (bJumpRootMotion) bJumpRootMotionUsedThisTick = true;
        return RootMotionVelocity;
    }
    return Super::ConstrainAnimRootMotionVelocity(RootMotionVelocity, CurrentVelocity);
}

bool UWuwaMovementComponent::IsValidLandingSpot(const FVector& CapsuleLocation, const FHitResult& Hit) const
{
    if (bJumpRootMotion && GetGravitySpaceZ(Velocity) >= 0.f) return false;
    return Super::IsValidLandingSpot(CapsuleLocation, Hit);
}

void UWuwaMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
    // 进入行走模式时竖直速度会被清掉，先记下落地瞬间的速度。
    LastLandingVelocity = Velocity;
    ++LandSerial;
    Super::ProcessLanded(Hit, remainingTime, Iterations);
    // 原作落地运动：有输入、没在放技能（蒙太奇）就带着空中的速度接着跑（上限是当前步态的最大速度）
    const UAnimInstance* Anim = CharacterOwner && CharacterOwner->GetMesh() ? CharacterOwner->GetMesh()->GetAnimInstance() : nullptr;
    if (IsMovingOnGround() && !(Anim && Anim->IsAnyMontagePlaying())) CarryLandingSpeed(GetMaxSpeed());
}

float UWuwaMovementComponent::GetSprintMaxSpeed() const
{
    return MovementSettings ? MovementSettings->ForGait(EWuwaGait::Sprint).MaxSpeed : SprintSpeed;
}

bool UWuwaMovementComponent::CarryLandingSpeed(float MaxCarrySpeed)
{
    const AWuwaCharacter* Character = Cast<AWuwaCharacter>(CharacterOwner);
    if (!Character || MaxCarrySpeed <= 0.f) return false;
    const FWuwaPlayerInputState Input = Character->GetPlayerInputState();
    if (!Input.bHasMoveInput || Input.MoveWorldDirection.IsNearlyZero()) return false;
    const FVector Horizontal = ProjectToGravityFloor(Velocity);
    const float Speed = FMath::Min(FMath::Max(AirPeakHorizontalSpeed, static_cast<float>(Horizontal.Size())), MaxCarrySpeed);
    if (Speed <= Horizontal.Size() + 1.f && Horizontal.GetSafeNormal().Dot(Input.MoveWorldDirection.GetSafeNormal()) > 0.99f) return false;
    if (Speed < 1.f) return false;
    Velocity = (Velocity - Horizontal) + Input.MoveWorldDirection.GetSafeNormal2D() * Speed;
    UpdateComponentVelocity();
    return true;
}

#pragma endregion


#pragma region SkillMotion

void UWuwaMovementComponent::InitializeComponent()
{
    Super::InitializeComponent();
    ProcessRootMotionPostConvertToWorld.BindUObject(this, &UWuwaMovementComponent::ProcessSkillRootMotion);
}

void UWuwaMovementComponent::BeginMoveToTarget(const void* Owner, const FWuwaMoveToTargetConfig& Config, AActor* Target, FName Socket, float TotalTime)
{
    MoveToTargetOwner = Owner;
    MoveToTargetConfig = Config;
    MoveToTargetActor = Target;
    MoveToTargetSocket = Socket;
    MoveToTargetTime = 0.f;
    MoveToTargetDuration = TotalTime;
    LastMoveToTargetBranch = FWuwaMoveToTargetResult::EBranch::None;
}

void UWuwaMovementComponent::EndMoveToTarget(const void* Owner)
{
    if (Owner != MoveToTargetOwner) return;
    MoveToTargetOwner = nullptr;
    MoveToTargetActor.Reset();
    MoveToTargetConfig.MoveCurve = nullptr;
}

void UWuwaMovementComponent::SetSkillCanRotate(bool bCanRotate)
{
    // 原作：关掉时重置转向参数（SZr.Reset）；转向目标由通知结束时另外清
    SkillRotate.bCanRotate = bCanRotate;
    if (!bCanRotate)
    {
        SkillRotate.bUseOffset = false;
        SkillRotate.Offset = 0.f;
        SkillRotate.PauseThreshold = SkillRotate.ResumeThreshold = 0.f;
        SkillRotate.bPaused = false;
    }
}

void UWuwaMovementComponent::SetSkillRotateToTarget(bool bToTarget, bool bUseOffset, float Offset, float PauseThreshold, float ResumeThreshold)
{
    SkillRotate.bRotateToTarget = bToTarget;
    SkillRotate.bUseOffset = bUseOffset;
    SkillRotate.Offset = -FMath::Clamp(Offset, -180.f, 180.f);
    SkillRotate.PauseThreshold = PauseThreshold;
    SkillRotate.ResumeThreshold = ResumeThreshold;
}

void UWuwaMovementComponent::SetSkillRotateTarget(AActor* Target, FName Socket)
{
    SkillRotateActor = Target;
    SkillRotateSocket = Socket;
}

FVector UWuwaMovementComponent::GetSkillRotateDirection()
{
    if (!UpdatedComponent) return FVector::ZeroVector;
    FVector TargetLocation;
    float Radius = 0.f;
    const bool bTarget = ResolveTargetPoint(SkillRotateActor.Get(), SkillRotateSocket, TargetLocation, Radius);
    return WuwaSkillMotion::GetSkillRotateDirection(SkillRotate, UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetForwardVector(),
        bTarget ? &TargetLocation : nullptr);
}

bool UWuwaMovementComponent::ResolveTargetPoint(const AActor* Target, FName Socket, FVector& OutLocation, float& OutRadius)
{
    if (!IsValid(Target)) return false;
    const ACharacter* Character = Cast<ACharacter>(Target);
    if (!Socket.IsNone() && Character && Character->GetMesh() && Character->GetMesh()->DoesSocketExist(Socket))
    {
        OutLocation = Character->GetMesh()->GetSocketLocation(Socket);
        OutRadius = 0.f;
        return true;
    }
    OutLocation = Target->GetActorLocation();
    OutRadius = Character && Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;
    return true;
}

FTransform UWuwaMovementComponent::ProcessSkillRootMotion(const FTransform& WorldRootMotion, UCharacterMovementComponent* Movement, float DeltaSeconds)
{
    bSkillMotionAppliedThisTick = true;
    if ((!IsMovingToTarget() && !SkillRotate.bCanRotate) || !UpdatedComponent || DeltaSeconds <= 0.f) return WorldRootMotion;
    FVector Displacement;
    FRotator Facing;
    FTransform Result = WorldRootMotion;
    if (ComputeSkillMotion(WorldRootMotion.GetTranslation(), DeltaSeconds, Displacement, Facing))
    {
        // CMC 在移动之后按 根运动旋转 x 当前朝向 转身：换成"转到 Facing"的增量
        Result.SetRotation(Facing.Quaternion() * UpdatedComponent->GetComponentQuat().Inverse());
    }
    Result.SetTranslation(Displacement);
    return Result;
}

bool UWuwaMovementComponent::ComputeSkillMotion(const FVector& RootMotion, float DeltaSeconds, FVector& OutDisplacement, FRotator& OutFacing)
{
    OutDisplacement = RootMotion;
    const FVector Location = UpdatedComponent->GetComponentLocation();
    const FRotator Current = UpdatedComponent->GetComponentRotation();
    bool bFace = false;
    OutFacing = Current;

    // 技能转向（原作 UpdateAllSkillRotator）：转向技能目标或输入方向，每帧最多转 速度 x 帧时间
    if (SkillRotate.bCanRotate)
    {
        FVector Direction = FVector::ZeroVector;
        if (SkillRotate.bRotateToTarget) Direction = GetSkillRotateDirection();
        else GetMoveInputDirection(Direction);
        Direction.Z = 0.0;
        if (!Direction.IsNearlyZero())
        {
            OutFacing.Yaw = WuwaSkillMotion::InterpYawConstantTo(Current.Yaw, Direction.Rotation().Yaw, DeltaSeconds, FMath::Abs(SkillRotate.Speed));
            bFace = true;
        }
    }

    // 位移吸附到目标；目标没了这一帧不动，进度照走（原作 RefreshTarget 失败时跳过 MoveToTarget）
    if (IsMovingToTarget())
    {
        FWuwaMoveToTargetFrame Frame;
        float TargetRadius = 0.f;
        if (ResolveTargetPoint(MoveToTargetActor.Get(), MoveToTargetSocket, Frame.TargetLocation, TargetRadius))
        {
            Frame.TargetLocation += MoveToTargetConfig.TargetOffset;
            Frame.Location = Location;
            Frame.RootMotion = RootMotion;
            Frame.TargetRadius = TargetRadius;
            const ACharacter* Character = CharacterOwner.Get();
            Frame.SelfRadius = Character && Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.f;
            Frame.NowTime = MoveToTargetTime;
            Frame.TotalTime = MoveToTargetDuration;
            Frame.DeltaTime = DeltaSeconds;
            const FWuwaMoveToTargetResult Result = WuwaSkillMotion::ComputeMoveToTarget(MoveToTargetConfig, Frame);
            OutDisplacement = Result.Displacement;
            LastMoveToTargetBranch = Result.Branch;
            if (Result.bFaceTarget)
            {
                OutFacing.Yaw = Result.Facing.Yaw;
                bFace = true;
            }
#if ENABLE_DRAW_DEBUG
            if (UWuwaDebugLibrary::IsDebugChannelEnabled(EWuwaDebugChannel::SkillMotion) && GetWorld())
            {
                const float Stop = MoveToTargetConfig.Distance + (MoveToTargetConfig.bIgnoreRadius ? 0.f : Frame.SelfRadius + TargetRadius);
                const FColor Color = Result.Branch == FWuwaMoveToTargetResult::EBranch::Pull ? FColor::Green
                    : Result.Branch == FWuwaMoveToTargetResult::EBranch::Hold ? FColor::Yellow : FColor::Silver;
                DrawDebugLine(GetWorld(), Location, Frame.TargetLocation, Color, false, 0.f, 0, 1.5f);
                DrawDebugCircle(GetWorld(), Frame.TargetLocation, Stop, 32, Color, false, 0.f, 0, 1.5f, FVector::ForwardVector, FVector::RightVector, false);
            }
#endif
        }
        MoveToTargetTime += DeltaSeconds;
    }
    return bFace;
}

#pragma endregion


#pragma region Climb Core

//When MovementState Changed This function will be called
void UWuwaMovementComponent::OnMovementModeChanged(EMovementMode PrevMode, uint8 PrevCustomMode)
{
	if (IsClimbing())
	{
		bOrientRotationToMovement = false;
	}

	if (PrevMode == MOVE_Custom && PrevCustomMode == ECustomMoveMode::MOVE_Climb)
	{
		bOrientRotationToMovement = true;
	}
	Super::OnMovementModeChanged(PrevMode, PrevCustomMode);
    if (bJumpRootMotion && !IsFalling()) EndRootMotionJump();
    if (IsMovingOnGround() && PrevMode != MOVE_Walking && PrevMode != MOVE_NavWalking) GroundedFrame = GFrameCounter;
    // 原作运动状态组件监听 CharMovementModeChanged 同步位置；随后本帧重算一次步态，不等下一次 Tick。
    if (auto* Gait = ResolveGaitComponent())
    {
        if (auto* State = ResolveUnifiedState()) State->HandleMovementModeChanged(ReadPositionState(), MovementMode.GetValue());
        Gait->RefreshPolicy();
    }
}

float UWuwaMovementComponent::GetMaxSpeed() const
{
    if (IsClimbing()) return 100.f;
    // 状态事件已把地面配置写入 MaxWalkSpeed；物理查询不再跨语言做步态决策。
    return Super::GetMaxSpeed();
}

float UWuwaMovementComponent::GetMaxAcceleration() const
{
	if (IsClimbing())
	{
		return 300.f;
	}
	return Super::GetMaxAcceleration();
}

bool UWuwaMovementComponent::IsClimbing() const
{
	return (MovementMode == MOVE_Custom) && (CustomMovementMode == (uint8)ECustomMoveMode::MOVE_Climb);
}

bool UWuwaMovementComponent::CanEnterClimbState()
{
	if (IsClimbing()) return false;
	if (IsFalling()) return false;
	if (!ClimbableSurfacesTrace()) return false;
	if (!ClimbableEyeSiteTrace()) return false;
	if (!IsFacingClimbableSurface()) return false;
	if (!PlayerisInputing()) return false;
	return true;
}

bool UWuwaMovementComponent::ProcessClimbSurfaces()
{
	if (ClimbableSurfaceResults.IsEmpty())
	{
		ProcessedSurfaceResults = FVector::Zero();
		ProcessedSurfaceNomal = FVector::Zero();
		return false;
	}
	FVector CurClimbabllSurfacesAveLoc = FVector::Zero();
	FVector CurClimbableSurfacesAveNormal = FVector::Zero();
	for (const FHitResult& TraceHitResults : ClimbableSurfaceResults)
	{
		CurClimbabllSurfacesAveLoc += TraceHitResults.ImpactPoint;
		CurClimbableSurfacesAveNormal += TraceHitResults.ImpactNormal;
	}
	ProcessedSurfaceResults = CurClimbabllSurfacesAveLoc/ClimbableSurfaceResults.Num();
	ProcessedSurfaceNomal = (CurClimbableSurfacesAveNormal / ClimbableSurfaceResults.Num()).GetSafeNormal();
	return true;
}

bool UWuwaMovementComponent::IsFacingClimbableSurface()
{
	// ---- 1. 筛选 + 平均 ----
	FVector NormalSum = FVector::ZeroVector;
	FVector PointSum = FVector::ZeroVector;
	int32   ValidCount = 0;

	// ImpactNormal.Z = cos(表面倾角)
	//   倾角 0°(地面)  → +1
	//   倾角 90°(垂直墙)→  0
	//   倾角 >90°(外倾) →  负
	const float MaxSlopeNormalZ = FMath::Cos(FMath::DegreesToRadians(MinClimbSurfaceAngle));   // 太缓 → 当地面
	const float MaxOverhangNormalZ = FMath::Cos(FMath::DegreesToRadians(MaxClimbSurfaceAngle));   // 太外倾 → 爬不上

	for (const FHitResult& Hit : ClimbableSurfaceResults)
	{
		if (!Hit.bBlockingHit) continue;

		const FVector& N = Hit.ImpactNormal;
		if (N.Z > MaxSlopeNormalZ)    continue;
		if (N.Z < MaxOverhangNormalZ) continue;   // 注意 MaxOverhangNormalZ 本身是负数

		NormalSum += N;
		PointSum += Hit.ImpactPoint;
		++ValidCount;
	}

	if (ValidCount == 0)
	{
		ProcessedSurfaceNomal = FVector::ZeroVector;
		ProcessedSurfaceResults = FVector::ZeroVector;
		return false;
	}

	const FVector AvgNormal = (NormalSum / ValidCount).GetSafeNormal();
	const FVector AvgLocation = PointSum / ValidCount;

	// ---- 2. 朝向锥(只看水平分量)----
	const FVector FlatNormal = AvgNormal.GetSafeNormal2D();
	const FVector FlatForward = UpdatedComponent->GetForwardVector().GetSafeNormal2D();
	if (FlatNormal.IsNearlyZero() || FlatForward.IsNearlyZero()) return false;

	const float Facing = FVector::DotProduct(-FlatNormal, FlatForward);
	if (Facing < FMath::Cos(FMath::DegreesToRadians(MaxClimbEntryAngle))) return false;

	//// ---- 3. 一致性校验:眼高看到的和胸口撞到的是同一面墙吗 ----
	//if (ClimbableEyesiteResult.bBlockingHit)
	//{
	//	const FVector EyeFlatNormal = ClimbableEyesiteResult.ImpactNormal.GetSafeNormal2D();
	//	if (!EyeFlatNormal.IsNearlyZero())
	//	{
	//		const float Coherence = FVector::DotProduct(EyeFlatNormal, FlatNormal);
	//		if (Coherence < FMath::Cos(FMath::DegreesToRadians(MaxSurfaceDivergenceAngle)))
	//			return false;
	//	}
	//}

	// ---- 4. 缓存给 PhysClimbing 用 ----
	ProcessedSurfaceNomal = AvgNormal;
	ProcessedSurfaceResults = AvgLocation;
	return true;
}

FQuat UWuwaMovementComponent::GetClimbRotation(float Deltatime)
{
	const FQuat CurrentRotation = UpdatedComponent->GetComponentQuat();
	if (HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity())
	{
		return CurrentRotation;
	}
	const FQuat TargetQuat = FRotationMatrix::MakeFromX(-ProcessedSurfaceNomal).ToQuat();
	return FMath::QInterpTo(CurrentRotation, TargetQuat, Deltatime, 5.f);
}

void UWuwaMovementComponent::SnapMovementToClimbableSurface(float Deltatime)
{
	const FVector ComponentForward = UpdatedComponent->GetForwardVector();
	const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();
	const FVector ProjectedCharacterToSurface =
		(ProcessedSurfaceResults - ComponentLocation).ProjectOnTo(ComponentForward);
	const FVector SnapVector = -ProcessedSurfaceNomal * ProjectedCharacterToSurface.Length();
	UpdatedComponent->MoveComponent(
		SnapVector * Deltatime * 500,
		UpdatedComponent->GetComponentQuat(),
		true);
}

bool UWuwaMovementComponent::CheckShouldStopClimbing()
{
	if (ClimbableSurfaceResults.IsEmpty()) return true;
	const float DotResult = FVector::DotProduct(ProcessedSurfaceNomal, FVector::UpVector);
	const float DegreeDiff = FMath::RadiansToDegrees(FMath::Acos(DotResult));
	return DegreeDiff < 60.f;
}

void UWuwaMovementComponent::StopClimbing()
{
	SetMovementMode(MOVE_Falling);
}


void UWuwaMovementComponent::PhysWalking(float deltaTime, int32 Iterations)
{
    // PerformMovement 已经把根运动速度写进 Velocity（行走时保留 Z）；这里先看要不要离地
    // 下落攻击（SetIgnoreAirRootMotion）落地后不再按根运动离地：快速落地时起手段还在淡出，它的上升根运动会把落地段又抬回空中
    if (!bIgnoreAirRootMotion && HasAnimRootMotion() && CharacterOwner && Velocity.Z > FMath::Max(-GetGravityZ() * deltaTime, UE_SMALL_NUMBER))
    {
        SetMovementMode(MOVE_Falling);
        StartNewPhysics(deltaTime, Iterations);
        return;
    }
    Super::PhysWalking(deltaTime, Iterations);
}

void UWuwaMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	if (IsClimbing())
	{
		PhysClimbing(deltaTime, Iterations);
	}
	Super::PhysCustom(deltaTime, Iterations);
}

void UWuwaMovementComponent::PhysClimbing(float deltaTime, int32 Iterations)
{
	//Only Happend On some extreme cases
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	//Calculate hit surfaces and average normals
	ClimbableSurfacesTrace();
	ProcessClimbSurfaces();


	if (CheckShouldStopClimbing())
	{
		StopClimbing();
		return;
	}

	//Incase Rootmotion's Velocity impact real Velocity
	RestorePreAdditiveRootMotionVelocity();
	//计算除了强加的比如MoveTo之外和动画的RootMotion之外本身的移动速度
	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		//calculate true Speed
		CalcVelocity(deltaTime, 0.1, true, MaxClimbBrakingDeceleration);
	}
	//return Rootmotion Speed
	ApplyRootMotionToVelocity(deltaTime);


	//origional position and expected Adjusted Move
	FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FVector Adjusted = Velocity * deltaTime;
	FHitResult Hit(1.f);
	//Calculate Move safely, when hit is not equal to 1 , it means that character hit something
	SafeMoveUpdatedComponent(Adjusted, GetClimbRotation(deltaTime), true, Hit);//

	//cause cha encountered some backforce so we need to fix the move
	if (Hit.Time < 1.f)
	{
		//adjust and try again
		HandleImpact(Hit, deltaTime, Adjusted);
		SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
	}

	//calculate final Velocity On this Frame
	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / deltaTime;
	}

	//Making Cha always face to the center of walls
	SnapMovementToClimbableSurface(deltaTime);
}

#pragma endregion


#pragma region Climb Traces
bool UWuwaMovementComponent::ClimbableSurfacesTrace()
{
	FVector Startoffset = UpdatedComponent->GetForwardVector()* 20.f;
	FVector StartPostion = UpdatedComponent->GetComponentLocation()+ Startoffset;
	FVector EndPosition = StartPostion + UpdatedComponent->GetForwardVector();
	ClimbableSurfaceResults = DoCapsuleMultipleforObjectTrace(StartPostion, EndPosition, false);
	return !ClimbableSurfaceResults.IsEmpty();
}
bool UWuwaMovementComponent::ClimbableEyeSiteTrace()
{
	FVector CharacterForward = UpdatedComponent->GetForwardVector();
	FVector StartPostion = UpdatedComponent->GetComponentLocation()+ UpdatedComponent->GetUpVector()*50.0f;
	FVector EndPosition = StartPostion + ClimbTraceDistance* CharacterForward;
	// 调试线只在 wuwa.Debug.Climb 打开时画（以前固定画，每帧一条）
	ClimbableEyesiteResult = DoEyeSideTrace(StartPostion, EndPosition, UWuwaDebugLibrary::IsDebugChannelEnabled(EWuwaDebugChannel::Climb));
	AActor* HitActor = ClimbableEyesiteResult.GetActor();
	return HitActor ? true : false;
}
TArray<FHitResult> UWuwaMovementComponent::DoCapsuleMultipleforObjectTrace(const FVector& StartPos, const FVector& EndPos, bool bDrawTraceOutSide)
{

	TArray<FHitResult> HitResults;
	UKismetSystemLibrary::CapsuleTraceMultiForObjects(
		this,
		StartPos,
		EndPos,
		ClimbCapsuleTraceRadius,
		ClimbCapsuleTraceHeight,
		ObjectTypes,
		false,
		ActorsToIgnore,
		bDrawTraceOutSide?EDrawDebugTrace::ForOneFrame: EDrawDebugTrace::None,
		HitResults,
		true
	);
	return HitResults;
}


FHitResult UWuwaMovementComponent::DoEyeSideTrace(const FVector& StartPos, const FVector& EndPos, bool bDrawTraceOutSide)
{
	FHitResult HitResult;
	UKismetSystemLibrary::LineTraceSingleForObjects(
		this,
		StartPos,
		EndPos,
		ObjectTypes,
		false,
		ActorsToIgnore,
		bDrawTraceOutSide ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None,
		HitResult,
		true
	);
	return HitResult;
}

#pragma endregion



void UWuwaMovementComponent::SetForceSpeed(const FVector& Speed)
{
    if (Speed.ContainsNaN()) return;
    ForceSpeed = Speed;
    bForceSpeedPending = true;
    Velocity = Speed;
    UpdateComponentVelocity();
}

void UWuwaMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
    Super::HandleImpact(Hit, TimeSlice, MoveDelta);
    BlockActorWhenMove = Hit.GetActor();
    BlockActorFrame = GFrameCounter;
}

AActor* UWuwaMovementComponent::GetBlockActorWhenMove() const
{
    return BlockActorFrame + 1 >= GFrameCounter ? BlockActorWhenMove.Get() : nullptr;
}
