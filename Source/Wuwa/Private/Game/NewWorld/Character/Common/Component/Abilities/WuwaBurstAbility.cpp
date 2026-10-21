#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaBurstAbility.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"

void UWuwaBurstAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// 跳过 UWuwaSkillMontageAbility 的"直接播第 0 段"：原作先处理落地
	UWuwaGameplayAbilityBase::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive()) return;
	AWuwaCharactorBase* Character = Cast<AWuwaCharactorBase>(GetAvatarActorFromActorInfo());
	UWuwaMovementComponent* Movement = Character ? Character->GetWuwaMovementComponent().Get() : nullptr;
	if (Movement) Movement->SetForceSpeed(FVector::ZeroVector);
	if (Character && Movement && Movement->GetUnifiedStateData().PositionState == EWuwaPositionState::Air)
	{
		FVector Ground;
		if (!TraceGround(Character, Character->GetActorLocation(), GroundTraceDistance, Ground))
		{
			EndSkill(true);
			return;
		}
		Character->SetActorLocation(Ground, false, nullptr, ETeleportType::TeleportPhysics);
	}
	if (!PlaySkillMontage(0)) EndSkill(true);
}

bool UWuwaBurstAbility::TraceGround(const ACharacter* Character, const FVector& Start, float Distance, FVector& OutLocation)
{
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!World || !Capsule) return false;
	// 原作 traceGroundWithGravity：单条直线、忽略自己，查静态物体
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WuwaBurstTraceGround), false, Character);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FHitResult Hit;
	if (!World->LineTraceSingleByObjectType(Hit, Start, Start - FVector(0.0, 0.0, Distance), Objects, Params) || !Hit.bBlockingHit) return false;
	OutLocation = Hit.Location + FVector(0.0, 0.0, Capsule->GetScaledCapsuleHalfHeight());
	return true;
}
