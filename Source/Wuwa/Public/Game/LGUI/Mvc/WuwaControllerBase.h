#pragma once

#include "CoreMinimal.h"
#include "Engine/LocalPlayer.h"
#include "GameplayTagContainer.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Game/LGUI/Mvc/WuwaEventSubsystem.h"
#include "WuwaControllerBase.generated.h"

class UWuwaUiManager;

/**
 * Controller 基类（≈ 鸣潮 UiControllerBase）：处理业务（请求、规则），改 Model，发事件，提供打开界面的入口。
 * 是 LocalPlayerSubsystem：UE 自动为每个非抽象子类创建一个实例（≈ ControllerManager）。
 * Initialize 时依次调 OnInit → OnAddEvents → OnAddOpenViewCheckFunction，Deinitialize 时反过来。
 * 用 ListenEvent / AddOpenViewCheck 注册的东西会在 Deinitialize 时自动移除，Remove 钩子只在需要额外清理时重写。
 * 子类用到 Model 时，重写 Initialize，先 Collection.InitializeDependency<模型类>() 再调 Super。
 */
UCLASS(Abstract)
class WUWA_API UWuwaControllerBase : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

protected:
	virtual void OnInit() {}
	virtual void OnClear() {}
	virtual void OnAddEvents() {}
	virtual void OnRemoveEvents() {}
	virtual void OnAddOpenViewCheckFunction() {}
	virtual void OnRemoveOpenViewCheckFunction() {}
	/** 这个玩家所在的世界被清理（切地图、结束 PIE）时调用。 */
	virtual void OnLeaveLevel() {}

	UWuwaUiManager* GetUiManager() const;
	UWuwaEventSubsystem* GetEvents() const;
	template <typename T>
	T* GetModel() const { return GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<T>() : nullptr; }

	/** 监听事件，Owner 是这个 Controller；Deinitialize 时自动移除。 */
	FDelegateHandle ListenEvent(FGameplayTag Event, FWuwaEventHandler Handler);
	/** 发事件的简写。 */
	template <typename T>
	void EmitEvent(FGameplayTag Event, const T& Payload) const
	{
		if (UWuwaEventSubsystem* Events = GetEvents()) { Events->Emit(Event, Payload); }
	}
	/** 注册打开前检查；Deinitialize 时自动移除（鸣潮要手动对应着删，删错名字就漏掉了）。 */
	void AddOpenViewCheck(FName ViewName, TFunction<bool(FName)> Check);

private:
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	TArray<TPair<FName, FDelegateHandle>> OpenViewChecks;
	FDelegateHandle WorldCleanupHandle;
};
