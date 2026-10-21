#include "Game/Animation/Notifies/WuwaAnimNotifyState_KeepAwayFromGround.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaSkillMotion.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaKeepAwayFromGround, Log, All);

void UWuwaAnimNotifyState_KeepAwayFromGround::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	const ACharacter* Character = MeshComp ? Cast<ACharacter>(MeshComp->GetOwner()) : nullptr;
	if (!Character) return;
	if (MaxHeight > 0.f && MaxHeight < MinHeight)
	{
		UE_LOG(LogWuwaKeepAwayFromGround, Error, TEXT("%s: MaxHeight %.0f < MinHeight %.0f"), *GetNameSafe(Animation), MaxHeight, MinHeight);
		return;
	}
	if (MaxSpeed <= 0.f) return;
	FWindow& Window = Windows.FindOrAdd(MeshComp);
	Window.NowTime = 0.f;
	Window.TotalTime = TotalDuration;
	Window.LastZ = Character->GetActorLocation().Z;
}

void UWuwaAnimNotifyState_KeepAwayFromGround::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);
	if (FrameDeltaTime < UE_KINDA_SMALL_NUMBER) return;
	ACharacter* Character = MeshComp ? Cast<ACharacter>(MeshComp->GetOwner()) : nullptr;
	FWindow* Window = Windows.Find(MeshComp);
	if (!Character || !Window) return;

	// 原作 MoveToTarget：量高度的距离 = 有上限用上限，否则用下限
	const float Height = GetHeightAboveGround(Character, MaxHeight <= 0.f ? MinHeight : MaxHeight);
	const double CurrentZ = Character->GetActorLocation().Z;
	FVector Offset = FVector::ZeroVector;
	if (Height < MinHeight)
	{
		const float Rate = GetRate(*Window, FrameDeltaTime);
		if (Rate > 0.f) Offset.Z = FMath::Min((MinHeight - Height) * Rate, MaxSpeed * FrameDeltaTime);
	}
	else if (MaxHeight > 0.f && Height >= MaxHeight && CurrentZ > Window->LastZ)
	{
		Offset.Z = Window->LastZ - CurrentZ;
	}
	if (!Offset.IsNearlyZero())
	{
		Character->AddActorWorldOffset(Offset, true);
	}
	Window->NowTime += FrameDeltaTime;
	Window->LastZ = Character->GetActorLocation().Z;
}

void UWuwaAnimNotifyState_KeepAwayFromGround::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Windows.Remove(MeshComp);
	for (auto It = Windows.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

float UWuwaAnimNotifyState_KeepAwayFromGround::GetRate(const FWindow& Window, float DeltaTime) const
{
	// 原作 GetRate：曲线优先；没有曲线时默认三次曲线，勾了不使用默认插值就是 1
	if (MoveCurve || !bIgnoreInterpolation) return WuwaSkillMotion::GetRate(MoveCurve, Window.NowTime, Window.TotalTime, DeltaTime);
	return 1.f;
}

float UWuwaAnimNotifyState_KeepAwayFromGround::GetHeightAboveGround(const ACharacter* Character, float Distance)
{
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Capsule || !World || Distance <= 0.f) return Distance;
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const FVector Feet = Character->GetActorLocation() - FVector(0.0, 0.0, Capsule->GetScaledCapsuleHalfHeight());
	const FVector Start = Feet + FVector(0.0, 0.0, Radius);
	const FVector End = Start - FVector(0.0, 0.0, Distance);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WuwaKeepAwayFromGround), false, Character);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FHitResult Hit;
	if (World->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity, Objects, FCollisionShape::MakeSphere(Radius), Params) && Hit.bBlockingHit)
	{
		return Hit.Time * Distance;
	}
	return Distance;
}

FString UWuwaAnimNotifyState_KeepAwayFromGround::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Keep Away From Ground %.0f~%.0f"), MinHeight, MaxHeight);
}
