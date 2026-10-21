#include "Game/LGUI/Core/WuwaUiManager.h"

#include "Core/ActorComponent/UIItem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Event/LGUIEventSystem.h"
#include "Game/LGUI/Container/WuwaUiListContainer.h"
#include "Game/LGUI/Container/WuwaUiQueueContainer.h"
#include "Game/LGUI/Container/WuwaUiSetContainer.h"
#include "Game/LGUI/Container/WuwaUiStackContainer.h"
#include "Game/LGUI/Core/WuwaUiLog.h"
#include "Game/LGUI/Core/WuwaUISettings.h"
#include "Game/LGUI/Layer/WuwaUiLayerRoot.h"
#include "Game/LGUI/View/WuwaUiView.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "LGUIBPLibrary.h"
#include "PrefabSystem/LGUIPrefab.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY(LogWuwaUI);

UWuwaUiManager* UWuwaUiManager::Get(const UObject* WorldContext)
{
	if (!IsValid(WorldContext))
	{
		return nullptr;
	}

	if (const ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(WorldContext))
	{
		return LocalPlayer->GetSubsystem<UWuwaUiManager>();
	}

	const APlayerController* PlayerController = Cast<APlayerController>(WorldContext);
	if (!PlayerController)
	{
		PlayerController = UGameplayStatics::GetPlayerController(WorldContext, 0);
	}
	const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<UWuwaUiManager>() : nullptr;
}

void UWuwaUiManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 每层一个容器（≈ 鸣潮 bCr）：进出规则由容器决定。
	Containers.Add(EWuwaUiLayer::HUD, NewObject<UWuwaUiSetContainer>(this));
	Containers.Add(EWuwaUiLayer::Normal, NewObject<UWuwaUiStackContainer>(this));
	Containers.Add(EWuwaUiLayer::Pop, NewObject<UWuwaUiListContainer>(this));
	Containers.Add(EWuwaUiLayer::Float, NewObject<UWuwaUiQueueContainer>(this));
	Containers.Add(EWuwaUiLayer::Guide, NewObject<UWuwaUiListContainer>(this));
	Containers.Add(EWuwaUiLayer::Loading, NewObject<UWuwaUiSetContainer>(this));
	Containers.Add(EWuwaUiLayer::Debug, NewObject<UWuwaUiSetContainer>(this));

	ViewConfigTable = GetDefault<UWuwaUISettings>()->ViewConfigTable.LoadSynchronous();
	if (!ViewConfigTable)
	{
		UE_LOG(LogWuwaUI, Error, TEXT("ViewConfigTable is not set or failed to load; no view can be opened."));
	}

	// 切地图：旧世界清理时关掉界面，新地图加载完重新打开常驻界面（≈ 鸣潮 UiManager.ClearAsync）。
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &UWuwaUiManager::HandleWorldCleanup);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UWuwaUiManager::HandlePostLoadMap);
}

void UWuwaUiManager::Deinitialize()
{
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	ClearAllViews(false);
	Super::Deinitialize();
}

