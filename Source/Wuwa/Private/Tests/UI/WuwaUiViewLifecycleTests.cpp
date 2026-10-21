#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Core/ActorComponent/UIItem.h"
#include "Core/ActorComponent/UIText.h"
#include "Event/Interface/LGUIPointerClickInterface.h"
#include "Event/LGUIEventSystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/UIButtonComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"
#include "Game/LGUI/Container/WuwaUiQueueContainer.h"
#include "Game/LGUI/Container/WuwaUiStackContainer.h"
#include "Game/LGUI/Core/WuwaUiManager.h"
#include "Game/LGUI/Layer/WuwaUiLayerRoot.h"
#include "Game/LGUI/Mvc/WuwaEventSubsystem.h"
#include "Game/LGUI/Component/WuwaUiViewAnimation.h"
#include "Game/LGUI/Test/WuwaTestPopView.h"
#include "Game/LGUI/Test/WuwaTestUiCameraHandler.h"
#include "Game/Module/Mail/WuwaMailBoxView.h"
#include "Game/Module/Mail/WuwaMailController.h"
#include "Game/Module/Mail/WuwaMailEvents.h"
#include "Game/Module/Mail/WuwaMailModel.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

// UI 框架各步的验收：生命周期、注册表和按钮、分层容器、打开前检查、切地图清理、MVC（事件、邮件示例模块）、打开效果（动画、音效、时间缩放、模糊、UI 相机、全屏）。
namespace WuwaUiViewLifecycleTests
{
	const FName TestView(TEXT("TestPopView"));

	struct FState
	{
		TWeakObjectPtr<UWuwaUiManager> Manager;
		TWeakObjectPtr<UWuwaTestPopView> First;
		TWeakObjectPtr<UWuwaTestPopView> Second;
		TWeakObjectPtr<AActor> FirstActor;
		/** 容器测试用的若干界面。 */
		TArray<TWeakObjectPtr<UWuwaUiView>> V;
		TWeakObjectPtr<UWorld> OldWorld;
		/** 切地图会触发 GC：要在之后检查的旧界面对象，用强引用留住。 */
		TArray<TStrongObjectPtr<UObject>> KeepAlive;
		bool bFailed = false;
	};

