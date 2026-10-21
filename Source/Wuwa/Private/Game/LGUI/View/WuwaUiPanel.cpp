#include "Game/LGUI/View/WuwaUiPanel.h"

#include "Core/ActorComponent/UIItem.h"
#include "Core/ActorComponent/UISprite.h"
#include "Core/ActorComponent/UIText.h"
#include "Engine/AssetManager.h"
#include "Engine/LocalPlayer.h"
#include "Game/LGUI/Core/WuwaUiManager.h"
#include "Game/LGUI/Component/WuwaUiComponentsRegistry.h"
#include "Game/LGUI/Component/WuwaUiViewAnimation.h"
#include "Interaction/UIButtonComponent.h"
#include "Engine/StreamableManager.h"
#include "Game/Common/Resource/WuwaResourceSubsystem.h"
#include "Game/LGUI/Core/WuwaUiLog.h"
#include "GameFramework/Actor.h"
#include "LGUIBPLibrary.h"
#include "PrefabSystem/LGUIPrefab.h"

namespace
{
	const TCHAR* LexCommand(EWuwaUiCommand Command)
	{
		switch (Command)
		{
		case EWuwaUiCommand::Create:  return TEXT("Create");
		case EWuwaUiCommand::Start:   return TEXT("Start");
		case EWuwaUiCommand::Show:    return TEXT("Show");
		case EWuwaUiCommand::Hide:    return TEXT("Hide");
		case EWuwaUiCommand::Destroy: return TEXT("Destroy");
		}
		return TEXT("?");
	}

	FString LexState(EWuwaUiState State)
	{
		return StaticEnum<EWuwaUiState>()->GetNameStringByValue(static_cast<int64>(State));
	}
}

// —— 公开指令：只负责把"要做什么"放进队列，真正执行在 Run* 里 ——

void UWuwaUiPanel::CreateByPrefab(const TSoftObjectPtr<ULGUIPrefab>& Prefab, UUIItem* Parent, FWuwaUiDone OnDone)
{
	TWeakObjectPtr<UUIItem> WeakParent(Parent);
	Submit(EWuwaUiCommand::Create, MoveTemp(OnDone), [this, Prefab, WeakParent](FWuwaUiDone Done)
	{
		RunCreateByPrefab(Prefab, WeakParent, MoveTemp(Done));
	});
}

void UWuwaUiPanel::CreateByActor(AActor* ExistingActor, FWuwaUiDone OnDone)
{
	TWeakObjectPtr<AActor> WeakActor(ExistingActor);
	Submit(EWuwaUiCommand::Create, MoveTemp(OnDone), [this, WeakActor](FWuwaUiDone Done)
	{
		RunCreateByActor(WeakActor, MoveTemp(Done));
	});
}

void UWuwaUiPanel::Start(FWuwaUiDone OnDone)
{
	Submit(EWuwaUiCommand::Start, MoveTemp(OnDone), [this](FWuwaUiDone Done) { RunStart(MoveTemp(Done)); });
}

void UWuwaUiPanel::Show(FWuwaUiDone OnDone)
{
	Submit(EWuwaUiCommand::Show, MoveTemp(OnDone), [this](FWuwaUiDone Done) { RunShow(MoveTemp(Done)); });
}

void UWuwaUiPanel::Hide(FWuwaUiDone OnDone)
{
	Submit(EWuwaUiCommand::Hide, MoveTemp(OnDone), [this](FWuwaUiDone Done) { RunHide(MoveTemp(Done)); });
}

void UWuwaUiPanel::Destroy(FWuwaUiDone OnDone)
{
	Submit(EWuwaUiCommand::Destroy, MoveTemp(OnDone), [this](FWuwaUiDone Done) { RunDestroy(MoveTemp(Done)); });
}