UWuwaUiView* UWuwaUiManager::OpenView(FName ViewName)
{
	const FWuwaUiViewConfig* Config = FindViewConfig(ViewName);
	if (!Config)
	{
		UE_LOG(LogWuwaUI, Error, TEXT("View '%s' is not registered."), *ViewName.ToString());
		return nullptr;
	}

	// 已经打开（且不允许多开）就返回已有的那个，不算失败。
	if (!Config->bAllowMultiple)
	{
		if (UWuwaUiView* Existing = GetView(ViewName))
		{
			UE_LOG(LogWuwaUI, Warning, TEXT("View '%s' is already open."), *ViewName.ToString());
			return Existing;
		}
	}

	FString Reason;
	if (!CheckOpenRules(ViewName, *Config, Reason))
	{
		// 不让打开是正常的游戏逻辑（被挡住、锁住），不是错误。
		UE_LOG(LogWuwaUI, Log, TEXT("Open '%s' rejected: %s"), *ViewName.ToString(), *Reason);
		return nullptr;
	}

	if (!EnsureRoot())
	{
		return nullptr;
	}

	UUIItem* ParentItem = LayerRoot->GetLayerItem(Config->Layer);
	UWuwaUiContainer* Container = GetContainer(Config->Layer);
	if (!IsValid(ParentItem) || !Container)
	{
		UE_LOG(LogWuwaUI, Error, TEXT("Layer %s for '%s' is not configured."),
			*StaticEnum<EWuwaUiLayer>()->GetNameStringByValue(static_cast<int64>(Config->Layer)), *ViewName.ToString());
		return nullptr;
	}

	// 逻辑对象先创建出来，预制体由容器决定什么时候加载、显示。
	UClass* ViewClass = Config->ViewClass ? Config->ViewClass.Get() : UWuwaUiView::StaticClass();
	UWuwaUiView* View = NewObject<UWuwaUiView>(this, ViewClass);
	const int32 ViewId = NextViewId++;
	View->Init(ViewName, ViewId, *Config, ParentItem);
	Views.Add(ViewId, View);

	TWeakObjectPtr<UWuwaUiManager> WeakThis(this);
	Container->OpenView(View, [WeakThis, ViewId, ViewName](bool bSuccess)
	{
		// 失败时容器已经把界面移走并销毁完了，这里只需要忘掉它。
		if (!bSuccess && WeakThis.IsValid())
		{
			UE_LOG(LogWuwaUI, Log, TEXT("View '%s' #%d did not open (failed or closed before showing)."), *ViewName.ToString(), ViewId);
			WeakThis->ForgetView(ViewId);
		}
	});
	return View;
}

bool UWuwaUiManager::CloseView(FName ViewName)
{
	TArray<UWuwaUiView*> Targets;
	GetViews(ViewName, Targets);
	if (Targets.IsEmpty())
	{
		UE_LOG(LogWuwaUI, Warning, TEXT("View '%s' is not open."), *ViewName.ToString());
		return false;
	}
	for (UWuwaUiView* View : Targets)
	{
		CloseViewById(View->GetViewId());
	}
	return true;
}

bool UWuwaUiManager::CloseViewById(int32 ViewId)
{
	UWuwaUiView* View = Views.FindRef(ViewId);
	if (!IsValid(RootActor) || !IsAlive(View))
	{
		UE_LOG(LogWuwaUI, Warning, TEXT("View #%d is not open."), ViewId);
		return false;
	}
	UWuwaUiContainer* Container = GetContainer(View->GetLayer());
	if (!Container)
	{
		return false;
	}

	// 先标记：关闭期间 IsViewOpen 不再算它，再 OpenView 同名界面会创建新的实例。
	View->MarkCloseRequested();
	TWeakObjectPtr<UWuwaUiManager> WeakThis(this);
	Container->CloseView(View, [WeakThis, ViewId](bool)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->ForgetView(ViewId);
		}
	});
	return true;
}

bool UWuwaUiManager::IsViewOpen(FName ViewName) const
{
	return GetView(ViewName) != nullptr;
}

bool UWuwaUiManager::IsViewShown(FName ViewName) const
{
	TArray<UWuwaUiView*> Instances;
	GetViews(ViewName, Instances);
	return Instances.ContainsByPredicate([](const UWuwaUiView* View) { return View->GetState() == EWuwaUiState::Shown; });
}

UWuwaUiView* UWuwaUiManager::GetView(FName ViewName) const
{
	TArray<UWuwaUiView*> Instances;
	GetViews(ViewName, Instances);
	return Instances.IsEmpty() ? nullptr : Instances[0];
}

UWuwaUiView* UWuwaUiManager::GetViewById(int32 ViewId) const
{
	UWuwaUiView* View = Views.FindRef(ViewId);
	return IsValid(RootActor) && IsAlive(View) ? View : nullptr;
}

void UWuwaUiManager::GetViews(FName ViewName, TArray<UWuwaUiView*>& OutViews) const
{
	// 根节点没了（切地图）说明界面的 Actor 也都没了。
	if (!IsValid(RootActor))
	{
		return;
	}
	for (const TPair<int32, TObjectPtr<UWuwaUiView>>& Pair : Views)
	{
		if (IsAlive(Pair.Value) && Pair.Value->GetViewName() == ViewName)
		{
			OutViews.Add(Pair.Value);
		}
	}
}

