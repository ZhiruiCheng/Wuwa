#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Game/LGUI/Mvc/WuwaEventSubsystem.h"
#include "Game/LGUI/View/WuwaUiState.h"
#include "WuwaUiPanel.generated.h"

class AActor;
class UUIButtonComponent;
class UUIItem;
class UUISprite;
class UUIText;
class ULGUIPrefab;
class ULocalPlayer;
class ULocalPlayerSubsystem;
struct FStreamableHandle;

/**
 * 界面生命周期基类（≈ 鸣潮 ComponentAction + UiPanelBase）。
 * 逻辑对象和预制体分开：这个对象先存在，Create 时才生成 RootActor。
 * 所有指令都先进队列：正在进行中（IsBusy）就排队，当前操作完成后按顺序执行。
 */
UCLASS(Abstract)
class WUWA_API UWuwaUiPanel : public UObject
{
	GENERATED_BODY()
public:
	/** 两种创建方式：异步加载预制体后挂到 Parent 下；或接管父界面预制体里已有的 Actor（子组件常用）。 */
	void CreateByPrefab(const TSoftObjectPtr<ULGUIPrefab>& Prefab, UUIItem* Parent, FWuwaUiDone OnDone = nullptr);
	void CreateByActor(AActor* ExistingActor, FWuwaUiDone OnDone = nullptr);
	void Start(FWuwaUiDone OnDone = nullptr);
	void Show(FWuwaUiDone OnDone = nullptr);
	void Hide(FWuwaUiDone OnDone = nullptr);
	void Destroy(FWuwaUiDone OnDone = nullptr);
	/**
	 * 立刻销毁，不排队（≈ 鸣潮 DestroyCompatible）：切地图时用，世界马上就没了，不能再等异步流程。
	 * 取消加载、丢弃排队的指令（各自回调 false）；创建过的会调 OnBeforeDestroy；子组件先销毁。
	 * bDestroyActor = false：Actor 交给正在清理的世界去销毁。
	 */
	void DestroyImmediately(bool bDestroyActor = true);

	/** 子组件跟着父组件走：父 Start/Show 之后带上子组件；隐藏、销毁时先处理子组件。 */
	void AddChild(UWuwaUiPanel* Child);

	EWuwaUiState GetState() const { return State; }
	/** Creating / Starting / Showing / Hiding / Destroying */
	bool IsBusy() const;
	/** 预制体加载进度 0~1（加载界面用）：还没开始或已经加载完都是 1。 */
	float GetPrefabLoadProgress() const;
	AActor* GetRootActor() const { return RootActor; }
	UUIItem* GetRootItem() const;

	virtual UWorld* GetWorld() const override;

protected:
	// —— 业务重写的钩子 ——
	virtual void OnCreate() {}
	/** 需要异步准备（先拉数据、先加载子组件）时重写；准备好后必须调用 Done。 */
	virtual void OnStartAsync(FSimpleDelegate Done) { Done.ExecuteIfBound(); }
	virtual void OnStart() {}
	virtual void OnBeforeShow() {}
	virtual void OnAfterShow() {}
	virtual void OnBeforeHide() {}
	virtual void OnAfterHide() {}
	virtual void OnBeforeDestroy() {}

	// —— MVC（≈ 鸣潮 UiViewBase 的 OnAddEventListener / OnRemoveEventListener）——

	/** 显示前调用：在这里用 ListenEvent 监听事件。只在显示期间监听，被盖住、隐藏时收不到。 */
	virtual void OnAddEventListener() {}
	/** 隐藏后调用；之后框架会自动移除这个界面的所有监听，忘了在这里移除也不会泄漏。 */
	virtual void OnRemoveEventListener() {}

	/** 监听事件，Owner 是这个界面。 */
	FDelegateHandle ListenEvent(FGameplayTag Event, FWuwaEventHandler Handler);
	UWuwaEventSubsystem* GetEvents() const;
	template <typename T>
	T* GetModel() const { return Cast<T>(GetLocalPlayerSubsystem(T::StaticClass())); }
	template <typename T>
	T* GetController() const { return Cast<T>(GetLocalPlayerSubsystem(T::StaticClass())); }
	/** 这个界面属于哪个本地玩家（沿 Outer 找到 UiManager）。 */
	ULocalPlayer* GetOwningLocalPlayer() const;

	// —— 打开/关闭动画 ——

	/** 打开动画：默认播放根节点上 WuwaUiViewAnimation 的打开效果，没有就立即完成。需要自定义动画时重写，播完必须调 Done。 */
	virtual void PlayShowAnimation(TFunction<void()> Done);
	/** 关闭动画：同上。 */
	virtual void PlayHideAnimation(TFunction<void()> Done);
	/** 立刻停止动画，不回调（立即销毁时）。 */
	virtual void StopAnimations();

	// —— 框架内部钩子（≈ 鸣潮的 *Implement）：给 UWuwaUiView 这类框架子类实现通用效果，业务代码用上面的 On* 钩子 ——

	/** Show 开始、根节点刚设为显示时（打开动画之前）。 */
	virtual void OnShowStartImplement() {}
	/** Show 完成（动画播完）时，在 OnAfterShow 之后。 */
	virtual void OnAfterShowImplement() {}
	/** Hide 开始时，在 OnBeforeHide 之后（关闭动画之前）。 */
	virtual void OnHideStartImplement() {}
	/** Hide 完成、根节点已设为不显示时，在 OnAfterHide 之后。 */
	virtual void OnAfterHideImplement() {}
	/** 销毁时（正常 Destroy 和 DestroyImmediately 都会调），在 OnBeforeDestroy 之后；没创建成功的也会调。 */
	virtual void OnBeforeDestroyImplement() {}

