#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Game/LGUI/Core/WuwaUiCameraHandler.h"
#include "Game/LGUI/Layer/WuwaUiLayer.h"
#include "UObject/WeakInterfacePtr.h"
#include "Game/LGUI/View/WuwaUiState.h"
#include "WuwaUiManager.generated.h"

class AActor;
class UDataTable;
class UWuwaUiContainer;
class UWuwaUiLayerRoot;
class UUIItem;
class UWuwaUiView;
struct FWuwaUiViewConfig;

/**
 * UI 入口（≈ 鸣潮 UiManager）：查配置、创建界面对象、分配 ViewId，然后交给所在层的容器打开/关闭。
 * 每层的进出规则（栈、列表、队列……）都在容器里，这里不关心。
 */
UCLASS()
class WUWA_API UWuwaUiManager : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
public:
	static UWuwaUiManager* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * 创建界面对象并交给容器打开；打开是异步的，返回时界面还没显示出来。
	 * 没开 bAllowMultiple 时，已经打开的同名界面直接返回已有的那个。
	 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|UI")
	UWuwaUiView* OpenView(FName ViewName);

	/** 关闭这个名字的所有实例（≈ 鸣潮 CloseView）。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|UI")
	bool CloseView(FName ViewName);

	/** 只关闭一个实例（≈ 鸣潮 CloseViewById）。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|UI")
	bool CloseViewById(int32 ViewId);

	/** 打开了（含打开中、排队中、被栈上层盖住的），并且没有在关闭。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|UI")
	bool IsViewOpen(FName ViewName) const;

	/** 真正显示着（Shown）。被盖住、排队中的不算。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|UI")
	bool IsViewShown(FName ViewName) const;

	/** 这个名字第一个打开着的实例。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|UI")
	UWuwaUiView* GetView(FName ViewName) const;

	UFUNCTION(BlueprintPure, Category = "Wuwa|UI")
	UWuwaUiView* GetViewById(int32 ViewId) const;

	/** 这个名字所有打开着的实例。 */
	void GetViews(FName ViewName, TArray<UWuwaUiView*>& OutViews) const;

	UWuwaUiContainer* GetContainer(EWuwaUiLayer Layer) const { return Containers.FindRef(Layer); }

	/** 配置表里这一行；没注册返回 nullptr（≈ 鸣潮 UiConfig.TryGetViewInfo）。 */
	const FWuwaUiViewConfig* FindViewConfig(FName ViewName) const;

	// —— 打开前检查（≈ 鸣潮 iVe）——

	/** 现在能不能打开（不管是否已经打开）：全局锁、是否注册、是否被挡住、业务检查函数。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|UI")
	bool CanOpenView(FName ViewName) const;

	/** 全局锁（≈ LockOpen）：剧情、切场景过程中暂时不让打开任何界面。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|UI")
	void LockOpen() { bOpenLocked = true; }
	UFUNCTION(BlueprintCallable, Category = "Wuwa|UI")
	void UnlockOpen() { bOpenLocked = false; }
	UFUNCTION(BlueprintPure, Category = "Wuwa|UI")
	bool IsOpenLocked() const { return bOpenLocked; }

	/**
	 * 业务注册的检查（≈ AddOpenViewCheckFunction）：返回 false 就不让打开。
	 * ViewName 为 NAME_None 时对所有界面生效（≈ 鸣潮的 "All"）。
	 */
	FDelegateHandle AddOpenViewCheck(FName ViewName, TFunction<bool(FName ViewName)> Check);
	void RemoveOpenViewCheck(FName ViewName, FDelegateHandle Handle);

	/** 调试：按层打印所有界面和它们的状态。 */
	void DumpViews() const;

	// —— 界面效果的全局状态（由 UWuwaUiView 根据配置表调用，业务代码一般不用直接调）——

	/** 相机系统启动时注册自己（只保存弱引用）；传 nullptr 取消。 */
	void SetUiCameraHandler(IWuwaUiCameraHandler* Handler);
	IWuwaUiCameraHandler* GetUiCameraHandler() const { return CameraHandler.Get(); }

	/** 某个界面要求的时间缩放；多个同时存在时取最慢的，全部撤销后恢复成第一次压入前的值。 */
	void PushTimeDilation(int32 ViewId, float TimeDilation);
	void PopTimeDilation(int32 ViewId);

	/** 全屏界面完全显示/开始关闭时调用：把它下面的各层隐藏/恢复。 */
	void SetFullScreenView(int32 ViewId, EWuwaUiLayer Layer, bool bShown);
	/** 这一层现在是否因为上面有全屏界面而被隐藏。 */
	bool IsLayerHiddenByFullScreen(EWuwaUiLayer Layer) const { return LayersHiddenByFullScreen.Contains(Layer); }

	/** 根预制体里这一层的节点；根节点还没生成时返回 nullptr。 */
	UUIItem* GetLayerItem(EWuwaUiLayer Layer) const;

