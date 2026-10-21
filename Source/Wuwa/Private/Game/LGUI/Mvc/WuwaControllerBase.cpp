#include "Game/LGUI/Mvc/WuwaControllerBase.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Game/LGUI/Core/WuwaUiManager.h"
#include "Subsystems/SubsystemCollection.h"

void UWuwaControllerBase::Initialize(FSubsystemCollectionBase& Collection)
{
	// 打开前检查要注册到 UiManager 上，先保证它已经初始化。
	Collection.InitializeDependency<UWuwaUiManager>();
	Super::Initialize(Collection);

	//世界清理事件,当世界清理时调用HandleWorldCleanup函数
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &UWuwaControllerBase::HandleWorldCleanup);
	OnInit();
	OnAddEvents();
	OnAddOpenViewCheckFunction();
}

//当这个system被清理的时候会发射管的事情
void UWuwaControllerBase::Deinitialize()
{
	OnRemoveOpenViewCheckFunction();
	OnRemoveEvents();

	// 兜底：通过基类注册的都在这里移除。
	if (UWuwaUiManager* UiManager = GetUiManager())
	{
		for (const TPair<FName, FDelegateHandle>& Check : OpenViewChecks)
		{
			UiManager->RemoveOpenViewCheck(Check.Key, Check.Value);
		}
	}
	OpenViewChecks.Reset();
	if (UWuwaEventSubsystem* Events = GetEvents())
	{
		Events->RemoveAll(this);
	}

	OnClear();
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	Super::Deinitialize();
}

UWuwaUiManager* UWuwaControllerBase::GetUiManager() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetSubsystem<UWuwaUiManager>() : nullptr;
}

UWuwaEventSubsystem* UWuwaControllerBase::GetEvents() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const UGameInstance* GameInstance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UWuwaEventSubsystem>() : nullptr;
}

FDelegateHandle UWuwaControllerBase::ListenEvent(FGameplayTag Event, FWuwaEventHandler Handler)
{
	UWuwaEventSubsystem* Events = GetEvents();
	return Events ? Events->Add(Event, this, MoveTemp(Handler)) : FDelegateHandle();
}

void UWuwaControllerBase::AddOpenViewCheck(FName ViewName, TFunction<bool(FName)> Check)
{
	if (UWuwaUiManager* UiManager = GetUiManager())
	{
		OpenViewChecks.Emplace(ViewName, UiManager->AddOpenViewCheck(ViewName, MoveTemp(Check)));
	}
}

void UWuwaControllerBase::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (World && World == GetWorld())
	{
		OnLeaveLevel();
	}
}