	UWorld* FindPlayWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) return Context.World();
		}
		return nullptr;
	}

	/** 某一层下面现有的界面 Actor 数量（多了说明有残留）。 */
	int32 CountLayerChildren(EWuwaUiLayer Layer)
	{
		UWorld* World = FindPlayWorld();
		if (!World) return -1;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (const UWuwaUiLayerRoot* LayerRoot = It->FindComponentByClass<UWuwaUiLayerRoot>())
			{
				const UUIItem* LayerItem = LayerRoot->GetLayerItem(Layer);
				return LayerItem ? LayerItem->GetAttachUIChildren().Num() : -1;
			}
		}
		return 0;
	}

	int32 CountPopChildren() { return CountLayerChildren(EWuwaUiLayer::Pop); }

	TArray<FName> Hooks(std::initializer_list<const TCHAR*> Names)
	{
		TArray<FName> Result;
		for (const TCHAR* Name : Names) Result.Add(Name);
		return Result;
	}

	FString Join(const TArray<FName>& Names)
	{
		return FString::JoinBy(Names, TEXT(","), [](FName Name) { return Name.ToString(); });
	}

	/** 先执行 Enter 一次，然后每帧轮询 Done，超时记错误；之前的步骤失败就直接跳过。 */
	class FStep final : public IAutomationLatentCommand
	{
	public:
		FStep(FAutomationTestBase* InTest, TSharedRef<FState> InState, FString InName, TFunction<void()> InEnter, TFunction<bool()> InDone)
			: Test(InTest), State(InState), Name(MoveTemp(InName)), Enter(MoveTemp(InEnter)), Done(MoveTemp(InDone)) {}

		virtual bool Update() override
		{
			if (State->bFailed) return true;
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0)
			{
				StartedAt = Now;
				if (Enter) Enter();
				if (State->bFailed) return true;
			}
			if (Done()) return true;
			if (Now - StartedAt > 15.0)
			{
				Test->AddError(FString::Printf(TEXT("Timed out: %s"), *Name));
				State->bFailed = true;
				return true;
			}
			return false;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FState> State;
		FString Name;
		TFunction<void()> Enter;
		TFunction<bool()> Done;
		double StartedAt = -1.0;
	};

	bool IsIn(const TWeakObjectPtr<UWuwaTestPopView>& View, EWuwaUiState Expected)
	{
		return View.IsValid() && View->GetState() == Expected;
	}

	bool IsIn(const TWeakObjectPtr<UWuwaUiView>& View, EWuwaUiState Expected)
	{
		return View.IsValid() && View->GetState() == Expected;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaUiViewLifecycleTest, "Wuwa.UI.ViewLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaUiViewLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace WuwaUiViewLifecycleTests;

	if (!AutomationOpenMap(TEXT("/Game/Map/LGUI")))
	{
		AddError(TEXT("Could not open /Game/Map/LGUI."));
		return false;
	}

	TSharedRef<FState> State = MakeShared<FState>();
	FAutomationTestBase* Test = this;
	auto Step = [this, State](const TCHAR* Name, TFunction<void()> Enter, TFunction<bool()> Done)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FStep(this, State, Name, MoveTemp(Enter), MoveTemp(Done)));
	};

	Step(TEXT("wait for UiManager"), nullptr, [State]()
	{
		UWorld* World = FindPlayWorld();
		APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
		State->Manager = Player ? UWuwaUiManager::Get(Player) : nullptr;
		return State->Manager.IsValid();
	});

	// 1. 正常打开：Create → Start → BeforeShow → AfterShow，界面显示出来。
	Step(TEXT("open"), [Test, State]()
	{
		State->First = Cast<UWuwaTestPopView>(State->Manager->OpenView(TestView));
		if (!Test->TestTrue(TEXT("OpenView returns a UWuwaTestPopView"), State->First.IsValid())) State->bFailed = true;
	}, [State]() { return IsIn(State->First, EWuwaUiState::Shown); });

	Step(TEXT("check open"), [Test, State]()
	{
		Test->TestEqual(TEXT("open hooks"), Join(State->First->GetHookLog()), Join(Hooks({TEXT("OnCreate"), TEXT("OnStart"), TEXT("OnBeforeShow"), TEXT("OnAfterShow")})));
		const UUIItem* RootItem = State->First->GetRootItem();
		Test->TestTrue(TEXT("root item is active after Show"), RootItem && RootItem->GetIsUIActiveSelf());
		Test->TestEqual(TEXT("one view under Pop layer"), CountPopChildren(), 1);
		Test->TestTrue(TEXT("IsViewOpen"), State->Manager->IsViewOpen(TestView));
		Test->TestNotNull(TEXT("an LGUI EventSystem exists in the world"), ULGUIEventSystem::GetLGUIEventSystemInstance(FindPlayWorld()));
		State->FirstActor = State->First->GetRootActor();
	}, []() { return true; });

	// 2. 正常关闭：BeforeHide → AfterHide → BeforeDestroy，Actor 被销毁。
	Step(TEXT("close"), [Test, State]()
	{
		Test->TestTrue(TEXT("CloseView"), State->Manager->CloseView(TestView));
		Test->TestFalse(TEXT("IsViewOpen right after CloseView"), State->Manager->IsViewOpen(TestView));
	}, [State]() { return IsIn(State->First, EWuwaUiState::Destroyed) && CountPopChildren() == 0; });

	Step(TEXT("check close"), [Test, State]()
	{
		Test->TestEqual(TEXT("full hooks"), Join(State->First->GetHookLog()), Join(Hooks({TEXT("OnCreate"), TEXT("OnStart"), TEXT("OnBeforeShow"), TEXT("OnAfterShow"),
			TEXT("OnBeforeHide"), TEXT("OnAfterHide"), TEXT("OnBeforeDestroy")})));
		Test->TestTrue(TEXT("root actor destroyed"), !State->FirstActor.IsValid() || State->FirstActor->IsActorBeingDestroyed());
	}, []() { return true; });

	// 3. 同一帧 Open → Close → Open：旧对象排队走完全程，新对象最终显示。
	Step(TEXT("stress"), [Test, State]()
	{
		State->First = Cast<UWuwaTestPopView>(State->Manager->OpenView(TestView));
		State->Manager->CloseView(TestView);
		State->Second = Cast<UWuwaTestPopView>(State->Manager->OpenView(TestView));
		Test->TestTrue(TEXT("second open creates a new view object"), State->First.IsValid() && State->Second.IsValid() && State->First != State->Second);
		Test->TestTrue(TEXT("GetView returns the new one"), State->Manager->GetView(TestView) == State->Second.Get());
	}, [State]()
	{
		return IsIn(State->First, EWuwaUiState::Destroyed) && IsIn(State->Second, EWuwaUiState::Shown) && CountPopChildren() == 1;
	});

	Step(TEXT("check stress"), [Test, State]()
	{
		Test->TestEqual(TEXT("old view ran every step in order"), Join(State->First->GetHookLog()), Join(Hooks({TEXT("OnCreate"), TEXT("OnStart"), TEXT("OnBeforeShow"), TEXT("OnAfterShow"),
			TEXT("OnBeforeHide"), TEXT("OnAfterHide"), TEXT("OnBeforeDestroy")})));
		Test->TestTrue(TEXT("still open"), State->Manager->IsViewOpen(TestView));
		State->Manager->CloseView(TestView);
	}, [State]() { return IsIn(State->Second, EWuwaUiState::Destroyed) && CountPopChildren() == 0; });

	// 4. 加载中关闭：Destroy 排在 Create 后面，不崩溃、不残留。
	Step(TEXT("close while loading"), [Test, State]()
	{
		State->First = Cast<UWuwaTestPopView>(State->Manager->OpenView(TestView));
		Test->TestTrue(TEXT("still Creating right after OpenView"), IsIn(State->First, EWuwaUiState::Creating));
		State->Manager->CloseView(TestView);
	}, [State]() { return IsIn(State->First, EWuwaUiState::Destroyed) && CountPopChildren() == 0; });

	Step(TEXT("check close while loading"), [Test, State]()
	{
		Test->TestNull(TEXT("no root actor left"), State->First->GetRootActor());
		Test->TestFalse(TEXT("not open"), State->Manager->IsViewOpen(TestView));
	}, []() { return true; });

	// 5. 注册表 + 按钮：0 号按钮绑定了 CloseMe，1 号文字在 OnStart 里写入打开次数。
	Step(TEXT("open for click"), [Test, State]()
	{
		State->First = Cast<UWuwaTestPopView>(State->Manager->OpenView(TestView));
	}, [State]() { return IsIn(State->First, EWuwaUiState::Shown); });

	Step(TEXT("click close"), [Test, State]()
	{
		UUIText* CountText = State->First->GetCountText();
		UUIButtonComponent* Button = State->First->GetCloseButton();
		if (!Test->TestNotNull(TEXT("1 is registered as UIText"), CountText) || !Test->TestNotNull(TEXT("0 is registered as UIButtonComponent"), Button))
		{
			State->bFailed = true;
			return;
		}
		Test->TestTrue(TEXT("OnStart wrote the open count"), CountText->GetText().ToString().StartsWith(TEXT("Opened ")));
		// 走按钮自己的点击入口（EventSystem 点中按钮时调的就是它）。
		ILGUIPointerClickInterface::Execute_OnPointerClick(Button, nullptr);
		Test->TestEqual(TEXT("click callback ran once"), State->First->GetCloseClickCount(), 1);
	}, [State]() { return IsIn(State->First, EWuwaUiState::Destroyed) && CountPopChildren() == 0; });

	Step(TEXT("check click close"), [Test, State]()
	{
		Test->TestFalse(TEXT("closed by its button"), State->Manager->IsViewOpen(TestView));
		Test->TestNull(TEXT("registered components are released"), State->First->GetCloseButton());
	}, []() { return true; });

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// 地图里没摆 EventSystem 时，第一次打开界面由 UiManager 按配置生成一个。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaUiEventSystemSpawnTest, "Wuwa.UI.EventSystemSpawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaUiEventSystemSpawnTest::RunTest(const FString& Parameters)
{
	using namespace WuwaUiViewLifecycleTests;

	if (!AutomationOpenMap(TEXT("/Game/Map/Wuwa")))
	{
		AddError(TEXT("Could not open /Game/Map/Wuwa."));
		return false;
	}

	TSharedRef<FState> State = MakeShared<FState>();
	FAutomationTestBase* Test = this;
	auto Step = [this, State](const TCHAR* Name, TFunction<void()> Enter, TFunction<bool()> Done)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FStep(this, State, Name, MoveTemp(Enter), MoveTemp(Done)));
	};

	Step(TEXT("wait for UiManager"), nullptr, [State]()
	{
		UWorld* World = FindPlayWorld();
		APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
		State->Manager = Player ? UWuwaUiManager::Get(Player) : nullptr;
		return State->Manager.IsValid();
	});

	Step(TEXT("open without a placed EventSystem"), [Test, State]()
	{
		if (ULGUIEventSystem::GetLGUIEventSystemInstance(FindPlayWorld()))
		{
			Test->AddWarning(TEXT("/Game/Map/Wuwa already has an LGUI EventSystem; the spawn path is not exercised."));
		}
		State->First = Cast<UWuwaTestPopView>(State->Manager->OpenView(TestView));
	}, [State]() { return IsIn(State->First, EWuwaUiState::Shown); });

	Step(TEXT("check EventSystem"), [Test, State]()
	{
		const ULGUIEventSystem* EventSystem = ULGUIEventSystem::GetLGUIEventSystemInstance(FindPlayWorld());
		if (Test->TestNotNull(TEXT("UiManager provided an LGUI EventSystem"), EventSystem))
		{
			Test->TestTrue(TEXT("it is the configured preset"), EventSystem->GetOwner()->GetClass()->GetName().StartsWith(TEXT("PresetEventSystemActor")));
		}
		State->Manager->CloseView(TestView);
	}, [State]() { return IsIn(State->First, EWuwaUiState::Destroyed); });

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}


