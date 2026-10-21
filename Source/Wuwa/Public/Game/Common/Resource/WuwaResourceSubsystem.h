#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WuwaResourceSubsystem.generated.h"

/**
 * 资源加载的统一入口（≈ 鸣潮 Core/Resource/ResourceSystem：LoadAsync / Load / CancelAsyncLoad，全项目的资源都从这里走）。
 *
 * 为什么要统一：
 * - 一处能看到"现在在加载什么、加载了多久"（日志 LogWuwaResource），找卡顿不用到处翻；
 * - 同步加载会卡住游戏线程（角色、特效这类大资源能卡好几秒），这里统一打警告，方便发现该改成异步的地方；
 * - 加载界面的进度来自这里返回的句柄（FStreamableHandle::GetProgress，按已加载的包数算）。
 *
 * 用法：
 *   auto Handle = Resources->LoadAsync({ Path1, Path2 }, TEXT("Formation"), [this]() { ... });
 *   Handle->GetProgress();   // 0~1，给进度条
 *   Handle->CancelHandle();  // 不要了
 * 回调用 WeakLambda / 自己判空：请求方可能在加载完成前就被销毁。
 */
UCLASS()
class WUWA_API UWuwaResourceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UWuwaResourceSubsystem* Get(const UObject* WorldContext);

	/**
	 * 异步加载一组资源（连带它们硬引用的全部资源）。全部已经在内存里时回调会立刻执行，返回的句柄也已完成。
	 * @param DebugName 日志里的名字，比如 "Formation"、"UI:MainHUD"
	 * @param Priority  越大越先加载；加载界面期间要显示的东西用 HighPriority
	 */
	TSharedPtr<FStreamableHandle> LoadAsync(const TArray<FSoftObjectPath>& Paths, const FString& DebugName, TFunction<void()> OnLoaded,
		TAsyncLoadPriority Priority = FStreamableManager::DefaultAsyncLoadPriority);

	/** 同步加载：会卡住游戏线程，只用于小资源（配置、脚本类）。超过阈值会打警告。 */
	UObject* LoadSync(const FSoftObjectPath& Path, const FString& DebugName);

	template <typename T>
	T* LoadSync(const TSoftObjectPtr<T>& Asset, const FString& DebugName) { return Cast<T>(LoadSync(Asset.ToSoftObjectPath(), DebugName)); }
	template <typename T>
	UClass* LoadSync(const TSoftClassPtr<T>& Class, const FString& DebugName) { return Cast<UClass>(LoadSync(Class.ToSoftObjectPath(), DebugName)); }

	static constexpr TAsyncLoadPriority HighPriority = 100;

	/** 同步加载超过这个时间（秒）就打警告。 */
	float SlowSyncLoadWarningSeconds = 0.05f;

private:
	FStreamableManager& GetStreamableManager() const;
};
