#include "Game/NewWorld/Combat/WuwaCreatureSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"

UWuwaCreatureSubsystem* UWuwaCreatureSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UWuwaCreatureSubsystem>() : nullptr;
}

void UWuwaCreatureSubsystem::Register(AWuwaCharactorBase* Creature)
{
	if (!IsValid(Creature) || Creatures.Contains(Creature)) return;
	// 顺手清掉已经失效的项，表不会越积越长
	Creatures.RemoveAll([](const TWeakObjectPtr<AWuwaCharactorBase>& Entry) { return !Entry.IsValid(); });
	Creatures.Add(Creature);
	OnCreatureRegistered.Broadcast(Creature);
}

void UWuwaCreatureSubsystem::Unregister(AWuwaCharactorBase* Creature)
{
	if (!Creature || !Creatures.Contains(Creature)) return;
	OnCreatureUnregistered.Broadcast(Creature);
	Creatures.Remove(Creature);
}

void UWuwaCreatureSubsystem::GetCreaturesInRange(const FVector& Location, float Radius, TArray<AWuwaCharactorBase*>& Out) const
{
	Out.Reset();
	const double RadiusSquared = FMath::Square(FMath::Max(Radius, 0.f));
	TArray<TPair<double, AWuwaCharactorBase*>> Found;
	for (const TWeakObjectPtr<AWuwaCharactorBase>& Entry : Creatures)
	{
		AWuwaCharactorBase* Creature = Entry.Get();
		if (!Creature) continue;
		const double DistanceSquared = FVector::DistSquared(Location, Creature->GetActorLocation());
		if (DistanceSquared <= RadiusSquared) Found.Emplace(DistanceSquared, Creature);
	}
	Found.Sort([](const TPair<double, AWuwaCharactorBase*>& A, const TPair<double, AWuwaCharactorBase*>& B) { return A.Key < B.Key; });
	for (const TPair<double, AWuwaCharactorBase*>& Pair : Found) Out.Add(Pair.Value);
}

bool UWuwaCreatureSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 和子弹系统一样只在游戏 / PIE 世界里有；动画编辑器预览里的角色不登记
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