// 第 3 步：各层容器的进出规则。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaUiContainersTest, "Wuwa.UI.Containers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaUiContainersTest::RunTest(const FString& Parameters)
{
	using namespace WuwaUiViewLifecycleTests;
	static const FName Normal1(TEXT("TestNormalView"));
	static const FName Normal2(TEXT("TestNormalView2"));
	static const FName FloatA(TEXT("TestFloatA"));
	static const FName FloatB(TEXT("TestFloatB"));
	static const FName FloatC(TEXT("TestFloatC"));

	if (!AutomationOpenMap(TEXT("/Game/Map/LGUI")))
	{
		AddError(TEXT("Could not open /Game/Map/LGUI."));
		return false;
	}

	TSharedRef<FState> State = MakeShared<FState>();
	FAutomationTestBase* Test = this;
	auto Step = [this, State](const TCHAR* Name, TFunction<void()> Enter, TFunction<bool()> Done)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FStep(this, State, Name, MoveTemp(Enter), MoveTemp(Done)));
	};
	auto Open = [State](FName Name) { State->V.Add(State->Manager->OpenView(Name)); };
	auto Stack = [State]() { return Cast<UWuwaUiStackContainer>(State->Manager->GetContainer(EWuwaUiLayer::Normal)); };
	auto Queue = [State]() { return Cast<UWuwaUiQueueContainer>(State->Manager->GetContainer(EWuwaUiLayer::Float)); };
	auto Reset = [State]() { State->V.Reset(); };

	Step(TEXT("wait for UiManager"), nullptr, [State]()
	{
		UWorld* World = FindPlayWorld();
		APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
		State->Manager = Player ? UWuwaUiManager::Get(Player) : nullptr;
		return State->Manager.IsValid();
	});

	// 测试地图的 BP_LGUIPlayerController 开局会打开 TestNormalView：先关掉，从空栈开始。
	Step(TEXT("start from an empty Normal layer"), [State]()
	{
		if (State->Manager->IsViewOpen(Normal1))
		{
			State->Manager->CloseView(Normal1);
		}
	}, [Stack]() { return Stack()->Num() == 0 && Stack()->NumPending() == 0 && CountLayerChildren(EWuwaUiLayer::Normal) == 0; });

	// —— 栈 ——
	Step(TEXT("stack: open 1"), [Open]() { Open(Normal1); }, [State]() { return IsIn(State->V[0], EWuwaUiState::Shown); });

	Step(TEXT("stack: open 2 covers 1"), [Open]() { Open(Normal2); }, [State]()
	{
		return IsIn(State->V[1], EWuwaUiState::Shown) && IsIn(State->V[0], EWuwaUiState::Hidden);
	});

	Step(TEXT("stack: check covered"), [Test, State, Stack]()
	{
		Test->TestTrue(TEXT("covered view still counts as open"), State->Manager->IsViewOpen(Normal1));
		Test->TestFalse(TEXT("covered view is not shown"), State->Manager->IsViewShown(Normal1));
		Test->TestTrue(TEXT("top is view 2"), Stack()->Peek() == State->V[1].Get());
		Test->TestEqual(TEXT("both actors exist under Normal"), CountLayerChildren(EWuwaUiLayer::Normal), 2);
		State->Manager->CloseView(Normal2);
	}, [State]() { return IsIn(State->V[1], EWuwaUiState::Destroyed) && IsIn(State->V[0], EWuwaUiState::Shown); });

	Step(TEXT("stack: open 2 again"), [Test, Open]()
	{
		Test->TestEqual(TEXT("closing the top brought 1 back, 2 is gone"), CountLayerChildren(EWuwaUiLayer::Normal), 1);
		Open(Normal2);   // V[2]
	}, [State]() { return IsIn(State->V[2], EWuwaUiState::Shown) && IsIn(State->V[0], EWuwaUiState::Hidden); });

	Step(TEXT("stack: close the middle one"), [State]()
	{
		State->Manager->CloseView(Normal1);
	}, [State]() { return IsIn(State->V[0], EWuwaUiState::Destroyed); });

	Step(TEXT("stack: check middle"), [Test, State, Stack]()
	{
		Test->TestTrue(TEXT("top still view 2 and shown"), Stack()->Peek() == State->V[2].Get() && IsIn(State->V[2], EWuwaUiState::Shown));
		Test->TestEqual(TEXT("stack size 1"), Stack()->Num(), 1);
		State->Manager->CloseView(Normal2);
	}, [State, Stack]() { return IsIn(State->V[2], EWuwaUiState::Destroyed) && Stack()->Num() == 0 && CountLayerChildren(EWuwaUiLayer::Normal) == 0; });

	// 同一帧：开 1、开 2、关 2 → 2 的打开还在待办里就被抵消，1 正常显示。
	Step(TEXT("stack: open 1, open 2, close 2 in one frame"), [Test, State, Open, Reset, Stack]()
	{
		Reset();
		Open(Normal1);
		Open(Normal2);
		Test->TestEqual(TEXT("open 2 waits while the stack is busy"), Stack()->NumPending(), 1);
		State->Manager->CloseView(Normal2);
		Test->TestEqual(TEXT("close 2 cancels the pending open"), Stack()->NumPending(), 0);
	}, [State, Stack]()
	{
		return IsIn(State->V[0], EWuwaUiState::Shown) && IsIn(State->V[1], EWuwaUiState::Destroyed) && Stack()->Num() == 1;
	});

	Step(TEXT("stack: check cancel"), [Test, State]()
	{
		Test->TestNull(TEXT("view 2 never created an actor"), State->V[1]->GetRootActor());
		Test->TestEqual(TEXT("only view 1 under Normal"), CountLayerChildren(EWuwaUiLayer::Normal), 1);
		State->Manager->CloseView(Normal1);
	}, [State]() { return IsIn(State->V[0], EWuwaUiState::Destroyed) && CountLayerChildren(EWuwaUiLayer::Normal) == 0; });

	// —— 列表：同一个界面开两个实例，按 Id 只关一个 ——
	Step(TEXT("list: two instances"), [Open, Reset]()
	{
		Reset();
		Open(TestView);
		Open(TestView);
	}, [State]() { return IsIn(State->V[0], EWuwaUiState::Shown) && IsIn(State->V[1], EWuwaUiState::Shown); });

	Step(TEXT("list: close one by id"), [Test, State]()
	{
		Test->TestTrue(TEXT("two different objects"), State->V[0] != State->V[1]);
		Test->TestEqual(TEXT("two actors under Pop"), CountPopChildren(), 2);
		State->Manager->CloseViewById(State->V[0]->GetViewId());
	}, [State]() { return IsIn(State->V[0], EWuwaUiState::Destroyed); });

	Step(TEXT("list: close the rest by name"), [Test, State]()
	{
		Test->TestTrue(TEXT("the other one is still shown"), IsIn(State->V[1], EWuwaUiState::Shown));
		State->Manager->CloseView(TestView);
	}, [State]() { return IsIn(State->V[1], EWuwaUiState::Destroyed) && CountPopChildren() == 0; });

	// —— 队列：A(1)、B(3)、C(2) 同一帧打开 → A 先显示（当时空着），之后按优先级 B、C ——
	Step(TEXT("queue: open A, B, C"), [Test, Open, Reset, Queue]()
	{
		Reset();
		Open(FloatA);
		Open(FloatB);
		Open(FloatC);
		Test->TestEqual(TEXT("B and C wait"), Queue()->NumWaiting(), 2);
	}, [State]() { return IsIn(State->V[0], EWuwaUiState::Shown); });

	Step(TEXT("queue: close A, B (highest priority) shows"), [Test, State]()
	{
		Test->TestTrue(TEXT("waiting views are not created yet"), IsIn(State->V[1], EWuwaUiState::None) && IsIn(State->V[2], EWuwaUiState::None));
		Test->TestTrue(TEXT("waiting views count as open"), State->Manager->IsViewOpen(FloatB));
		State->Manager->CloseView(FloatA);
	}, [State]() { return IsIn(State->V[0], EWuwaUiState::Destroyed) && IsIn(State->V[1], EWuwaUiState::Shown); });

	Step(TEXT("queue: close B, C shows"), [Test, State]()
	{
		Test->TestTrue(TEXT("C still waiting while B shows"), IsIn(State->V[2], EWuwaUiState::None));
		Test->TestEqual(TEXT("one actor under Float"), CountLayerChildren(EWuwaUiLayer::Float), 1);
		State->Manager->CloseView(FloatB);
	}, [State]() { return IsIn(State->V[1], EWuwaUiState::Destroyed) && IsIn(State->V[2], EWuwaUiState::Shown); });

	Step(TEXT("queue: close C"), [State]()
	{
		State->Manager->CloseView(FloatC);
	}, [State, Queue]() { return IsIn(State->V[2], EWuwaUiState::Destroyed) && !Queue()->GetCurrent() && CountLayerChildren(EWuwaUiLayer::Float) == 0; });

	// 排队中的界面直接关掉：从队列里移除，不会被创建。
	Step(TEXT("queue: close a waiting view"), [Test, State, Open, Reset, Queue]()
	{
		Reset();
		Open(FloatA);
		Open(FloatB);
		State->Manager->CloseView(FloatB);
		Test->TestEqual(TEXT("B removed from the queue"), Queue()->NumWaiting(), 0);
	}, [State]() { return IsIn(State->V[0], EWuwaUiState::Shown) && IsIn(State->V[1], EWuwaUiState::Destroyed); });

	Step(TEXT("queue: cleanup"), [Test, State]()
	{
		Test->TestNull(TEXT("B never created an actor"), State->V[1]->GetRootActor());
		State->Manager->CloseView(FloatA);
	}, [State]() { return IsIn(State->V[0], EWuwaUiState::Destroyed) && CountLayerChildren(EWuwaUiLayer::Float) == 0; });

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}


