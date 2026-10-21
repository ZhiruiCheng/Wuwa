// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Combat/WuwaLockOnComponent.h"
#include "Engine/DataTable.h"
#include "AbilitySystemComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/NewWorld/Buff/WuwaBuffLibrary.h"

bool UWuwaGameplayAbilityBase::IsSkillExecutionActive() const
{
	return IsInstantiated() && IsActive() && !bIsAbilityEnding;
}

bool UWuwaGameplayAbilityBase::IsSkillExecutionFor(AActor* ExpectedAvatar) const
{
	return IsValid(ExpectedAvatar) && IsSkillExecutionActive() && CurrentActorInfo
		&& CurrentActorInfo->AvatarActor.Get() == ExpectedAvatar;
}

bool UWuwaGameplayAbilityBase::CanEndSkillExecutionNow() const
{
	return IsSkillExecutionActive() && ScopeLockCount == 0;
}

bool UWuwaGameplayAbilityBase::TryEndSkillExecution(int32 ExpectedHandle)
{
	if (ExpectedHandle <= 0 || FightStateHandle != ExpectedHandle || !CanEndSkillExecutionNow()) return false;
	// 与原作 K2_EndAbility 的让位语义一致，不依赖 CancelAbilitiesWithTag 或 CanBeCanceled。
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	return FightStateHandle != ExpectedHandle;
}

FWuwaPlayerInputState UWuwaGameplayAbilityBase::GetPlayerInputState() const
{
	// 不缓存角色指针，也不使用 GetPlayerCharacter(0)：始终读取这个 GA 自己的 Avatar。
	const AWuwaCharacter* Character = CurrentActorInfo ? Cast<AWuwaCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	return IsValid(Character) ? Character->GetPlayerInputState() : FWuwaPlayerInputState{};
}

//停止当前正在执行的蒙太奇动画并先停止根运动防止在混出的时候占据CMC的移动计算
bool UWuwaGameplayAbilityBase::StopMontageForMovement(float BlendOutTime)
{
	if (!CurrentActorInfo || !FMath::IsFinite(BlendOutTime) || BlendOutTime < 0.f|| !GetPlayerInputState().bHasMoveInput){return false;}

	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	UAnimInstance* AnimInstance = CurrentActorInfo->GetAnimInstance();
	UAnimMontage* Montage = GetCurrentMontage();
	if (!IsValid(ASC) || !IsValid(AnimInstance) || !IsValid(Montage)|| ASC->GetAnimatingAbility() != this || ASC->GetCurrentMontage() != Montage){return false;}

	FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(Montage);
	if (!Instance){return false;}

	// 必须在 Stop 前取得实例并禁用：Everything 模式混出末帧的零权重根运动
	// 仍可能覆盖 CMC 的速度。只停止这次播放的提取，不清空其他动画的根运动。
	Instance->PushDisableRootMotion();
	// 此实例已确定退出，禁用保持到实例销毁；GA End 不应提前 Pop 恢复提取。
	// 经由 ASC 停止以保留 GAS 的播放归属及停止同步流程。
	// Stop 可同步触发回调并更换播放实例，因此之后不再访问 Instance。
	ASC->CurrentMontageStop(BlendOutTime);
	return true;
}


int32 UWuwaGameplayAbilityBase::GetSkillId() const
{
	const UWuwaAbilitySystemComponent* ASC = Cast<UWuwaAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
	return ASC ? ASC->GetSkillIdForHandle(GetCurrentAbilitySpecHandle()) : 0;
}