void UWuwaUiManager::DumpViews() const
{
	const UEnum* LayerEnum = StaticEnum<EWuwaUiLayer>();
	const UEnum* StateEnum = StaticEnum<EWuwaUiState>();
	UE_LOG(LogWuwaUI, Display, TEXT("Views tracked: %d"), Views.Num());
	for (const TPair<EWuwaUiLayer, TObjectPtr<UWuwaUiContainer>>& Pair : Containers)
	{
		TArray<UWuwaUiView*> LayerViews;
		Pair.Value->GetViews(LayerViews);
		if (LayerViews.IsEmpty())
		{
			continue;
		}
		UE_LOG(LogWuwaUI, Display, TEXT("  %s (%s):"), *LayerEnum->GetNameStringByValue(static_cast<int64>(Pair.Key)), *Pair.Value->GetClass()->GetName());
		for (const UWuwaUiView* View : LayerViews)
		{
			UE_LOG(LogWuwaUI, Display, TEXT("    #%d %s (%s) %s%s"), View->GetViewId(), *View->GetViewName().ToString(), *View->GetClass()->GetName(),
				*StateEnum->GetNameStringByValue(static_cast<int64>(View->GetState())), View->IsCloseRequested() ? TEXT(" [closing]") : TEXT(""));
		}
	}
}

bool UWuwaUiManager::EnsureRoot()
{
	if (IsValid(RootActor))
	{
		return true;
	}

	// 根节点不在了（第一次打开，或切地图时随旧世界一起销毁），旧界面的 Actor 也都没了，丢掉这些记录。
	// 正常情况下切地图时 HandleWorldCleanup 已经清过了；这里兜底（比如根节点被别处删掉）。
	if (Views.Num() > 0)
	{
		ClearAllViews(false);
	}
	LayerRoot = nullptr;

	// EventSystem 和根节点一样跟着世界走：切地图后两个都要重新准备。
	EnsureEventSystem();

	const UWuwaUISettings* Settings = GetDefault<UWuwaUISettings>();
	ULGUIPrefab* Prefab = Settings->RootPrefab.LoadSynchronous();
	if (!IsValid(Prefab))
	{
		UE_LOG(LogWuwaUI, Error, TEXT("Failed to load RootPrefab."));
		return false;
	}

	RootActor = Prefab->LoadPrefab(GetWorld(), nullptr);
	if (!IsValid(RootActor))
	{
		UE_LOG(LogWuwaUI, Error, TEXT("Failed to create UI root."));
		return false;
	}

	LayerRoot = RootActor->FindComponentByClass<UWuwaUiLayerRoot>();
	if (!IsValid(LayerRoot))
	{
		UE_LOG(LogWuwaUI, Error, TEXT("RootPrefab is missing UWuwaUiLayerRoot."));
		ULGUIBPLibrary::DestroyActorWithHierarchy(RootActor, true);
		RootActor = nullptr;
		return false;
	}

	// 每一层都铺满整个画布：界面按层的四角/中心做锚点（HUD 贴左上、技能贴右下……），层本身不能是别的尺寸。
	// 根预制体里的层节点如果是默认的 100x100，这里统一拉满，不依赖手工拼对。
	FUIAnchorData Stretch;
	Stretch.AnchorMin = FVector2D(0.f, 0.f);
	Stretch.AnchorMax = FVector2D(1.f, 1.f);
	Stretch.SizeDelta = FVector2D::ZeroVector;
	for (int32 Index = 0; Index < static_cast<int32>(EWuwaUiLayer::Count); ++Index)
	{
		if (UUIItem* LayerItem = LayerRoot->GetLayerItem(static_cast<EWuwaUiLayer>(Index)))
		{
			LayerItem->SetAnchorData(Stretch);
		}
	}

	return true;
}

void UWuwaUiManager::ForgetView(int32 ViewId)
{
	Views.Remove(ViewId);
}

bool UWuwaUiManager::IsAlive(const UWuwaUiView* View)
{
	if (!IsValid(View) || View->IsCloseRequested())
	{
		return false;
	}
	const EWuwaUiState State = View->GetState();
	return State != EWuwaUiState::Destroying && State != EWuwaUiState::Destroyed;
}