void UWuwaUiPanel::DestroyImmediately(bool bDestroyActor)
{
	if (State == EWuwaUiState::Destroyed)
	{
		return;
	}

	// 先把还没执行的指令拿出来，最后再统一回调 false：回调里的代码看到的应该是已经销毁完的状态。
	TArray<FPendingCommand> Dropped = MoveTemp(Pending);
	Pending.Reset();
	if (LoadHandle.IsValid())
	{
		LoadHandle->CancelHandle();
		LoadHandle.Reset();
	}

	// 创建过（有 RootActor）才调钩子；不用 IsValid：世界清理时 Actor 可能已经被标记销毁，但钩子仍然要调。
	const bool bWasCreated = RootActor != nullptr;
	bDestroyedImmediately = true;
	State = EWuwaUiState::Destroying;
	StopAnimations();
	if (bWasCreated)
	{
		OnBeforeDestroy();
	}
	OnBeforeDestroyImplement();
	for (UWuwaUiPanel* Child : TArray<TObjectPtr<UWuwaUiPanel>>(Children))
	{
		if (IsValid(Child))
		{
			Child->DestroyImmediately(bDestroyActor);
		}
	}
	Children.Empty();
	StopListening();
	UnbindClickEvents();
	if (bDestroyActor && IsValid(RootActor))
	{
		ULGUIBPLibrary::DestroyActorWithHierarchy(RootActor, true);
	}
	RootActor = nullptr;
	State = EWuwaUiState::Destroyed;

	for (FPendingCommand& Command : Dropped)
	{
		Command.Discard();
	}
	// 回调里如果又提交了指令（比如再 Destroy 一次），按已销毁的状态处理掉。
	ProcessQueue();
}

void UWuwaUiPanel::AddChild(UWuwaUiPanel* Child)
{
	if (!IsValid(Child) || Child == this || Children.Contains(Child))
	{
		return;
	}
	Children.Add(Child);
}

float UWuwaUiPanel::GetPrefabLoadProgress() const
{
	return LoadHandle.IsValid() ? LoadHandle->GetProgress() : 1.f;
}

bool UWuwaUiPanel::IsBusy() const
{
	switch (State)
	{
	case EWuwaUiState::Creating:
	case EWuwaUiState::Starting:
	case EWuwaUiState::Showing:
	case EWuwaUiState::Hiding:
	case EWuwaUiState::Destroying:
		return true;
	default:
		return false;
	}
}

UUIItem* UWuwaUiPanel::GetRootItem() const
{
	return IsValid(RootActor) ? Cast<UUIItem>(RootActor->GetRootComponent()) : nullptr;
}

UWorld* UWuwaUiPanel::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}
	if (IsValid(RootActor))
	{
		return RootActor->GetWorld();
	}
	// Outer 是 UiManager（或父界面），沿 Outer 链拿到 LocalPlayer 的世界。
	return Super::GetWorld();
}

// —— 队列 ——

void UWuwaUiPanel::Submit(EWuwaUiCommand Command, FWuwaUiDone OnDone, TFunction<void(FWuwaUiDone)> Run)
{
	FPendingCommand& Entry = Pending.AddDefaulted_GetRef();
	Entry.Command = Command;
	// 合法性要等轮到它时再按"那时的状态"判断，而不是提交时。
	Entry.Execute = [this, Command, OnDone, Run = MoveTemp(Run)]()
	{
		switch (CheckTransition(Command))
		{
		case ETransition::Run:
			Run(OnDone);
			break;
		case ETransition::AlreadyDone:
			if (OnDone) { OnDone(true); }
			break;
		case ETransition::Invalid:
			UE_LOG(LogWuwaUI, Error, TEXT("%s: %s is not allowed in state %s."),
				*GetClass()->GetName(), LexCommand(Command), *LexState(State));
			if (OnDone) { OnDone(false); }
			break;
		}
	};
	Entry.Discard = [OnDone]()
	{
		if (OnDone) { OnDone(false); }
	};
	ProcessQueue();
}

void UWuwaUiPanel::ProcessQueue()
{
	// 回调里再提交指令会重入到这里：交给外层循环继续处理即可。
	if (bProcessingQueue)
	{
		return;
	}
	TGuardValue<bool> Guard(bProcessingQueue, true);

	while (!IsBusy() && Pending.Num() > 0)
	{
		//取栈顶的元素执行
		FPendingCommand Next = MoveTemp(Pending[0]);
		Pending.RemoveAt(0);
		Next.Execute();
	}
}