	// —— 组件注册（≈ 鸣潮 ComponentRegisterInfos / BtnBindInfo）——
	struct FWuwaUiComponentInfo
	{
		int32 Index;
		TSubclassOf<UActorComponent> Type;
	};
	struct FWuwaUiButtonBind
	{
		int32 Index;
		TFunction<void()> OnClick;
	};

	/** 子类在这里填 ComponentRegisterInfos 和 BtnBindInfo；预制体生成后、OnCreate 之前调用。 */
	virtual void OnRegisterComponent() {}
	/** [编号, 组件类型]：编号是预制体上 WuwaUiComponentsRegistry 里的下标。 */
	TArray<FWuwaUiComponentInfo> ComponentRegisterInfos;
	/** [编号, 点击回调]：编号必须在 ComponentRegisterInfos 里注册成按钮。 */
	TArray<FWuwaUiButtonBind> BtnBindInfo;

	/** 按编号取注册过的组件；类型不对或没注册返回 nullptr。 */
	template <typename T>
	T* GetComponentAt(int32 Index) const { return Cast<T>(RegisteredComponents.FindRef(Index)); }
	UUIItem* GetItem(int32 Index) const;
	UUIText* GetText(int32 Index) const;
	UUISprite* GetSprite(int32 Index) const;
	UUIButtonComponent* GetButton(int32 Index) const;

	/**
	 * 把注册表里第 Index 个 Actor（父预制体里嵌着的子组件根节点，需要在 ComponentRegisterInfos 里登记为 UUIItem）
	 * 包成一个子 Panel：NewObject → CreateByActor → AddChild。之后它跟着父界面 Start/Show/Hide/Destroy。没有这个 Actor 返回 nullptr。
	 */
	template <typename T>
	T* AddChildByIndex(int32 Index)
	{
		AActor* ChildActor = GetRegisteredActor(Index);
		if (!ChildActor)
		{
			return nullptr;
		}
		T* Child = NewObject<T>(this);
		Child->CreateByActor(ChildActor);
		AddChild(Child);
		return Child;
	}
	/** 注册表第 Index 项登记的组件所在的 Actor；没登记或缺失返回 nullptr。 */
	AActor* GetRegisteredActor(int32 Index) const;

private:
	struct FPendingCommand
	{
		EWuwaUiCommand Command;
		/** 轮到它时执行；回调已经捕获在里面。 */
		TFunction<void()> Execute;
		/** 被丢弃时（前一个操作失败）用它回调 false。 */
		TFunction<void()> Discard;
	};

	enum class ETransition : uint8 { Run, AlreadyDone, Invalid };

	/** 忙就排队，不忙就执行。所有公开指令都走这里。 */
	void Submit(EWuwaUiCommand Command, FWuwaUiDone OnDone, TFunction<void(FWuwaUiDone)> Run);
	/** 一个操作结束时调用：改状态、回调、继续处理队列。失败时丢弃队列里除 Destroy 以外的指令。 */
	void Finish(EWuwaUiState NewState, bool bSuccess, const FWuwaUiDone& OnDone);
	void ProcessQueue();
	/** 合法转换表：执行前按当前状态判断。 */
	ETransition CheckTransition(EWuwaUiCommand Command) const;

	void RunCreateByPrefab(TSoftObjectPtr<ULGUIPrefab> Prefab, TWeakObjectPtr<UUIItem> Parent, FWuwaUiDone OnDone);
	void HandlePrefabLoaded(TSoftObjectPtr<ULGUIPrefab> Prefab, TWeakObjectPtr<UUIItem> Parent, FWuwaUiDone OnDone);
	void RunCreateByActor(TWeakObjectPtr<AActor> ExistingActor, FWuwaUiDone OnDone);
	void FinishCreate(AActor* NewRootActor, const FWuwaUiDone& OnDone);
	void RunStart(FWuwaUiDone OnDone);
	void RunShow(FWuwaUiDone OnDone);
	void RunHide(FWuwaUiDone OnDone);
	void RunDestroy(FWuwaUiDone OnDone);

	/** 对当前所有子组件执行同一个指令，全部完成后调用 OnAllDone（子组件失败不影响父组件）。 */
	void RunOnChildren(EWuwaUiCommand Command, TFunction<void()> OnAllDone);

	/** 按 ComponentRegisterInfos 从注册表里取组件；缺的报错并跳过，不影响界面打开。 */
	void RegisterComponents();
	void BindClickEvents();
	/** 销毁 Actor 之前调用：Actor 以后可能被复用（鸣潮的 UiActorPool），回调不能留在下一个使用者的按钮上。 */
	void UnbindClickEvents();
	/** 如果正在监听：调 OnRemoveEventListener，再移除这个界面的所有监听。隐藏、销毁时都会走这里。 */
	void StopListening();
	ULocalPlayerSubsystem* GetLocalPlayerSubsystem(TSubclassOf<ULocalPlayerSubsystem> SubsystemClass) const;

	UPROPERTY(Transient) TObjectPtr<AActor> RootActor;
	UPROPERTY(Transient) TMap<int32, TObjectPtr<UActorComponent>> RegisteredComponents;
	TArray<TPair<TWeakObjectPtr<UUIButtonComponent>, FDelegateHandle>> ClickHandles;
	UPROPERTY(Transient) TArray<TObjectPtr<UWuwaUiPanel>> Children;
	EWuwaUiState State = EWuwaUiState::None;
	TArray<FPendingCommand> Pending;
	bool bProcessingQueue = false;
	/** DestroyImmediately 过：之后晚到的异步完成回调一律按失败处理，不能把状态改回去。 */
	bool bDestroyedImmediately = false;
	/** OnAddEventListener 调过、还没 StopListening。 */
	bool bListening = false;
	TSharedPtr<FStreamableHandle> LoadHandle;
};
