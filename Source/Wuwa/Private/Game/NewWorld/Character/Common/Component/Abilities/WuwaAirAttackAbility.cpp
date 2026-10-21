#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAirAttackAbility.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"

void UWuwaAirAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	bLandingAttack = false;
	bIgnoringPawns = false;
	bLeftAirChecked = false;
	BlockTime = 0.f;
	// 跳过 UWuwaSkillMontageAbility 的"直接播第 0 段"，先做原作 GA_ChangLi_AirAttack 的地面判断
	UWuwaGameplayAbilityBase::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive()) return;
	if (bEndIfActivatedOnGround && IsOnGround())
	{
		EndSkill(false);
		return;
	}
	if (!PlaySkillMontage(0)) EndSkill(true);
}

void UWuwaAirAttackAbility::OnSkillMontageCompleted(int32 Index)
{
	if (Index == 0) PlayFallLoop();
	else EndSkill(false);
}

void UWuwaAirAttackAbility::OnSkillMontageBlendOut(int32 Index)
{
	if (Index == 0) PlayFallLoop();
	else if (Index == 2 || !bLandingAttack) EndSkill(false);
}

void UWuwaAirAttackAbility::OnSkillMontageInterrupted(int32 Index)
{
	if (Index == 1 && bLandingAttack) return;
	EndSkill(true);
}

void UWuwaAirAttackAbility::OnSkillMontageCancelled(int32 Index)
{
	if (Index == 1 && bLandingAttack) return;
	EndSkill(true);
}

void UWuwaAirAttackAbility::OnSkillMontageTick(int32 Index, float DeltaTime)
{
	if (Index != 1 || bLandingAttack) return;
	if (IsInPlainAirState())
	{
		// 原作 Kuro_GetBlockActorWhenMove 是 StaticMeshActor 就累计，否则清零
		const UWuwaMovementComponent* Movement = GetWuwaMovement();
		if (Movement && Cast<AStaticMeshActor>(Movement->GetBlockActorWhenMove()))
		{
			BlockTime += DeltaTime;
			if (BlockTime > MaxBlockTime) EndSkill(false);
		}
		else
		{
			BlockTime = 0.f;
		}
		return;
	}
	if (bLeftAirChecked) return;
	bLeftAirChecked = true;
	if (IsOnGround()) PlayLanding();
	else EndSkill(false);
}

void UWuwaAirAttackAbility::PlayFallLoop()
{
	// 起手段在淡出时还带着根运动（下落中项目用动画根运动的完整速度），会把向下冲的速度盖掉；
	// 换成下落循环后就不再要起手段的根运动，让 SetForceSpeed 和重力接手。
	UAnimInstance* Anim = CurrentActorInfo ? CurrentActorInfo->GetAnimInstance() : nullptr;
	const FAnimMontageInstance* StartInstance = Anim && SkillMontages.IsValidIndex(0) && SkillMontages[0]
		? Anim->GetActiveInstanceForMontage(SkillMontages[0]) : nullptr;
	const int32 StartInstanceId = StartInstance ? StartInstance->GetInstanceID() : INDEX_NONE;
	if (!PlaySkillMontage(1, true))
	{
		if (!bLandingAttack) EndSkill(false);
		return;
	}
	if (FAnimMontageInstance* Old = Anim && StartInstanceId != INDEX_NONE ? Anim->GetMontageInstanceForID(StartInstanceId) : nullptr)
	{
		Old->PushDisableRootMotion();
	}
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (UWuwaMovementComponent* Movement = GetWuwaMovement(); Movement && Avatar)
	{
		// ABP 从整个姿势取根运动，上面的 PushDisableRootMotion 挡不住淡出中的起手段：空中直接不让根运动改速度（落地自动恢复）
		Movement->SetIgnoreAirRootMotion(true);
		Movement->SetForceSpeed(Avatar->GetActorUpVector() * -FallForceSpeed);
	}
	// 往下冲时不挡 Pawn：砸在敌人头顶也直接穿下去（原作结束时把 Pawn 通道设回阻挡）
	if (const ACharacter* Character = Cast<ACharacter>(Avatar))
	{
		if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent(); Capsule && Capsule->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block)
		{
			Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			bIgnoringPawns = true;
		}
	}
}

void UWuwaAirAttackAbility::PlayLanding()
{
	bLandingAttack = true;
	if (!PlaySkillMontage(2)) EndSkill(false);
}

bool UWuwaAirAttackAbility::IsInPlainAirState() const
{
	const UWuwaMovementComponent* Movement = GetWuwaMovement();
	if (!Movement) return false;
	const FWuwaUnifiedStateData State = Movement->GetUnifiedStateData();
	return State.PositionState == EWuwaPositionState::Air
		&& (State.MoveState == EWuwaMoveState::Other || State.MoveState == EWuwaMoveState::Jump || State.MoveState == EWuwaMoveState::Fall);
}

bool UWuwaAirAttackAbility::IsOnGround() const
{
	const UWuwaMovementComponent* Movement = GetWuwaMovement();
	return Movement && Movement->GetUnifiedStateData().PositionState == EWuwaPositionState::Ground;
}

UWuwaMovementComponent* UWuwaAirAttackAbility::GetWuwaMovement() const
{
	const AWuwaCharactorBase* Character = Cast<AWuwaCharactorBase>(GetAvatarActorFromActorInfo());
	return Character ? Character->GetWuwaMovementComponent().Get() : nullptr;
}

void UWuwaAirAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 原作 OnEndAbility：SetForceSpeed(0)
	if (IsActive())
	{
		if (UWuwaMovementComponent* Movement = GetWuwaMovement())
		{
			Movement->SetIgnoreAirRootMotion(false);
			Movement->SetForceSpeed(FVector::ZeroVector);
		}
		// 原作 OnEndAbility：SetCollisionResponse(2, 2)（Pawn 通道设回阻挡；落在敌人身体里时由移动组件推出去）
		const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
		if (UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr; Capsule && bIgnoringPawns)
		{
			Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		}
		bIgnoringPawns = false;
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