private:
	struct FOpenViewCheck
	{
		FDelegateHandle Handle;
		TFunction<bool(FName)> Check;
	};

	/** 检查链，不通过时 OutReason 写原因。 */
	bool CheckOpenRules(FName ViewName, const FWuwaUiViewConfig& Config, FString& OutReason) const;

	/** 世界被清理（切地图、结束 PIE）：UI 根节点所在的世界才处理。 */
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	/** 新地图加载完：把切地图前打开着的常驻界面重新打开。 */
	void HandlePostLoadMap(UWorld* World);
	/** 立刻销毁所有界面（调 OnBeforeDestroy），清空容器和记录；bRememberPermanent 时记下常驻界面以便重新打开。 */
	void ClearAllViews(bool bRememberPermanent);

	void ApplyTimeDilation();
	void RefreshFullScreenLayers();

	bool EnsureRoot();
	/** 世界里没有 LGUI EventSystem 就按配置生成一个；没有它界面收不到点击。 */
	void EnsureEventSystem();
	/** 界面打开失败或销毁完成：不再记录它。 */
	void ForgetView(int32 ViewId);
	/** 打开着（没在关闭、没销毁）。 */
	static bool IsAlive(const UWuwaUiView* View);

	UPROPERTY(Transient) TObjectPtr<AActor> RootActor;
	UPROPERTY(Transient) TObjectPtr<UWuwaUiLayerRoot> LayerRoot;
	/** 层 → 容器（≈ 鸣潮 bCr）。 */
	UPROPERTY(Transient) TMap<EWuwaUiLayer, TObjectPtr<UWuwaUiContainer>> Containers;
	/** ViewId → 界面（≈ 鸣潮 qCr）：从创建到销毁完成都在这里，也负责让界面对象不被 GC。 */
	UPROPERTY(Transient) TMap<int32, TObjectPtr<UWuwaUiView>> Views;
	int32 NextViewId = 1;

	/** Initialize 时加载并持有，避免被 GC 后反复加载。 */
	UPROPERTY(Transient) TObjectPtr<UDataTable> ViewConfigTable;
	bool bOpenLocked = false;
	/** 界面名（NAME_None = 所有界面）→ 检查函数。 */
	TMap<FName, TArray<FOpenViewCheck>> OpenViewChecks;
	/** 切地图时打开着的常驻界面，新地图加载完重新打开。 */
	TArray<FName> PermanentViewsToRestore;
	FDelegateHandle WorldCleanupHandle;
	FDelegateHandle PostLoadMapHandle;

	TWeakInterfacePtr<IWuwaUiCameraHandler> CameraHandler;
	/** ViewId → 它要求的时间缩放。 */
	TMap<int32, float> TimeDilations;
	/** 第一个时间缩放压入前的值，全部撤销后恢复它（不覆盖玩法自己设的慢动作）。 */
	float TimeDilationBeforeUi = 1.f;
	/** ViewId → 所在层：完全显示着的全屏界面。 */
	TMap<int32, EWuwaUiLayer> FullScreenViews;
	TSet<EWuwaUiLayer> LayersHiddenByFullScreen;
};