void UWuwaUiPanel::Finish(EWuwaUiState NewState, bool bSuccess, const FWuwaUiDone& OnDone)
{
	// 被 DestroyImmediately 打断的操作晚到了：不再改状态，只告诉调用方没成功。
	if (bDestroyedImmediately && NewState != EWuwaUiState::Destroyed)
	{
		if (OnDone) { OnDone(false); }
		return;
	}

	// 先改状态：回调里查状态、再提交指令时，看到的必须是"已经完成"的状态。
	State = NewState;

	// 操作失败时，后面排着的指令都建立在它成功的前提上，一起失败；Destroy 保留，保证能清理干净。
	TArray<FPendingCommand> Dropped;
	if (!bSuccess)
	{
		TArray<FPendingCommand> Kept;
		for (FPendingCommand& Command : Pending)
		{
			(Command.Command == EWuwaUiCommand::Destroy ? Kept : Dropped).Add(MoveTemp(Command));
		}
		Pending = MoveTemp(Kept);
	}

	if (OnDone)
	{
		OnDone(bSuccess);
	}
	for (FPendingCommand& Command : Dropped)
	{
		Command.Discard();
	}

	// 最后才处理队列：保证调用方先拿到这一步的结果，下一条指令才开始。
	ProcessQueue();
}

UWuwaUiPanel::ETransition UWuwaUiPanel::CheckTransition(EWuwaUiCommand Command) const
{
	switch (Command)
	{
	case EWuwaUiCommand::Create:
		return State == EWuwaUiState::None ? ETransition::Run : ETransition::Invalid;
	case EWuwaUiCommand::Start:
		return State == EWuwaUiState::Created ? ETransition::Run : ETransition::Invalid;
	case EWuwaUiCommand::Show:
		if (State == EWuwaUiState::Shown) { return ETransition::AlreadyDone; }
		return State == EWuwaUiState::Started || State == EWuwaUiState::Hidden ? ETransition::Run : ETransition::Invalid;
	case EWuwaUiCommand::Hide:
		// Created：还没显示过，本来就看不见（容器关闭一个刚创建完的界面时会走到这里）。
		if (State == EWuwaUiState::Hidden || State == EWuwaUiState::Created) { return ETransition::AlreadyDone; }
		return State == EWuwaUiState::Started || State == EWuwaUiState::Shown ? ETransition::Run : ETransition::Invalid;
	case EWuwaUiCommand::Destroy:
		return State == EWuwaUiState::Destroyed ? ETransition::AlreadyDone : ETransition::Run;
	}
	return ETransition::Invalid;
}

// —— 各指令的执行 ——

void UWuwaUiPanel::RunCreateByPrefab(TSoftObjectPtr<ULGUIPrefab> Prefab, TWeakObjectPtr<UUIItem> Parent, FWuwaUiDone OnDone)
{
	State = EWuwaUiState::Creating;

	if (Prefab.IsNull())
	{
		UE_LOG(LogWuwaUI, Error, TEXT("%s: prefab is not set."), *GetClass()->GetName());
		Finish(EWuwaUiState::None, false, OnDone);
		return;
	}

	// LGUI 没有异步实例化：先异步加载预制体资源，回来再同步 LoadPrefab。UAssetManager只会加载这个磁盘的内容到内存,不会实例化
	// WeakLambda：加载回来时界面对象已经没了，回调就不会执行。
	// 走统一的资源加载入口（UWuwaResourceSubsystem）：日志里能看到每个界面预制体加载了多久，加载界面也能读到进度。
	UWuwaResourceSubsystem* Resources = UWuwaResourceSubsystem::Get(this);
	if (!Resources)
	{
		UE_LOG(LogWuwaUI, Error, TEXT("%s: no resource subsystem."), *GetClass()->GetName());
		Finish(EWuwaUiState::None, false, OnDone);
		return;
	}
	TWeakObjectPtr<UWuwaUiPanel> WeakThis(this);
	LoadHandle = Resources->LoadAsync({Prefab.ToSoftObjectPath()}, FString::Printf(TEXT("UI:%s"), *GetClass()->GetName()),
		[WeakThis, Prefab, Parent, OnDone]()
		{
			UWuwaUiPanel* Self = WeakThis.Get();
			if (!Self)
			{
				return;
			}
			Self->HandlePrefabLoaded(Prefab, Parent, OnDone);
		}, UWuwaResourceSubsystem::HighPriority);
	// 预制体已经在内存里时回调同步执行完，句柄也已完成，不需要保留。
	if (LoadHandle.IsValid() && LoadHandle->HasLoadCompleted())
	{
		LoadHandle.Reset();
	}
}