void UWuwaUiManager::EnsureEventSystem()
{
	UWorld* World = GetWorld();
	// 地图里已经摆了（或者别处已经生成过）就直接用：一个世界只能有一个 LGUIEventSystem。
	if (!World || ULGUIEventSystem::GetLGUIEventSystemInstance(World))
	{
		return;
	}

	const UWuwaUISettings* Settings = GetDefault<UWuwaUISettings>();
	UClass* EventSystemClass = Settings->EventSystemClass.LoadSynchronous();
	if (!EventSystemClass)
	{
		UE_LOG(LogWuwaUI, Error, TEXT("EventSystemClass is not set or failed to load; UI will not receive clicks."));
		return;
	}

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	if (!World->SpawnActor<AActor>(EventSystemClass, Params))
	{
		UE_LOG(LogWuwaUI, Error, TEXT("Failed to spawn EventSystem '%s'."), *EventSystemClass->GetName());
	}
}

const FWuwaUiViewConfig* UWuwaUiManager::FindViewConfig(FName ViewName) const
{
	return ViewConfigTable ? ViewConfigTable->FindRow<FWuwaUiViewConfig>(ViewName, TEXT("UiManager"), false) : nullptr;
}

bool UWuwaUiManager::CanOpenView(FName ViewName) const
{
	const FWuwaUiViewConfig* Config = FindViewConfig(ViewName);
	FString Reason;
	return Config && CheckOpenRules(ViewName, *Config, Reason);
}

bool UWuwaUiManager::CheckOpenRules(FName ViewName, const FWuwaUiViewConfig& Config, FString& OutReason) const
{
	if (bOpenLocked)
	{
		OutReason = TEXT("opening is locked");
		return false;
	}

	for (const FName& Obstructer : Config.BeObstructViews)
	{
		if (IsViewShown(Obstructer))
		{
			OutReason = FString::Printf(TEXT("obstructed by '%s'"), *Obstructer.ToString());
			return false;
		}
	}

	// 先查只针对这个界面的，再查对所有界面生效的（和鸣潮一样的顺序）。
	for (const FName Key : {ViewName, FName(NAME_None)})
	{
		if (const TArray<FOpenViewCheck>* Checks = OpenViewChecks.Find(Key))
		{
			for (const FOpenViewCheck& Check : *Checks)
			{
				if (!Check.Check(ViewName))
				{
					OutReason = Key.IsNone() ? TEXT("rejected by a global check") : TEXT("rejected by a view check");
					return false;
				}
			}
		}
	}
	return true;
}

FDelegateHandle UWuwaUiManager::AddOpenViewCheck(FName ViewName, TFunction<bool(FName)> Check)
{
	const FDelegateHandle Handle(FDelegateHandle::GenerateNewHandle);
	OpenViewChecks.FindOrAdd(ViewName).Add({Handle, MoveTemp(Check)});
	return Handle;
}

void UWuwaUiManager::RemoveOpenViewCheck(FName ViewName, FDelegateHandle Handle)
{
	if (TArray<FOpenViewCheck>* Checks = OpenViewChecks.Find(ViewName))
	{
		Checks->RemoveAll([Handle](const FOpenViewCheck& Check) { return Check.Handle == Handle; });
		if (Checks->IsEmpty())
		{
			OpenViewChecks.Remove(ViewName);
		}
	}
}

void UWuwaUiManager::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	// 只管 UI 根节点所在的世界（编辑器、LGUI 预制体预览等其他世界的清理不相干）。
	if (!World || !RootActor || RootActor->GetWorld() != World)
	{
		return;
	}
	ClearAllViews(true);
}

void UWuwaUiManager::HandlePostLoadMap(UWorld* World)
{
	if (PermanentViewsToRestore.IsEmpty() || !World || World != GetWorld())
	{
		return;
	}
	const TArray<FName> ToRestore = MoveTemp(PermanentViewsToRestore);
	PermanentViewsToRestore.Reset();
	for (const FName& ViewName : ToRestore)
	{
		UE_LOG(LogWuwaUI, Log, TEXT("Reopening permanent view '%s' after map change."), *ViewName.ToString());
		OpenView(ViewName);
	}
}