//检查是否能够激活
bool UWuwaGameplayAbilityBase::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const AWuwaCharacter* Character = ActorInfo ? Cast<AWuwaCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UWuwaSkillBridgeComponent* Skills = IsValid(Character) ? Character->SkillComponent.Get() : nullptr;
	// 只有主技能需要经过技能组件的让位判断；非主技能不查询（括号不能省：&& 优先于 ||）。
	if (bIsMainSkill && (!IsValid(Skills) || !Skills->CanBeginSkill(const_cast<UWuwaGameplayAbilityBase*>(this))))
	{
		return false;
	}

	if (WritesStartMoveState())
	{
		// CDO 查询使用本次传入的 Avatar，不依赖实例 CurrentActorInfo。
		// 只检查要写入的动作状态在当前位置是否合法（原作 legalMoveStates），不占用、不比较优先级。
		const UWuwaUnifiedStateBridgeComponent* State = IsValid(Character) ? Character->UnifiedStateComponent.Get() : nullptr;
		if (!IsValid(State) || !State->IsMoveStateLegal(State->GetStateData().PositionState, StartMoveState))
		{
			return false;
		}
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UWuwaGameplayAbilityBase::PreActivate(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate, const FGameplayEventData* TriggerEventData)
{
	//只有使用策略为NonInstanced才会返回false其他的正常策略都是true
	if (!IsInstantiated())
	{
		Super::PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate, TriggerEventData);
		return;
	}

	//
	const uint64 Serial = ++ActivationSerial;
	Super::PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate, TriggerEventData);
	// Super 建立 GAS 上下文/活动计数；其 Tag 和激活通知可以重入并结束本次能力。
	if (Serial != ActivationSerial || !IsActive() || bIsAbilityEnding)
	{
		return;
	}

	const AWuwaCharacter* Character = ActorInfo ? Cast<AWuwaCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (bIsMainSkill)
	{
		UWuwaSkillBridgeComponent* Skills = IsValid(Character) ? Character->SkillComponent.Get() : nullptr;
		if (!IsValid(Skills)) return;
		const TWeakObjectPtr<UWuwaSkillBridgeComponent> RequestedSkills(Skills);
		const int32 NewSkillHandle = Skills->TryBeginSkill(this);
		if (Serial != ActivationSerial || !IsSkillExecutionActive())
		{
			if (NewSkillHandle != 0 && RequestedSkills.IsValid()) RequestedSkills->EndSkill(NewSkillHandle);
			return;
		}
		SkillOwner = RequestedSkills;
		FightStateHandle = NewSkillHandle;
		if (NewSkillHandle == 0) return;
	}

	if (!WritesStartMoveState())
	{
		return;
	}
	UWuwaUnifiedStateBridgeComponent* State = IsValid(Character) ? Character->UnifiedStateComponent.Get() : nullptr;
	if (!IsValid(State))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Action state requested without an initialized UnifiedState component."), *GetName());
		return;
	}
	// 与原作的动作技能一样只在开始时写入一次，保留当前步态的速度配置；没有句柄，也不阻止之后的普通写入。
	State->SetMoveState(StartMoveState, State->GetStateData().Gait);
}

void UWuwaGameplayAbilityBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,const FGameplayEventData* TriggerEventData)
{
	// CallActivateAbility 总会在 PreActivate 之后调用 ActivateAbility。
	// 只在这里拦截本次主技能登记失败，避免已拒绝的技能仍进入蓝图播放蒙太奇。
	// 动作状态只在实例化的 PreActivate 中写入，非实例化的 GA 不能配置它。
	if ((bIsMainSkill && (!IsInstantiated() || FightStateHandle == 0 || !SkillOwner.IsValid() || !IsActive()))
		|| (WritesStartMoveState() && !IsInstantiated()))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 原作 BaseSkillComponent.BeginSkill：先选技能目标（QZr），再开始技能、按 SkillDirection 转向（DoSkillBeginMoveAction）；
	// 放在蓝图 ActivateAbility 播蒙太奇之前
	BeginSkillTarget();
	BeginSkillActionTags();
	ApplySkillStartBuffs();
	BeginSkillWalkOffLedge();
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UWuwaGameplayAbilityBase::BeginSkillWalkOffLedge()
{
	AWuwaCharacter* Character = CurrentActorInfo ? Cast<AWuwaCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	const int32 SkillId = GetSkillId();
	const FWuwaSkillInfo* Info = IsValid(Character) && Character->SkillTable && SkillId != 0
		? Character->SkillTable->FindRow<FWuwaSkillInfo>(FName(*FString::FromInt(SkillId)), TEXT("WalkOffLedge"), false) : nullptr;
	UWuwaMovementComponent* Movement = IsValid(Character) ? Character->GetWuwaMovementComponent() : nullptr;
	if (!Info || !Info->bWalkOffLedge || !Movement || WalkOffLedgeMovement.IsValid()) return;
	Movement->SetWalkOffLedgeRecord(false);
	WalkOffLedgeMovement = Movement;
}

void UWuwaGameplayAbilityBase::EndSkillWalkOffLedge()
{
	if (UWuwaMovementComponent* Movement = WalkOffLedgeMovement.Get()) Movement->SetWalkOffLedgeRecord(true);
	WalkOffLedgeMovement.Reset();
}

void UWuwaGameplayAbilityBase::BeginSkillActionTags()
{
	// 原作 Skill.BeginSkillBuffAndTag：技能开始把技能表 SkillTag 加到自己身上（项目只加 ActionTags）
	AWuwaCharacter* Character = CurrentActorInfo ? Cast<AWuwaCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	UAbilitySystemComponent* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	const int32 SkillId = GetSkillId();
	const FWuwaSkillInfo* Info = IsValid(Character) && ASC && Character->SkillTable && SkillId != 0
		? Character->SkillTable->FindRow<FWuwaSkillInfo>(FName(*FString::FromInt(SkillId)), TEXT("ActionTags"), false) : nullptr;
	if (!Info || Info->ActionTags.IsEmpty() || ActionTagsASC.IsValid()) return;
	ASC->AddLooseGameplayTags(Info->ActionTags);
	ActionTagsASC = ASC;
	AppliedActionTags = Info->ActionTags;
}

void UWuwaGameplayAbilityBase::EndSkillActionTags()
{
	if (UAbilitySystemComponent* ASC = ActionTagsASC.Get()) ASC->RemoveLooseGameplayTags(AppliedActionTags);
	ActionTagsASC.Reset();
	AppliedActionTags.Reset();
}

void UWuwaGameplayAbilityBase::ApplySkillStartBuffs()
{
	// 原作 BaseSkillComponent.BeginSkill 里的 SkillStartBuff：技能开始时给自己加 buff（只在有权威的一端加）
	AWuwaCharacter* Character = CurrentActorInfo ? Cast<AWuwaCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	UAbilitySystemComponent* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	const int32 SkillId = GetSkillId();
	const FWuwaSkillInfo* Info = IsValid(Character) && ASC && Character->SkillTable && SkillId != 0
		? Character->SkillTable->FindRow<FWuwaSkillInfo>(FName(*FString::FromInt(SkillId)), TEXT("SkillStartBuffs"), false) : nullptr;
	if (!Info) return;
	for (const int64 BuffId : Info->SkillStartBuffs) UWuwaBuffLibrary::AddBuff(ASC, BuffId);
}

void UWuwaGameplayAbilityBase::ApplySkillEndBuffs()
{
	// 原作 Skill.EndSkill：移除技能 Tag 之后加 SkillEndBuff（不区分正常结束 / 被打断；只在有权威的一端加）
	AWuwaCharacter* Character = CurrentActorInfo ? Cast<AWuwaCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	UAbilitySystemComponent* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	const int32 SkillId = GetSkillId();
	const FWuwaSkillInfo* Info = IsValid(Character) && ASC && Character->SkillTable && SkillId != 0
		? Character->SkillTable->FindRow<FWuwaSkillInfo>(FName(*FString::FromInt(SkillId)), TEXT("SkillEndBuffs"), false) : nullptr;
	if (!Info) return;
	for (const int64 BuffId : Info->SkillEndBuffs) UWuwaBuffLibrary::AddBuff(ASC, BuffId);
}

void UWuwaGameplayAbilityBase::BeginSkillTarget()
{
	AWuwaCharacter* Character = CurrentActorInfo ? Cast<AWuwaCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	const int32 SkillId = GetSkillId();
	const FWuwaSkillInfo* Info = IsValid(Character) && Character->SkillTable && SkillId != 0
		? Character->SkillTable->FindRow<FWuwaSkillInfo>(FName(*FString::FromInt(SkillId)), TEXT("BeginSkillTarget"), false) : nullptr;
	UWuwaLockOnComponent* LockOn = IsValid(Character) ? Character->LockOnComponent.Get() : nullptr;
	if (!Info || !LockOn) return;

	// 选目标：每次技能开始先清空；配置了锁定才挑
	AActor* Target = nullptr;
	FName Socket;
	if (Info->bIsLockOn && LockOn->DetectSoftLockTarget(Info->LockOnConfigId, Info->SkillTargetPriority))
	{
		Target = LockOn->GetCurrentTarget().Actor.Get();
		Socket = LockOn->GetCurrentTarget().Socket;
	}
	if (UWuwaSkillBridgeComponent* Skills = Character->SkillComponent.Get()) Skills->SetSkillTarget(Target, Socket);

	// 转向（原作 CharacterSkillComponent.SetSkillTargetDirection）：瞬间转到目标 / 输入 / 镜头方向，只转水平朝向
	const FWuwaPlayerInputState Input = Character->GetPlayerInputState();
	const FVector InputDirection = Input.bHasMoveInput ? Input.MoveWorldDirection : FVector::ZeroVector;
	const AController* Controller = Character->GetController();
	const FVector CameraDirection = Controller ? Controller->GetControlRotation().Vector() : FVector::ZeroVector;
	FVector Facing = FVector::ZeroVector;
	switch (Info->SkillDirection)
	{
	case EWuwaSkillDirection::SkillTarget:
	case EWuwaSkillDirection::VehicleSkillTarget:
		Facing = Target ? Target->GetActorLocation() - Character->GetActorLocation()
			: (Info->SkillTargetPriority == EWuwaSkillTargetPriority::CameraDirectionPrecise ? CameraDirection : InputDirection);
		break;
	case EWuwaSkillDirection::InputDirection: Facing = InputDirection; break;
	case EWuwaSkillDirection::CameraDirection: Facing = CameraDirection; break;
	default: break;   // 角色方向：不转
	}
	Facing.Z = 0.0;
	if (!Facing.IsNearlyZero()) Character->SetActorRotation(FRotator(0.0, Facing.Rotation().Yaw, 0.0));
}

void UWuwaGameplayAbilityBase::EndAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsInstantiated() || (FightStateHandle == 0 && !WritesStartMoveState()))
	{
		if (IsInstantiated())
		{
			EndSkillWalkOffLedge();
			EndSkillActionTags();
		}
		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
		return;
	}
	if (!IsEndAbilityValid(Handle, ActorInfo))
	{
		return;
	}
	if (ScopeLockCount > 0)
	{
		// 保持 GAS 的延迟结束语义；真正退出时才归还技能句柄并重算移动状态。
		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
		return;
	}

	// 原作 DoSkillEndMoveAction：归还边缘保护；原作 EndSkill 移除技能 Tag，再加 SkillEndBuff。
	EndSkillWalkOffLedge();
	EndSkillActionTags();
	ApplySkillEndBuffs();
	const TWeakObjectPtr<UWuwaSkillBridgeComponent> EndingSkills = SkillOwner;
	const int32 EndingSkillHandle = FightStateHandle;
	SkillOwner.Reset();
	FightStateHandle = 0;
	// 写过动作状态的 GA 结束后让 RoleGait 立即重算一次，不等下一次 CMC Tick。
	const AWuwaCharacter* Character = ActorInfo ? Cast<AWuwaCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const TWeakObjectPtr<UWuwaRoleGaitBridgeComponent> EndingGait = WritesStartMoveState() && IsValid(Character)
		? Character->RoleGaitComponent.Get() : nullptr;

	const TWeakObjectPtr<UAbilitySystemComponent> ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const TWeakObjectPtr<UAnimInstance> AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	const TWeakObjectPtr<UAnimMontage> EndingMontage = GetCurrentMontage();
	int32 EndingMontageInstanceId = INDEX_NONE;
	if (ASC.IsValid() && AnimInstance.IsValid() && EndingMontage.IsValid()
		&& ASC->GetAnimatingAbility() == this && ASC->GetCurrentMontage() == EndingMontage.Get())
	{
		if (const FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(EndingMontage.Get()))
		{
			EndingMontageInstanceId = Instance->GetInstanceID();
		}
	}

	// PlayMontageAndWait 等任务在 Super 的清理中请求停止，Notify/Ended 可同步重入。
	// 原句柄已移出成员，回调中新激活的技能只会保存/归还它自己的句柄。
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	// 若未来某个显式配置的动作没有 StopWhenAbilityEnds，补停仍属本次的播放。
	// 新能力/同资产的新播放实例均不能被旧能力的结束逻辑停止。
	if (EndingMontageInstanceId != INDEX_NONE && ASC.IsValid() && AnimInstance.IsValid() && EndingMontage.IsValid()
		&& ASC->GetAnimatingAbility() == nullptr && ASC->GetCurrentMontage() == EndingMontage.Get())
	{
		const FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(EndingMontage.Get());
		if (Instance && Instance->GetInstanceID() == EndingMontageInstanceId)
		{
			ASC->CurrentMontageStop();
		}
	}

	// 这里只保证停止请求/同步清理先于状态交接；Queued NotifyEnd 仍可能稍后执行。
	// 不撤销 StopMontageForMovement 对旧实例的根运动禁用，也不改写角色速度。
	if (EndingSkills.IsValid())
	{
		EndingSkills->EndSkill(EndingSkillHandle);
	}
	if (EndingGait.IsValid())
	{
		EndingGait->RefreshPolicy();
	}
}
