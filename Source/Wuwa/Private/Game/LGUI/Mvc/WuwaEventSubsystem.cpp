#include "Game/LGUI/Mvc/WuwaEventSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UWuwaEventSubsystem* UWuwaEventSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UWuwaEventSubsystem>() : nullptr;
}

FDelegateHandle UWuwaEventSubsystem::Add(FGameplayTag Event, const UObject* Owner, FWuwaEventHandler Handler)
{
	TArray<FListener>& EventListeners = Listeners.FindOrAdd(Event);
	// 顺手清掉 Owner 已经没了的。
	EventListeners.RemoveAll([this](const FListener& Listener) { return !IsAlive(Listener); });

	FListener& Listener = EventListeners.Add_GetRef({FDelegateHandle(FDelegateHandle::GenerateNewHandle), Owner, Owner != nullptr,
		MakeShared<FWuwaEventHandler>(MoveTemp(Handler))});
	return Listener.Handle;
}

void UWuwaEventSubsystem::Remove(FGameplayTag Event, FDelegateHandle Handle)
{
	if (TArray<FListener>* EventListeners = Listeners.Find(Event))
	{
		EventListeners->RemoveAll([Handle](const FListener& Listener) { return Listener.Handle == Handle; });
	}
}

void UWuwaEventSubsystem::RemoveAll(const UObject* Owner)
{
	if (!Owner)
	{
		return;
	}
	for (TPair<FGameplayTag, TArray<FListener>>& Pair : Listeners)
	{
		Pair.Value.RemoveAll([Owner](const FListener& Listener) { return Listener.bHasOwner && Listener.Owner.Get() == Owner; });
	}
}

void UWuwaEventSubsystem::Emit(FGameplayTag Event, const FInstancedStruct& Payload)
{
	const TArray<FListener>* EventListeners = Listeners.Find(Event);
	if (!EventListeners)
	{
		return;
	}

	// 遍历拷贝：回调里增删监听不会影响这一轮。被这一轮里前面的回调移除的，后面不再调用。
	const TArray<FListener> Snapshot = *EventListeners;
	for (const FListener& Listener : Snapshot)
	{
		if (IsAlive(Listener) && IsRegistered(Event, Listener.Handle))
		{
			(*Listener.Handler)(Payload);
		}
	}
}

int32 UWuwaEventSubsystem::NumListeners(FGameplayTag Event) const
{
	const TArray<FListener>* EventListeners = Listeners.Find(Event);
	if (!EventListeners)
	{
		return 0;
	}
	int32 Count = 0;
	for (const FListener& Listener : *EventListeners)
	{
		Count += IsAlive(Listener) ? 1 : 0;
	}
	return Count;
}

bool UWuwaEventSubsystem::IsRegistered(FGameplayTag Event, FDelegateHandle Handle) const
{
	const TArray<FListener>* EventListeners = Listeners.Find(Event);
	return EventListeners && EventListeners->ContainsByPredicate([Handle](const FListener& Listener) { return Listener.Handle == Handle; });
}