// 第 4 步：打开前检查（遮挡、全局锁、业务检查函数）。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaUiOpenChecksTest, "Wuwa.UI.OpenChecks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaUiOpenChecksTest::RunTest(const FString& Parameters)
{
	using namespace WuwaUiViewLifecycleTests;
	static const FName Normal1(TEXT("TestNormalView"));
	static const FName Normal2(TEXT("TestNormalView2"));
	static const FName Obstructed(TEXT("TestObstructedPop"));

	if (!AutomationOpenMap(TEXT("/Game/Map/LGUI")))
	{
		AddError(TEXT("Could not open /Game/Map/LGUI."));
		return false;
	}

	TSharedRef<FState> State = MakeShared<FState>();
	FAutomationTestBase* Test = this;
	auto Step = [this, State](const TCHAR* Name, TFunction<void()> Enter, TFunction<bool()> Done)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FStep(this, State, Name, MoveTemp(Enter), MoveTemp(Done)));
	};

	Step(TEXT("wait for UiManager"), nullptr, [State]()
	{
		UWorld* World = FindPlayWorld();
		APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
		State->Manager = Player ? UWuwaUiManager::Get(Player) : nullptr;
		return State->Manager.IsValid();
	});

	// 测试地图的 BP_LGUIPlayerController 开局会打开 TestNormalView：先关掉。
	Step(TEXT("start from an empty Normal layer"), [State]()
	{
		if (State->Manager->IsViewOpen(Normal1)) State->Manager->CloseView(Normal1);
	}, []() { return CountLayerChildren(EWuwaUiLayer::Normal) == 0; });

	// —— 遮挡：TestObstructedPop 配了 BeObstructViews = [TestNormalView2] ——
	Step(TEXT("obstruct: show the obstructer"), [State]()
	{
		State->V.Add(State->Manager->OpenView(Normal2));
	}, [State]() { return IsIn(State->V[0], EWuwaUiState::Shown); });

	Step(TEXT("obstruct: rejected while it is shown"), [Test, State]()
	{
		Test->TestFalse(TEXT("CanOpenView is false"), State->Manager->CanOpenView(Obstructed));
		Test->TestNull(TEXT("OpenView returns null"), State->Manager->OpenView(Obstructed));
		State->Manager->CloseView(Normal2);
	}, [State]() { return IsIn(State->V[0], EWuwaUiState::Destroyed); });

	Step(TEXT("obstruct: allowed after it closes"), [Test, State]()
	{
		State->V.Add(State->Manager->OpenView(Obstructed));
		Test->TestNotNull(TEXT("opens once the obstructer is gone"), State->V[1].Get());
	}, [State]() { return IsIn(State->V[1], EWuwaUiState::Shown); });

	// —— 全局锁 ——
	Step(TEXT("lock"), [Test, State]()
	{
		State->Manager->CloseView(Obstructed);
		State->Manager->LockOpen();
		Test->TestNull(TEXT("nothing opens while locked"), State->Manager->OpenView(Normal2));
		State->Manager->UnlockOpen();
		Test->TestTrue(TEXT("can open after unlock"), State->Manager->CanOpenView(Normal2));
	}, [State]() { return IsIn(State->V[1], EWuwaUiState::Destroyed); });

	// —— 业务检查函数：所有界面的、单个界面的 ——
	Step(TEXT("checks"), [Test, State]()
	{
		UWuwaUiManager* Manager = State->Manager.Get();
		const FDelegateHandle Global = Manager->AddOpenViewCheck(NAME_None, [](FName) { return false; });
		Test->TestNull(TEXT("a global check rejects every view"), Manager->OpenView(Normal2));
		Test->TestFalse(TEXT("including other views"), Manager->CanOpenView(TEXT("TestFloatA")));
		Manager->RemoveOpenViewCheck(NAME_None, Global);

		TArray<FName> Asked;
		const FDelegateHandle Single = Manager->AddOpenViewCheck(Normal2, [&Asked](FName ViewName) { Asked.Add(ViewName); return false; });
		Test->TestNull(TEXT("a view check rejects that view"), Manager->OpenView(Normal2));
		Test->TestTrue(TEXT("it does not affect other views"), Manager->CanOpenView(TEXT("TestFloatA")));
		Test->TestEqual(TEXT("the check receives the view name"), Asked.Num() > 0 ? Asked[0] : NAME_None, Normal2);
		Manager->RemoveOpenViewCheck(Normal2, Single);
		Test->TestTrue(TEXT("removed checks no longer apply"), Manager->CanOpenView(Normal2));
	}, []() { return true; });

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// 第 4 步：切地图时非常驻界面被销毁（钩子照常调用），常驻界面在新地图里重新打开。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaUiLevelChangeTest, "Wuwa.UI.LevelChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaUiLevelChangeTest::RunTest(const FString& Parameters)
{
	using namespace WuwaUiViewLifecycleTests;
	static const FName Permanent(TEXT("TestPermanentHud"));

	if (!AutomationOpenMap(TEXT("/Game/Map/LGUI")))
	{
		AddError(TEXT("Could not open /Game/Map/LGUI."));
		return false;
	}

	TSharedRef<FState> State = MakeShared<FState>();
	FAutomationTestBase* Test = this;
	auto Step = [this, State](const TCHAR* Name, TFunction<void()> Enter, TFunction<bool()> Done)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FStep(this, State, Name, MoveTemp(Enter), MoveTemp(Done)));
	};

	Step(TEXT("wait for UiManager"), nullptr, [State]()
	{
		UWorld* World = FindPlayWorld();
		APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
		State->Manager = Player ? UWuwaUiManager::Get(Player) : nullptr;
		return State->Manager.IsValid();
	});

	Step(TEXT("open a permanent and a normal view"), [State]()
	{
		State->V.Add(State->Manager->OpenView(Permanent));
		State->First = Cast<UWuwaTestPopView>(State->Manager->OpenView(TestView));
	}, [State]() { return IsIn(State->V[0], EWuwaUiState::Shown) && IsIn(State->First, EWuwaUiState::Shown); });

	Step(TEXT("change map"), [State]()
	{
		State->OldWorld = FindPlayWorld();
		State->KeepAlive.Emplace(State->First.Get());
		State->KeepAlive.Emplace(State->V[0].Get());
		UGameplayStatics::OpenLevel(State->OldWorld.Get(), FName(TEXT("/Game/Map/LGUI")));
	}, [State]()
	{
		UWorld* World = FindPlayWorld();
		return World && World != State->OldWorld.Get() && State->Manager.IsValid() && State->Manager->IsViewShown(Permanent);
	});

	Step(TEXT("check map change"), [Test, State]()
	{
		Test->TestTrue(TEXT("old normal view was destroyed"), IsIn(State->First, EWuwaUiState::Destroyed));
		Test->TestTrue(TEXT("its OnBeforeDestroy ran"), State->First.IsValid() && State->First->GetHookLog().Contains(FName(TEXT("OnBeforeDestroy"))));
		Test->TestFalse(TEXT("non-permanent view is not reopened"), State->Manager->IsViewOpen(TestView));
		Test->TestTrue(TEXT("old permanent object was destroyed"), IsIn(State->V[0], EWuwaUiState::Destroyed));
		UWuwaUiView* Reopened = State->Manager->GetView(Permanent);
		Test->TestTrue(TEXT("permanent view reopened as a new object in the new world"), Reopened && Reopened != State->V[0].Get()
			&& Reopened->GetRootActor() && Reopened->GetRootActor()->GetWorld() == FindPlayWorld());
		// HUD 层里还可能有项目自己的常驻界面（比如 MainHUD），只检查测试界面自己：只有一个实例，挂在新世界的 HUD 层下。
		TArray<UWuwaUiView*> Instances;
		State->Manager->GetViews(Permanent, Instances);
		Test->TestEqual(TEXT("exactly one permanent test view after the map change"), Instances.Num(), 1);
		Test->TestTrue(TEXT("it sits under the new HUD layer"), Reopened && Reopened->GetRootItem()
			&& Reopened->GetRootItem()->GetAttachParent() == State->Manager->GetLayerItem(EWuwaUiLayer::HUD));
		State->V.Add(Reopened);   // V[1]
		State->Manager->CloseView(Permanent);
	}, [State]() { return State->V.Num() > 1 && IsIn(State->V[1], EWuwaUiState::Destroyed); });

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}