void UWuwaUiPanel::HandlePrefabLoaded(TSoftObjectPtr<ULGUIPrefab> Prefab, TWeakObjectPtr<UUIItem> Parent, FWuwaUiDone OnDone)
{
	//加载完成后就不需要这个句柄了,直接释放
	LoadHandle.Reset();

	ULGUIPrefab* LoadedPrefab = Prefab.Get();
	UUIItem* ParentItem = Parent.Get();
	if (!IsValid(LoadedPrefab) || !IsValid(ParentItem))
	{
		UE_LOG(LogWuwaUI, Error, TEXT("%s: failed to load prefab '%s' or parent is gone."),
			*GetClass()->GetName(), *Prefab.ToString());
		Finish(EWuwaUiState::None, false, OnDone);
		return;
	}

	//LGUI真正实例化的地方
	AActor* NewRootActor = LoadedPrefab->LoadPrefab(ParentItem->GetWorld(), ParentItem, true);
	if (!IsValid(NewRootActor) || !Cast<UUIItem>(NewRootActor->GetRootComponent()))
	{
		if (IsValid(NewRootActor))
		{
			ULGUIBPLibrary::DestroyActorWithHierarchy(NewRootActor, true);
		}
		Finish(EWuwaUiState::None, false, OnDone);
		return;
	}

	FinishCreate(NewRootActor, OnDone);
}

void UWuwaUiPanel::RunCreateByActor(TWeakObjectPtr<AActor> ExistingActor, FWuwaUiDone OnDone)
{
	State = EWuwaUiState::Creating;

	AActor* Actor = ExistingActor.Get();
	if (!IsValid(Actor) || !Cast<UUIItem>(Actor->GetRootComponent()))
	{
		UE_LOG(LogWuwaUI, Error, TEXT("%s: CreateByActor needs a valid actor with a UIItem root."), *GetClass()->GetName());
		Finish(EWuwaUiState::None, false, OnDone);
		return;
	}

	FinishCreate(Actor, OnDone);
}

void UWuwaUiPanel::FinishCreate(AActor* NewRootActor, const FWuwaUiDone& OnDone)
{
	RootActor = NewRootActor;
	// 和鸣潮 UiPanelBase 的顺序一样：注册组件 → 绑定点击 → 藏起来 → OnCreate，业务在 OnCreate 里就能用 GetText(i)。
	OnRegisterComponent();
	RegisterComponents();
	BindClickEvents();
	// 生成出来先藏起来，等 Show 再显示，避免 Start 期间闪一下。
	if (UUIItem* RootItem = GetRootItem()) { RootItem->SetIsUIActive(false); }
	OnCreate();
	Finish(EWuwaUiState::Created, true, OnDone);
}

void UWuwaUiPanel::RunStart(FWuwaUiDone OnDone)
{
	State = EWuwaUiState::Starting;

	OnStartAsync(FSimpleDelegate::CreateWeakLambda(this, [this, OnDone]()
	{
		// 防止子类把 Done 调用了不止一次。
		if (State != EWuwaUiState::Starting)
		{
			return;
		}
		OnStart();
		RunOnChildren(EWuwaUiCommand::Start, [this, OnDone]()
		{
			Finish(EWuwaUiState::Started, true, OnDone);
		});
	}));
}

