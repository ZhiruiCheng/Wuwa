#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EnhancedActionKeyMapping.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Core/Tag/WuwaGameTags.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Input/DataAsset/WuwaInputDataAsset.h"
#include "Game/Input/WuwaInputTypes.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputIntentComponent.h"

// 诊断"按住 Alt 显示鼠标"整条链路：配置资产 → 实际使用的控制器 → 路由分发 → UI 处理器。不在常规测试套件里。
namespace UiInputChainDiag
{
	UWorld* FindPlayWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) return Context.World();
		}
		return nullptr;
	}

	void DumpConfig(FAutomationTestBase* Test, const UWuwaInputDataAsset* TagMap, const UInputMappingContext* Context)
	{
		Test->AddInfo(FString::Printf(TEXT("  InputTagMap = %s"), *GetPathNameSafe(TagMap)));
		if (TagMap)
		{
			for (const FInputDataAsset& Entry : TagMap->InputDataAssetMap)
			{
				Test->AddInfo(FString::Printf(TEXT("    %-28s tag=%-28s route=%-22s configured=%d"), *GetNameSafe(Entry.InputAction),
					*Entry.InputTag.ToString(), *Entry.RouteTag.ToString(), Entry.IsConfigured()));
			}
		}
		Test->AddInfo(FString::Printf(TEXT("  DefaultMappingContext = %s"), *GetPathNameSafe(Context)));
		if (Context)
		{
			for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
			{
				if (Mapping.Key.ToString().Contains(TEXT("Alt")) || GetNameSafe(Mapping.Action).Contains(TEXT("Mouse")))
				{
					Test->AddInfo(FString::Printf(TEXT("    key %-12s -> %s (triggers %d)"), *Mapping.Key.ToString(), *GetNameSafe(Mapping.Action), Mapping.Triggers.Num()));
				}
			}
		}
	}

	class FCheckChain final : public IAutomationLatentCommand
	{
	public:
		FCheckChain(FAutomationTestBase* InTest, FString InMap) : Test(InTest), Map(MoveTemp(InMap)) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			UWorld* World = FindPlayWorld();
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			if (!Player)
			{
				if (Now - StartedAt > 20.0) { Test->AddError(TEXT("No player controller.")); return true; }
				return false;
			}

			Test->AddInfo(FString::Printf(TEXT("[%s] controller class = %s"), *Map, *Player->GetClass()->GetPathName()));
			AWuwaPlayerController* Wuwa = Cast<AWuwaPlayerController>(Player);
			if (!Wuwa)
			{
				Test->AddWarning(FString::Printf(TEXT("[%s] the controller is not an AWuwaPlayerController: no InputRouter, the UI input handler never runs here."), *Map));
				return true;
			}
			DumpConfig(Test, Wuwa->InputTagMap, Wuwa->DefaultMappingContext);

			// 绕过按键，直接从路由注入"显示鼠标"按下/松开，看 UI 处理器有没有生效。
			FWuwaInputEvent Event;
			Event.InputTag = FWuwaGameTags::Get().Player_UI_ShowMouse;
			Event.RouteTag = FWuwaGameTags::Get().Input_Route_UI;
			Event.Phase = EWuwaInputPhase::Pressed;
			const bool bHandledPress = Wuwa->RouteInputEvent(Event);
			const bool bShownAfterPress = Wuwa->ShouldShowMouseCursor();
			const bool bLookIgnored = Wuwa->IsLookInputIgnored();
			const FWuwaGameTags& Tags = FWuwaGameTags::Get();
			Test->AddInfo(FString::Printf(TEXT("[%s] while shown: blocked Ability=%d Movement=%d Camera=%d UI=%d"), *Map,
				Wuwa->IsRouteBlocked(Tags.Input_Route_Ability), Wuwa->IsRouteBlocked(Tags.Input_Route_Movement),
				Wuwa->IsRouteBlocked(Tags.Input_Route_Camera), Wuwa->IsRouteBlocked(Tags.Input_Route_UI)));
			Event.Phase = EWuwaInputPhase::Released;
			Wuwa->RouteInputEvent(Event);
			Test->AddInfo(FString::Printf(TEXT("[%s] after release: blocked Ability=%d Movement=%d Camera=%d"), *Map,
				Wuwa->IsRouteBlocked(Tags.Input_Route_Ability), Wuwa->IsRouteBlocked(Tags.Input_Route_Movement), Wuwa->IsRouteBlocked(Tags.Input_Route_Camera)));
			const bool bShownAfterRelease = Wuwa->ShouldShowMouseCursor();
			Test->AddInfo(FString::Printf(TEXT("[%s] injected press: handled=%d cursorShown=%d lookIgnored=%d; after release: cursorShown=%d lookIgnored=%d"),
				*Map, bHandledPress, bShownAfterPress, bLookIgnored, bShownAfterRelease, Wuwa->IsLookInputIgnored()));
			return true;
		}

	private:
		FAutomationTestBase* Test;
		FString Map;
		double StartedAt = -1.0;
	};
}

