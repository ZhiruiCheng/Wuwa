#include "Game/NewWorld/Combat/WuwaTimeStopSubsystem.h"
#include "Game/NewWorld/Combat/WuwaHitComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"

UWuwaTimeStopSubsystem* UWuwaTimeStopSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWuwaTimeStopSubsystem>() : nullptr;
}

bool UWuwaTimeStopSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UWuwaTimeStopSubsystem::Deinitialize()
{
	Locks.Reset();
	Refresh();
	Super::Deinitialize();
}

bool UWuwaTimeStopSubsystem::BeginAbsoluteTimeStop(AActor* Caster, bool bStopMove)
{
	if (!IsValid(Caster) || Locks.Contains(Caster)) return false;
	Locks.Add(Caster, bStopMove);
	Refresh();
	return true;
}

void UWuwaTimeStopSubsystem::EndAbsoluteTimeStop(AActor* Caster)
{
	if (Locks.Remove(Caster) > 0) Refresh();
}

bool UWuwaTimeStopSubsystem::IsFrozen(const AActor* Actor) const
{
	return Actor && Frozen.Contains(Actor);
}

bool UWuwaTimeStopSubsystem::IsMovementFrozen(const AActor* Actor) const
{
	const FFrozen* State = Actor ? Frozen.Find(Actor) : nullptr;
	return State && State->bStopMove;
}

void UWuwaTimeStopSubsystem::Refresh()
{
	for (auto It = Locks.CreateIterator(); It; ++It)
		if (!It.Key().IsValid()) It.RemoveCurrent();
	if (Locks.IsEmpty())
	{
		// 原作 Yhh：全部恢复
		TArray<TWeakObjectPtr<AActor>> All;
		Frozen.GetKeys(All);
		for (const TWeakObjectPtr<AActor>& Actor : All) Unfreeze(Actor.Get());
		Frozen.Reset();
		return;
	}
	// 原作 Kgp：所有锁都要求冻结移动才冻结移动
	bool bStopMove = true;
	for (const TPair<TWeakObjectPtr<AActor>, bool>& Lock : Locks) bStopMove &= Lock.Value;
	const FVector Center = Locks.CreateConstIterator().Key()->GetActorLocation();
	UWorld* World = GetWorld();
	if (!World) return;
	for (TActorIterator<AWuwaCharactorBase> It(World); It; ++It)
	{
		AWuwaCharactorBase* Character = *It;
		if (Locks.Contains(Character))
		{
			Unfreeze(Character);
			continue;
		}
		if (FVector::DistSquared(Character->GetActorLocation(), Center) > Range * Range) continue;
		Freeze(Character, bStopMove);
	}
}

void UWuwaTimeStopSubsystem::Freeze(AActor* Actor, bool bStopMove)
{
	if (!IsValid(Actor)) return;
	if (const FFrozen* Existing = Frozen.Find(Actor); Existing && Existing->bStopMove == bStopMove) return;
	Unfreeze(Actor);
	FFrozen& State = Frozen.Add(Actor);
	State.bStopMove = bStopMove;
	State.SavedDilation = Actor->CustomTimeDilation;
	if (bStopMove)
	{
		// 动画、移动、Tick 一起停（有受击组件的由它统一算：顿帧 × 时停）
		Actor->CustomTimeDilation = 0.f;
		if (const AWuwaCharactorBase* Character = Cast<AWuwaCharactorBase>(Actor); Character && Character->HitComponent) Character->HitComponent->RefreshTimeScale();
	}
	else if (const ACharacter* Character = Cast<ACharacter>(Actor); Character && Character->GetMesh())
	{
		Character->GetMesh()->bPauseAnims = true;
	}
}

void UWuwaTimeStopSubsystem::Unfreeze(AActor* Actor)
{
	FFrozen State;
	if (!Frozen.RemoveAndCopyValue(Actor, State) || !IsValid(Actor)) return;
	if (State.bStopMove)
	{
		// 有受击组件的按它当前的顿帧重算；没有的恢复冻结前的值
		if (const AWuwaCharactorBase* Character = Cast<AWuwaCharactorBase>(Actor); Character && Character->HitComponent) Character->HitComponent->RefreshTimeScale();
		else Actor->CustomTimeDilation = State.SavedDilation;
	}
	else if (const ACharacter* Character = Cast<ACharacter>(Actor); Character && Character->GetMesh())
	{
		Character->GetMesh()->bPauseAnims = false;
	}
}