void UWuwaUiPanel::RunShow(FWuwaUiDone OnDone)
{
	State = EWuwaUiState::Showing;

	// 和鸣潮一样：先开始监听，再 OnBeforeShow（业务通常在 OnBeforeShow 里从 Model 刷新一遍）。
	if (!bListening)
	{
		bListening = true;
		OnAddEventListener();
	}
	OnBeforeShow();
	if (UUIItem* RootItem = GetRootItem()) { RootItem->SetIsUIActive(true); }
	OnShowStartImplement();

	// 子组件 Show 和自己的打开动画同时进行，都完成才算 Shown。
	TWeakObjectPtr<UWuwaUiPanel> WeakThis(this);
	TSharedRef<int32> Remaining = MakeShared<int32>(2);
	TFunction<void()> Join = [WeakThis, Remaining, OnDone]()
	{
		UWuwaUiPanel* Self = WeakThis.Get();
		if (--(*Remaining) != 0 || !Self)
		{
			return;
		}
		if (!Self->bDestroyedImmediately)
		{
			Self->OnAfterShow();
			Self->OnAfterShowImplement();
		}
		Self->Finish(EWuwaUiState::Shown, true, OnDone);
	};
	RunOnChildren(EWuwaUiCommand::Show, Join);
	PlayShowAnimation(Join);
}

void UWuwaUiPanel::RunHide(FWuwaUiDone OnDone)
{
	State = EWuwaUiState::Hiding;

	OnBeforeHide();
	OnHideStartImplement();

	// 子组件 Hide 和自己的关闭动画同时进行，都完成后再把自己设为不显示。
	TWeakObjectPtr<UWuwaUiPanel> WeakThis(this);
	TSharedRef<int32> Remaining = MakeShared<int32>(2);
	TFunction<void()> Join = [WeakThis, Remaining, OnDone]()
	{
		UWuwaUiPanel* Self = WeakThis.Get();
		if (--(*Remaining) != 0 || !Self)
		{
			return;
		}
		if (!Self->bDestroyedImmediately)
		{
			if (UUIItem* RootItem = Self->GetRootItem()) { RootItem->SetIsUIActive(false); }
			Self->OnAfterHide();
			Self->StopListening();
			Self->OnAfterHideImplement();
		}
		Self->Finish(EWuwaUiState::Hidden, true, OnDone);
	};
	RunOnChildren(EWuwaUiCommand::Hide, Join);
	PlayHideAnimation(Join);
}

void UWuwaUiPanel::RunDestroy(FWuwaUiDone OnDone)
{
	// 没创建成功过（None）的界面不调钩子，业务代码不用判断控件是否存在。
	const bool bWasCreated = IsValid(RootActor);
	State = EWuwaUiState::Destroying;

	if (bWasCreated)
	{
		OnBeforeDestroy();
	}
	OnBeforeDestroyImplement();
	// 先销毁子组件，再销毁自己。
	RunOnChildren(EWuwaUiCommand::Destroy, [this, OnDone]()
	{
		Children.Empty();
		StopListening();
		UnbindClickEvents();
		if (IsValid(RootActor))
		{
			ULGUIBPLibrary::DestroyActorWithHierarchy(RootActor, true);
		}
		RootActor = nullptr;
		Finish(EWuwaUiState::Destroyed, true, OnDone);
	});
}

