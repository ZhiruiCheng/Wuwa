#include "Game/Common/Resource/WuwaResourceSubsystem.h"

#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaResource, Log, All);

UWuwaResourceSubsystem* UWuwaResourceSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UWuwaResourceSubsystem>() : nullptr;
}

FStreamableManager& UWuwaResourceSubsystem::GetStreamableManager() const
{
	// 用 AssetManager 的 StreamableManager：和引擎、插件共用一套加载队列和已加载资源的引用计数。
	return UAssetManager::GetStreamableManager();
}

TSharedPtr<FStreamableHandle> UWuwaResourceSubsystem::LoadAsync(const TArray<FSoftObjectPath>& Paths, const FString& DebugName,
	TFunction<void()> OnLoaded, TAsyncLoadPriority Priority)
{
	//添加有效路径
	TArray<FSoftObjectPath> Valid;
	for (const FSoftObjectPath& Path : Paths)
	{
		if (Path.IsValid())
		{
			Valid.AddUnique(Path);
		}
	}
	
	
	if (Valid.IsEmpty())
	{
		UE_LOG(LogWuwaResource, Warning, TEXT("LoadAsync[%s]: nothing to load."), *DebugName);
		if (OnLoaded)
		{
			OnLoaded();
		}
		return nullptr;
	}

	const double Start = FPlatformTime::Seconds();
	UE_LOG(LogWuwaResource, Log, TEXT("LoadAsync[%s]: %d assets, priority %d"), *DebugName, Valid.Num(), Priority);
	return GetStreamableManager().RequestAsyncLoad(Valid, FStreamableDelegate::CreateWeakLambda(this,
		[DebugName, Start, OnLoaded = MoveTemp(OnLoaded)]()
		{
			UE_LOG(LogWuwaResource, Log, TEXT("LoadAsync[%s]: done in %.2fs"), *DebugName, FPlatformTime::Seconds() - Start);
			if (OnLoaded)
			{
				OnLoaded();
			}
		}), Priority, false, false, DebugName);
}

UObject* UWuwaResourceSubsystem::LoadSync(const FSoftObjectPath& Path, const FString& DebugName)
{
	if (!Path.IsValid())
	{
		return nullptr;
	}
	
	//如果已经在内存里面就直接拿来用
	if (UObject* Loaded = Path.ResolveObject())
	{
		return Loaded;
	}
	const double Start = FPlatformTime::Seconds();
	UObject* Loaded = GetStreamableManager().LoadSynchronous(Path, false);
	const double Seconds = FPlatformTime::Seconds() - Start;
	if (Seconds > SlowSyncLoadWarningSeconds)
	{
		UE_LOG(LogWuwaResource, Warning, TEXT("LoadSync[%s]: %s took %.2fs on the game thread; consider LoadAsync."), *DebugName, *Path.ToString(), Seconds);
	}
	return Loaded;
}