// 第 5 步：事件系统本身（不需要 PIE）。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaUiEventSystemTest, "Wuwa.UI.EventSystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaUiEventSystemTest::RunTest(const FString& Parameters)
{
	// 事件子系统的 Outer 必须是 GameInstance；不调 Initialize，只测 Add/Emit/Remove。
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UWuwaEventSubsystem* Events = NewObject<UWuwaEventSubsystem>(GameInstance);
	const FGameplayTag Changed = WuwaMailEvents::Changed;
	// 任意一个具体的 UObject 当 Owner。
	UObject* Owner = NewObject<UDataTable>(GetTransientPackage());

	int32 Received = 0;
	int32 LastMailId = 0;
	Events->Add(Changed, Owner, [&](const FInstancedStruct& Payload)
	{
		++Received;
		if (const FWuwaMailChangedEvent* Event = Payload.GetPtr<FWuwaMailChangedEvent>())
		{
			LastMailId = Event->MailId;
		}
	});

	FWuwaMailChangedEvent Event;
	Event.MailId = 7;
	Events->Emit(Changed, Event);
	TestEqual(TEXT("handler called once"), Received, 1);
	TestEqual(TEXT("payload delivered"), LastMailId, 7);

	// 回调里移除后面的监听：这一轮里它不能再被调用。
	FDelegateHandle Second;
	int32 SecondCalls = 0;
	Events->Add(Changed, nullptr, [&](const FInstancedStruct&) { Events->Remove(Changed, Second); });
	Second = Events->Add(Changed, nullptr, [&](const FInstancedStruct&) { ++SecondCalls; });
	Events->Emit(Changed);
	TestEqual(TEXT("a listener removed during emit is not called"), SecondCalls, 0);

	Events->RemoveAll(Owner);
	Events->Emit(Changed, Event);
	// 前两次 Emit 都收到了（第二次是无参数的），RemoveAll 之后的这次收不到。
	TestEqual(TEXT("RemoveAll(Owner) stops its handlers"), Received, 2);

	// Owner 被销毁后自动失效。
	UObject* ShortLived = NewObject<UDataTable>(GetTransientPackage());
	int32 ShortLivedCalls = 0;
	Events->Add(Changed, ShortLived, [&](const FInstancedStruct&) { ++ShortLivedCalls; });
	ShortLived->MarkAsGarbage();
	Events->Emit(Changed);
	TestEqual(TEXT("a destroyed owner's handler is skipped"), ShortLivedCalls, 0);
	return true;
}