void UWuwaUiPanel::RunOnChildren(EWuwaUiCommand Command, TFunction<void()> OnAllDone)
{
	// 只处理"这个指令对它有意义"的子组件；正在进行中的也算，指令会在它那边排队。
	auto Accepts = [Command](EWuwaUiState ChildState)
	{
		switch (Command)
		{
		case EWuwaUiCommand::Start:
			return ChildState == EWuwaUiState::Creating || ChildState == EWuwaUiState::Created;
		case EWuwaUiCommand::Show:
			return ChildState == EWuwaUiState::Starting || ChildState == EWuwaUiState::Started
				|| ChildState == EWuwaUiState::Hiding || ChildState == EWuwaUiState::Hidden;
		case EWuwaUiCommand::Hide:
			return ChildState == EWuwaUiState::Starting || ChildState == EWuwaUiState::Started
				|| ChildState == EWuwaUiState::Showing || ChildState == EWuwaUiState::Shown;
		case EWuwaUiCommand::Destroy:
			return ChildState != EWuwaUiState::Destroying && ChildState != EWuwaUiState::Destroyed;
		default:
			return false;
		}
	};

	TArray<UWuwaUiPanel*> Targets;
	for (UWuwaUiPanel* Child : Children)
	{
		if (IsValid(Child) && Accepts(Child->GetState()))
		{
			Targets.Add(Child);
		}
	}

	if (Targets.IsEmpty())
	{
		OnAllDone();
		return;
	}

	// 子组件可能异步完成：计数归零时才继续，并且父界面那时必须还活着。
	TWeakObjectPtr<UWuwaUiPanel> WeakThis(this);
	TSharedRef<int32> Remaining = MakeShared<int32>(Targets.Num());
	TSharedRef<TFunction<void()>> SharedDone = MakeShared<TFunction<void()>>(MoveTemp(OnAllDone));
	const FWuwaUiDone ChildDone = [WeakThis, Remaining, SharedDone](bool)
	{
		if (--(*Remaining) == 0 && WeakThis.IsValid())
		{
			(*SharedDone)();
		}
	};

	for (UWuwaUiPanel* Child : Targets)
	{
		switch (Command)
		{
		case EWuwaUiCommand::Start:   Child->Start(ChildDone); break;
		case EWuwaUiCommand::Show:    Child->Show(ChildDone); break;
		case EWuwaUiCommand::Hide:    Child->Hide(ChildDone); break;
		case EWuwaUiCommand::Destroy: Child->Destroy(ChildDone); break;
		default: break;
		}
	}
}

// —— 组件注册 ——

namespace
{
	/** "Root/Panel/BtnClose" 形式的层级路径，报错时方便在预制体里定位。 */
	FString ActorHierarchyPath(const AActor* Actor)
	{
		TArray<FString> Names;
		for (const AActor* Current = Actor; Current; Current = Current->GetAttachParentActor())
		{
			Names.Insert(Current->GetActorNameOrLabel(), 0);
		}
		return FString::Join(Names, TEXT("/"));
	}
}

UUIItem* UWuwaUiPanel::GetItem(int32 Index) const { return GetComponentAt<UUIItem>(Index); }
UUIText* UWuwaUiPanel::GetText(int32 Index) const { return GetComponentAt<UUIText>(Index); }
UUISprite* UWuwaUiPanel::GetSprite(int32 Index) const { return GetComponentAt<UUISprite>(Index); }
UUIButtonComponent* UWuwaUiPanel::GetButton(int32 Index) const { return GetComponentAt<UUIButtonComponent>(Index); }

AActor* UWuwaUiPanel::GetRegisteredActor(int32 Index) const
{
	const UActorComponent* Component = RegisteredComponents.FindRef(Index);
	return Component ? Component->GetOwner() : nullptr;
}

void UWuwaUiPanel::RegisterComponents()
{
	RegisteredComponents.Reset();
	if (ComponentRegisterInfos.IsEmpty())
	{
		return;
	}

	const UWuwaUiComponentsRegistry* Registry = RootActor->FindComponentByClass<UWuwaUiComponentsRegistry>();
	if (!Registry)
	{
		UE_LOG(LogWuwaUI, Error, TEXT("%s: prefab root '%s' has no WuwaUiComponentsRegistry, %d components not registered."),
			*GetClass()->GetName(), *ActorHierarchyPath(RootActor), ComponentRegisterInfos.Num());
		return;
	}

	// 缺的只报错、跳过，界面照常打开（和鸣潮一样）：一个控件没配好不该让整个界面打不开。
	for (const FWuwaUiComponentInfo& Info : ComponentRegisterInfos)
	{
		const UClass* Type = Info.Type.Get();
		AActor* Actor = Registry->GetActorAt(Info.Index);
		if (!Actor)
		{
			UE_LOG(LogWuwaUI, Error, TEXT("%s: registry on '%s' has no actor at index %d (size %d), wanted %s."),
				*GetClass()->GetName(), *ActorHierarchyPath(RootActor), Info.Index, Registry->Num(), *GetNameSafe(Type));
			continue;
		}

		UActorComponent* Component = Type ? Actor->GetComponentByClass(Info.Type) : nullptr;
		if (!Component)
		{
			UE_LOG(LogWuwaUI, Error, TEXT("%s: index %d '%s' is missing component %s."),
				*GetClass()->GetName(), Info.Index, *ActorHierarchyPath(Actor), *GetNameSafe(Type));
			continue;
		}

		RegisteredComponents.Add(Info.Index, Component);
	}
}