void UWuwaUiManager::ClearAllViews(bool bRememberPermanent)
{
	TArray<UWuwaUiView*> ToDestroy;
	for (const TPair<int32, TObjectPtr<UWuwaUiView>>& Pair : Views)
	{
		UWuwaUiView* View = Pair.Value;
		if (!IsValid(View))
		{
			continue;
		}
		if (bRememberPermanent && View->IsPermanent() && !View->IsCloseRequested())
		{
			PermanentViewsToRestore.AddUnique(View->GetViewName());
		}
		ToDestroy.Add(View);
	}

	// 先清空容器和记录，再逐个销毁：销毁时触发的回调（打开失败、关闭完成）找不到记录，就不会再改容器。
	for (const TPair<EWuwaUiLayer, TObjectPtr<UWuwaUiContainer>>& Pair : Containers)
	{
		Pair.Value->ClearContainer();
	}
	Views.Empty();
	RootActor = nullptr;
	LayerRoot = nullptr;

	// Actor 交给正在清理的世界去销毁；这里只保证钩子被调用（界面会各自撤销时间缩放、相机等）。
	for (UWuwaUiView* View : ToDestroy)
	{
		View->DestroyImmediately(false);
	}
	FullScreenViews.Empty();
	LayersHiddenByFullScreen.Empty();
}

// —— 界面效果的全局状态 ——

void UWuwaUiManager::SetUiCameraHandler(IWuwaUiCameraHandler* Handler)
{
	CameraHandler = Handler;
}

void UWuwaUiManager::PushTimeDilation(int32 ViewId, float TimeDilation)
{
	if (TimeDilations.IsEmpty())
	{
		TimeDilationBeforeUi = UGameplayStatics::GetGlobalTimeDilation(this);
	}
	TimeDilations.Add(ViewId, FMath::Clamp(TimeDilation, 0.f, 1.f));
	ApplyTimeDilation();
}

void UWuwaUiManager::PopTimeDilation(int32 ViewId)
{
	if (TimeDilations.Remove(ViewId) > 0)
	{
		ApplyTimeDilation();
	}
}

void UWuwaUiManager::ApplyTimeDilation()
{
	float Value = TimeDilationBeforeUi;
	if (!TimeDilations.IsEmpty())
	{
		Value = 1.f;
		for (const TPair<int32, float>& Pair : TimeDilations)
		{
			Value = FMath::Min(Value, Pair.Value);
		}
	}
	// 0（暂停）会被引擎夹到 WorldSettings 的 MinGlobalTimeDilation，游戏几乎停住；UI 动画和音效不受影响。
	UGameplayStatics::SetGlobalTimeDilation(this, Value);
}

void UWuwaUiManager::SetFullScreenView(int32 ViewId, EWuwaUiLayer Layer, bool bShown)
{
	if (bShown)
	{
		FullScreenViews.Add(ViewId, Layer);
	}
	else if (FullScreenViews.Remove(ViewId) == 0)
	{
		return;
	}
	RefreshFullScreenLayers();
}

void UWuwaUiManager::RefreshFullScreenLayers()
{
	if (!IsValid(LayerRoot))
	{
		return;
	}

	// 最上面那个全屏界面所在层以下的层都看不见了。同一层被盖住的界面由容器（栈）负责隐藏。
	int32 Threshold = -1;
	for (const TPair<int32, EWuwaUiLayer>& Pair : FullScreenViews)
	{
		Threshold = FMath::Max(Threshold, static_cast<int32>(Pair.Value));
	}

	for (int32 Index = 0; Index < static_cast<int32>(EWuwaUiLayer::Count); ++Index)
	{
		const EWuwaUiLayer Layer = static_cast<EWuwaUiLayer>(Index);
		UUIItem* Item = LayerRoot->GetLayerItem(Layer);
		if (!Item)
		{
			continue;
		}
		const bool bShouldHide = Index < Threshold;
		if (bShouldHide && !LayersHiddenByFullScreen.Contains(Layer))
		{
			LayersHiddenByFullScreen.Add(Layer);
			Item->SetIsUIActive(false);
		}
		else if (!bShouldHide && LayersHiddenByFullScreen.Contains(Layer))
		{
			// 只恢复我们自己藏起来的层。
			LayersHiddenByFullScreen.Remove(Layer);
			Item->SetIsUIActive(true);
		}
	}
}

UUIItem* UWuwaUiManager::GetLayerItem(EWuwaUiLayer Layer) const
{
	return IsValid(LayerRoot) ? LayerRoot->GetLayerItem(Layer) : nullptr;
}