// 第 5 步：邮件示例模块走完 View → Controller → Model → 事件 → View 的完整循环。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaUiMailModuleTest, "Wuwa.UI.MailModule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaUiMailModuleTest::RunTest(const FString& Parameters)
{
	using namespace WuwaUiViewLifecycleTests;
	static const FName Normal1(TEXT("TestNormalView"));
	static const FName Normal2(TEXT("TestNormalView2"));

	if (!AutomationOpenMap(TEXT("/Game/Map/LGUI")))
	{
		AddError(TEXT("Could not open /Game/Map/LGUI."));
		return false;
	}

	struct FMailState
	{
		TWeakObjectPtr<UWuwaMailController> Controller;
		TWeakObjectPtr<UWuwaMailModel> Model;
		TWeakObjectPtr<UWuwaMailBoxView> Box;
		int32 RefreshesBeforeCover = 0;
	};
	TSharedRef<FState> State = MakeShared<FState>();
	TSharedRef<FMailState> Mail = MakeShared<FMailState>();
	FAutomationTestBase* Test = this;
	auto Step = [this, State](const TCHAR* Name, TFunction<void()> Enter, TFunction<bool()> Done)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FStep(this, State, Name, MoveTemp(Enter), MoveTemp(Done)));
	};
	auto BoxText = [Mail]() { return Mail->Box.IsValid() ? Mail->Box->GetUnreadText() : FString(); };

	Step(TEXT("wait for UiManager and the mail module"), nullptr, [State, Mail]()
	{
		UWorld* World = FindPlayWorld();
		APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
		ULocalPlayer* LocalPlayer = Player ? Player->GetLocalPlayer() : nullptr;
		State->Manager = LocalPlayer ? LocalPlayer->GetSubsystem<UWuwaUiManager>() : nullptr;
		Mail->Controller = LocalPlayer ? LocalPlayer->GetSubsystem<UWuwaMailController>() : nullptr;
		Mail->Model = LocalPlayer ? LocalPlayer->GetSubsystem<UWuwaMailModel>() : nullptr;
		return State->Manager.IsValid() && Mail->Controller.IsValid() && Mail->Model.IsValid();
	});

	Step(TEXT("start from an empty Normal layer"), [State]()
	{
		if (State->Manager->IsViewOpen(Normal1)) State->Manager->CloseView(Normal1);
	}, []() { return CountLayerChildren(EWuwaUiLayer::Normal) == 0; });

	// 打开：OnBeforeShow 从 Model 读数据。
	Step(TEXT("open the mailbox"), [Test, Mail]()
	{
		Test->TestEqual(TEXT("sample data has 3 unread"), Mail->Model->GetUnreadCount(), 3);
		Mail->Box = Cast<UWuwaMailBoxView>(Mail->Controller->OpenMailBox());
	}, [Mail]() { return Mail->Box.IsValid() && Mail->Box->GetState() == EWuwaUiState::Shown; });

	// 点"读一封"：View → Controller →（延迟）→ Model → 事件 → View 刷新。
	Step(TEXT("click read"), [Test, Mail, BoxText]()
	{
		Test->TestEqual(TEXT("shows the model's unread count"), BoxText(), FString(TEXT("Unread: 3")));
		ILGUIPointerClickInterface::Execute_OnPointerClick(Mail->Box->GetReadButton(), nullptr);
		Test->TestTrue(TEXT("the request is in flight, nothing changed yet"), Mail->Controller->IsRequestInFlight() && Mail->Model->GetUnreadCount() == 3);
	}, [BoxText]() { return BoxText() == TEXT("Unread: 2"); });

	// 被盖住：不监听事件，后台读一封不刷新；重新显示时从 Model 读到最新值。
	Step(TEXT("cover the mailbox"), [State]()
	{
		State->V.Add(State->Manager->OpenView(Normal2));
	}, [State, Mail]() { return IsIn(State->V[0], EWuwaUiState::Shown) && Mail->Box->GetState() == EWuwaUiState::Hidden; });

	Step(TEXT("read while covered"), [Test, State, Mail]()
	{
		UWuwaEventSubsystem* Events = UWuwaEventSubsystem::Get(FindPlayWorld());
		Test->TestEqual(TEXT("a hidden view does not listen"), Events ? Events->NumListeners(WuwaMailEvents::Changed) : -1, 0);
		Mail->RefreshesBeforeCover = Mail->Box->GetRefreshCount();
		Mail->Controller->RequestReadOne();
	}, [Mail]() { return Mail->Model->GetUnreadCount() == 1 && !Mail->Controller->IsRequestInFlight(); });

	Step(TEXT("uncover"), [Test, State, Mail, BoxText]()
	{
		Test->TestEqual(TEXT("the hidden view was not refreshed"), Mail->Box->GetRefreshCount(), Mail->RefreshesBeforeCover);
		Test->TestEqual(TEXT("still shows the old value"), BoxText(), FString(TEXT("Unread: 2")));
		State->Manager->CloseView(Normal2);
	}, [Mail, BoxText]() { return Mail->Box->GetState() == EWuwaUiState::Shown && BoxText() == TEXT("Unread: 1"); });

	// Controller 注册的打开前检查。
	Step(TEXT("open check"), [Test, Mail]()
	{
		Mail->Box->CloseMe();
	}, [Mail]() { return Mail->Box->GetState() == EWuwaUiState::Destroyed; });

	Step(TEXT("disabled mail cannot open"), [Test, Mail]()
	{
		Mail->Controller->SetMailEnabled(false);
		Test->TestNull(TEXT("mailbox rejected while disabled"), Mail->Controller->OpenMailBox());
		Mail->Controller->SetMailEnabled(true);
		Mail->Box = Cast<UWuwaMailBoxView>(Mail->Controller->OpenMailBox());
		Test->TestNotNull(TEXT("opens again once enabled"), Mail->Box.Get());
	}, [Mail]() { return Mail->Box.IsValid() && Mail->Box->GetState() == EWuwaUiState::Shown; });

	// 切地图：Model 跟着 LocalPlayer 活着，数据还在。
	Step(TEXT("change map"), [State]()
	{
		State->OldWorld = FindPlayWorld();
		UGameplayStatics::OpenLevel(State->OldWorld.Get(), FName(TEXT("/Game/Map/LGUI")));
	}, [State]() { UWorld* World = FindPlayWorld(); return World && World != State->OldWorld.Get() && World->GetFirstPlayerController(); });

	Step(TEXT("check model survived"), [Test, Mail]()
	{
		Test->TestTrue(TEXT("same model object"), Mail->Model.IsValid());
		Test->TestEqual(TEXT("unread count kept across maps"), Mail->Model.IsValid() ? Mail->Model->GetUnreadCount() : -1, 1);
	}, []() { return true; });

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}