void UWuwaUiPanel::BindClickEvents()
{
	for (const FWuwaUiButtonBind& Bind : BtnBindInfo)
	{
		UUIButtonComponent* Button = GetButton(Bind.Index);
		if (!Button)
		{
			UE_LOG(LogWuwaUI, Error, TEXT("%s: BtnBindInfo index %d is not registered as a UIButtonComponent in ComponentRegisterInfos."),
				*GetClass()->GetName(), Bind.Index);
			continue;
		}

		// WeakLambda：界面对象没了，按钮再被点也不会调到已经失效的 this。
		const FDelegateHandle Handle = Button->RegisterClickEvent(FSimpleDelegate::CreateWeakLambda(this, [OnClick = Bind.OnClick]()
		{
			if (OnClick) { OnClick(); }
		}));
		ClickHandles.Emplace(Button, Handle);
	}
}

void UWuwaUiPanel::UnbindClickEvents()
{
	for (const TPair<TWeakObjectPtr<UUIButtonComponent>, FDelegateHandle>& Pair : ClickHandles)
	{
		if (UUIButtonComponent* Button = Pair.Key.Get())
		{
			Button->UnregisterClickEvent(Pair.Value);
		}
	}
	ClickHandles.Reset();
	RegisteredComponents.Reset();
}

// —— MVC ——

void UWuwaUiPanel::StopListening()
{
	if (!bListening)
	{
		return;
	}
	bListening = false;
	OnRemoveEventListener();
	if (UWuwaEventSubsystem* Events = GetEvents())
	{
		Events->RemoveAll(this);
	}
}

FDelegateHandle UWuwaUiPanel::ListenEvent(FGameplayTag Event, FWuwaEventHandler Handler)
{
	UWuwaEventSubsystem* Events = GetEvents();
	return Events ? Events->Add(Event, this, MoveTemp(Handler)) : FDelegateHandle();
}

UWuwaEventSubsystem* UWuwaUiPanel::GetEvents() const
{
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const UGameInstance* GameInstance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UWuwaEventSubsystem>() : nullptr;
}

ULocalPlayer* UWuwaUiPanel::GetOwningLocalPlayer() const
{
	// 顶层界面的 Outer 是 UiManager；子组件的 Outer 一路往上也会到它。
	for (const UObject* Outer = GetOuter(); Outer; Outer = Outer->GetOuter())
	{
		if (const UWuwaUiManager* Manager = Cast<UWuwaUiManager>(Outer))
		{
			return Manager->GetLocalPlayer();
		}
	}
	return nullptr;
}

ULocalPlayerSubsystem* UWuwaUiPanel::GetLocalPlayerSubsystem(TSubclassOf<ULocalPlayerSubsystem> SubsystemClass) const
{
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetSubsystemBase(SubsystemClass) : nullptr;
}

// —— 打开/关闭动画 ——

void UWuwaUiPanel::PlayShowAnimation(TFunction<void()> Done)
{
	if (UWuwaUiViewAnimation* Animation = IsValid(RootActor) ? RootActor->FindComponentByClass<UWuwaUiViewAnimation>() : nullptr)
	{
		Animation->PlayShow(MoveTemp(Done));
		return;
	}
	Done();
}

void UWuwaUiPanel::PlayHideAnimation(TFunction<void()> Done)
{
	if (UWuwaUiViewAnimation* Animation = IsValid(RootActor) ? RootActor->FindComponentByClass<UWuwaUiViewAnimation>() : nullptr)
	{
		Animation->PlayHide(MoveTemp(Done));
		return;
	}
	Done();
}

void UWuwaUiPanel::StopAnimations()
{
	if (UWuwaUiViewAnimation* Animation = IsValid(RootActor) ? RootActor->FindComponentByClass<UWuwaUiViewAnimation>() : nullptr)
	{
		Animation->Stop();
	}
}