namespace UiInputChainDiag
{
	// 回归：按住 W 再按 Alt，屏蔽开始时要给 W 补一个取消（移动轴归零）；屏蔽期间 W 的持续输入被拦，松开 Alt 后 W 继续生效。
	class FCheckBlockCancelsHeldMove final : public IAutomationLatentCommand
	{
	public:
		explicit FCheckBlockCancelsHeldMove(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			UWorld* World = FindPlayWorld();
			AWuwaPlayerController* Wuwa = World ? Cast<AWuwaPlayerController>(World->GetFirstPlayerController()) : nullptr;
			APawn* Pawn = Wuwa ? Wuwa->GetPawn() : nullptr;
			if (!Pawn)
			{
				// 冷启动时编队异步加载要 20 秒以上（角色资源第一次加载），给足时间
				if (Now - StartedAt > 120.0) { Test->AddError(TEXT("No AWuwaPlayerController with a pawn.")); return true; }
				return false;
			}
			UWuwaInputIntentComponent* Intent = Pawn->FindComponentByClass<UWuwaInputIntentComponent>();
			if (!Intent || !Wuwa->InputTagMap)
			{
				Test->AddError(TEXT("Pawn has no UWuwaInputIntentComponent, or the controller has no InputTagMap."));
				return true;
			}

			const FWuwaGameTags& Tags = FWuwaGameTags::Get();
			const FInputDataAsset* MoveEntry = Wuwa->InputTagMap->InputDataAssetMap.FindByPredicate(
				[&Tags](const FInputDataAsset& Entry) { return Entry.InputTag == Tags.Player_Common_Movement_Move; });
			if (!MoveEntry || !MoveEntry->InputAction)
			{
				Test->AddError(TEXT("InputTagMap has no Move entry."));
				return true;
			}

			FWuwaInputEvent Move;
			Move.InputTag = MoveEntry->InputTag;
			Move.RouteTag = MoveEntry->RouteTag;
			Move.SourceAction = MoveEntry->InputAction;
			Move.Phase = EWuwaInputPhase::Triggered;
			Move.Value = FInputActionValue(FVector2D(0.0, 1.0));

			FWuwaInputEvent ShowMouse;
			ShowMouse.InputTag = Tags.Player_UI_ShowMouse;
			ShowMouse.RouteTag = Tags.Input_Route_UI;

			const auto Axis = [Intent]() { return Intent->GetMoveIntent().Axis; };

			Wuwa->RouteInputEvent(Move);
			Test->TestEqual(TEXT("Holding W writes the move axis"), Axis(), FVector2D(0.0, 1.0));

			ShowMouse.Phase = EWuwaInputPhase::Pressed;
			Wuwa->RouteInputEvent(ShowMouse);
			Test->TestTrue(TEXT("Alt blocks the movement route"), Wuwa->IsRouteBlocked(Move.RouteTag));
			Test->TestEqual(TEXT("Pressing Alt cancels the held W"), Axis(), FVector2D::ZeroVector);

			Wuwa->RouteInputEvent(Move);
			Test->TestEqual(TEXT("W keeps being blocked while Alt is held"), Axis(), FVector2D::ZeroVector);

			ShowMouse.Phase = EWuwaInputPhase::Released;
			Wuwa->RouteInputEvent(ShowMouse);
			Test->TestFalse(TEXT("Releasing Alt unblocks the movement route"), Wuwa->IsRouteBlocked(Move.RouteTag));

			Wuwa->RouteInputEvent(Move);
			Test->TestEqual(TEXT("A still-held W moves again after Alt is released"), Axis(), FVector2D(0.0, 1.0));

			// 按住 W 时按下 Alt，再在 Alt 按住期间松开 W：松开要能穿过屏蔽，Alt 松开后不再移动。
			ShowMouse.Phase = EWuwaInputPhase::Pressed;
			Wuwa->RouteInputEvent(ShowMouse);
			Move.Phase = EWuwaInputPhase::Released;
			Move.Value.Reset();
			Wuwa->RouteInputEvent(Move);
			ShowMouse.Phase = EWuwaInputPhase::Released;
			Wuwa->RouteInputEvent(ShowMouse);
			Test->TestEqual(TEXT("W released during Alt stays released"), Axis(), FVector2D::ZeroVector);
			return true;
		}

	private:
		FAutomationTestBase* Test;
		double StartedAt = -1.0;
	};

	bool Run(FAutomationTestBase* Test, const TCHAR* Map)
	{
		if (!AutomationOpenMap(Map))
		{
			Test->AddError(FString::Printf(TEXT("Could not open %s."), Map));
			return false;
		}
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FCheckChain(Test, Map));
		ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUiInputChainDiagWuwaTest, "Diag.UIInputChain.WuwaMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FUiInputChainDiagWuwaTest::RunTest(const FString& Parameters) { return UiInputChainDiag::Run(this, TEXT("/Game/Map/Wuwa")); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUiInputChainDiagLguiTest, "Diag.UIInputChain.LGUIMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FUiInputChainDiagLguiTest::RunTest(const FString& Parameters) { return UiInputChainDiag::Run(this, TEXT("/Game/Map/LGUI")); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaInputBlockCancelsHeldMoveTest, "Wuwa.Input.Block.CancelsHeldInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWuwaInputBlockCancelsHeldMoveTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Map/Wuwa")))
	{
		AddError(TEXT("Could not open /Game/Map/Wuwa."));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(UiInputChainDiag::FCheckBlockCancelsHeldMove(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