// 第 7 步：打开/关闭动画、音效、时间缩放、背景模糊、UI 相机、全屏。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaUiEffectsTest, "Wuwa.UI.Effects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaUiEffectsTest::RunTest(const FString& Parameters)
{
	using namespace WuwaUiViewLifecycleTests;
	static const FName Normal1(TEXT("TestNormalView"));
	static const FName Normal2(TEXT("TestNormalView2"));
	static const FName EffectsView(TEXT("TestEffectsView"));
	static const FName SlowPop(TEXT("TestSlowPop"));

	if (!AutomationOpenMap(TEXT("/Game/Map/LGUI")))
	{
		AddError(TEXT("Could not open /Game/Map/LGUI."));
		return false;
	}

	struct FEffectsState
	{
		TWeakObjectPtr<UWuwaTestUiCameraHandler> Camera;
		float BaseDilation = 1.f;
		bool bSawShowAnimating = false;
		bool bSawOpenSound = false;
		bool bSawHideAnimating = false;
		bool bHudVisibleWhileHiding = false;
		bool bSawCloseSound = false;
	};
	TSharedRef<FState> State = MakeShared<FState>();
	TSharedRef<FEffectsState> Fx = MakeShared<FEffectsState>();
	FAutomationTestBase* Test = this;
	auto Step = [this, State](const TCHAR* Name, TFunction<void()> Enter, TFunction<bool()> Done)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FStep(this, State, Name, MoveTemp(Enter), MoveTemp(Done)));
	};
	auto View = [State]() { return State->V.Num() > 0 ? State->V[0].Get() : nullptr; };
	auto Animation = [View]() -> UWuwaUiViewAnimation*
	{
		UWuwaUiView* V = View();
		return V && V->GetRootActor() ? V->GetRootActor()->FindComponentByClass<UWuwaUiViewAnimation>() : nullptr;
	};
	auto Dilation = []() { return UGameplayStatics::GetGlobalTimeDilation(FindPlayWorld()); };

	Step(TEXT("wait for UiManager"), nullptr, [State]()
	{
		UWorld* World = FindPlayWorld();
		APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
		State->Manager = Player ? UWuwaUiManager::Get(Player) : nullptr;
		return State->Manager.IsValid();
	});

	Step(TEXT("start from an empty Normal layer"), [State]()
	{
		if (State->Manager->IsViewOpen(Normal1)) State->Manager->CloseView(Normal1);
	}, []() { return CountLayerChildren(EWuwaUiLayer::Normal) == 0; });

	// 打开：动画按时间播放；第一次显示时播音效、压时间缩放、Push 相机、铺模糊；播完后全屏隐藏下面的层。
	Step(TEXT("open with effects"), [State, Fx, Dilation]()
	{
		UWuwaTestUiCameraHandler* Camera = NewObject<UWuwaTestUiCameraHandler>(GetTransientPackage());
		State->KeepAlive.Emplace(Camera);
		Fx->Camera = Camera;
		State->Manager->SetUiCameraHandler(Camera);
		Fx->BaseDilation = Dilation();
		State->V.Add(State->Manager->OpenView(EffectsView));
	}, [View, Animation, Fx]()
	{
		UWuwaUiView* V = View();
		if (V && V->GetState() == EWuwaUiState::Showing)
		{
			Fx->bSawOpenSound |= V->GetLastPlayedSound() != nullptr;
			if (const UWuwaUiViewAnimation* Anim = Animation())
			{
				Fx->bSawShowAnimating |= Anim->IsPlaying() && Anim->GetProgress() > 0.05f && Anim->GetProgress() < 0.95f;
			}
		}
		return V && V->GetState() == EWuwaUiState::Shown;
	});

	Step(TEXT("check opened"), [Test, State, Fx, View, Animation, Dilation]()
	{
		UWuwaUiView* V = View();
		Test->TestTrue(TEXT("show animation played over several frames"), Fx->bSawShowAnimating);
		Test->TestTrue(TEXT("animation finished at full"), Animation() && FMath::IsNearlyEqual(Animation()->GetProgress(), 1.f));
		Test->TestTrue(TEXT("open sound played"), Fx->bSawOpenSound);
		Test->TestEqual(TEXT("time dilation 0.5"), Dilation(), 0.5f, 0.001f);
		Test->TestEqual(TEXT("UI camera pushed"), Fx->Camera->GetActiveCameraMode(), FName(TEXT("TestCamera")));

		const AActor* Blur = V->GetBlurActor();
		const UUIItem* BlurItem = Blur ? Cast<UUIItem>(Blur->GetRootComponent()) : nullptr;
		const UUIItem* RootItem = V->GetRootItem();
		Test->TestTrue(TEXT("blur sits under the Normal layer"), BlurItem && BlurItem->GetAttachParent() == State->Manager->GetLayerItem(EWuwaUiLayer::Normal));
		Test->TestTrue(TEXT("blur is drawn before (behind) the view"), BlurItem && RootItem && BlurItem->GetHierarchyIndex() < RootItem->GetHierarchyIndex());

		const UUIItem* Hud = State->Manager->GetLayerItem(EWuwaUiLayer::HUD);
		Test->TestTrue(TEXT("full screen hides the HUD layer"), State->Manager->IsLayerHiddenByFullScreen(EWuwaUiLayer::HUD) && Hud && !Hud->GetIsUIActiveSelf());
		Test->TestFalse(TEXT("its own layer stays visible"), State->Manager->IsLayerHiddenByFullScreen(EWuwaUiLayer::Normal));

		State->V.Add(State->Manager->OpenView(SlowPop));
	}, [State]() { return IsIn(State->V[1], EWuwaUiState::Shown); });

	// 时间缩放叠加：取最慢的；关掉后回到剩下的那个。
	Step(TEXT("stacked time dilation"), [Test, State, Dilation]()
	{
		Test->TestEqual(TEXT("the slower one wins"), Dilation(), 0.2f, 0.001f);
		Test->TestFalse(TEXT("layers above the full screen view stay visible"), State->Manager->IsLayerHiddenByFullScreen(EWuwaUiLayer::Pop));
		State->Manager->CloseView(SlowPop);
	}, [State, Dilation]() { return IsIn(State->V[1], EWuwaUiState::Destroyed) && FMath::IsNearlyEqual(Dilation(), 0.5f, 0.001f); });

	// 被栈上层盖住：模糊撤掉、下面的层恢复；时间缩放和相机保持（界面还开着）。
	Step(TEXT("cover"), [State]()
	{
		State->V.Add(State->Manager->OpenView(Normal2));   // V[2]
	}, [State]() { return IsIn(State->V[2], EWuwaUiState::Shown) && IsIn(State->V[0], EWuwaUiState::Hidden); });

	Step(TEXT("check covered"), [Test, State, Fx, View, Dilation]()
	{
		Test->TestNull(TEXT("blur removed while covered"), View()->GetBlurActor());
		Test->TestFalse(TEXT("HUD restored while covered"), State->Manager->IsLayerHiddenByFullScreen(EWuwaUiLayer::HUD));
		Test->TestEqual(TEXT("time dilation kept while covered"), Dilation(), 0.5f, 0.001f);
		Test->TestEqual(TEXT("camera kept while covered"), Fx->Camera->GetActiveCameraMode(), FName(TEXT("TestCamera")));
		State->Manager->CloseView(Normal2);
	}, [State, View]() { return IsIn(State->V[0], EWuwaUiState::Shown) && View()->GetBlurActor() != nullptr; });

	// 关闭：开始关闭就恢复下面的层，关闭动画播完才算 Hidden；播关闭音效；撤销时间缩放和相机。
	Step(TEXT("close with effects"), [Test, State]()
	{
		Test->TestTrue(TEXT("HUD hidden again after uncover"), State->Manager->IsLayerHiddenByFullScreen(EWuwaUiLayer::HUD));
		State->Manager->CloseView(EffectsView);
	}, [State, Fx, View, Animation]()
	{
		UWuwaUiView* V = View();
		if (V && V->GetState() == EWuwaUiState::Hiding)
		{
			Fx->bSawCloseSound |= V->GetLastPlayedSound() != nullptr;
			Fx->bHudVisibleWhileHiding |= !State->Manager->IsLayerHiddenByFullScreen(EWuwaUiLayer::HUD);
			if (const UWuwaUiViewAnimation* Anim = Animation())
			{
				Fx->bSawHideAnimating |= Anim->IsPlaying() && Anim->GetProgress() > 0.05f && Anim->GetProgress() < 0.95f;
			}
		}
		return V && V->GetState() == EWuwaUiState::Destroyed;
	});

	Step(TEXT("check closed"), [Test, State, Fx, Dilation]()
	{
		Test->TestTrue(TEXT("hide animation played over several frames"), Fx->bSawHideAnimating);
		Test->TestTrue(TEXT("HUD visible during the close animation"), Fx->bHudVisibleWhileHiding);
		Test->TestTrue(TEXT("close sound played"), Fx->bSawCloseSound);
		Test->TestEqual(TEXT("time dilation restored"), Dilation(), Fx->BaseDilation, 0.001f);
		Test->TestEqual(TEXT("UI camera popped"), Fx->Camera->GetActiveCameraMode(), FName(NAME_None));
		Test->TestEqual(TEXT("nothing left under Normal (view and blur)"), CountLayerChildren(EWuwaUiLayer::Normal), 0);
		State->Manager->SetUiCameraHandler(nullptr);
	}, []() { return true; });

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
