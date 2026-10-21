#include "Game/LGUI/Mvc/WuwaModelBase.h"

#include "Engine/World.h"

void UWuwaModelBase::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &UWuwaModelBase::HandleWorldCleanup);
	OnInit();
}

void UWuwaModelBase::Deinitialize()
{
	OnClear();
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	Super::Deinitialize();
}

void UWuwaModelBase::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (World && World == GetWorld())
	{
		OnLeaveLevel();
	}
}
