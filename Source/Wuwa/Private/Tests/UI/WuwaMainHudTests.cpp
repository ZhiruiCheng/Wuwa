#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Game/LGUI/Core/WuwaUISettings.h"
#include "Game/LGUI/Core/WuwaUiManager.h"
#include "Game/LGUI/View/WuwaUiState.h"
#include "Game/LGUI/View/WuwaUiView.h"
#include "Game/Module/Hud/WuwaMainHudController.h"
#include "Core/ActorComponent/UIItem.h"
#include "Layout/LGUICanvasScaler.h"
#include "UObject/UObjectIterator.h"

namespace WuwaMainHudTests
{
	APlayerController* FindPlayer()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				return Context.World()->GetFirstPlayerController();
			}
		}
		return nullptr;
	}

	// 有队伍的游戏地图：不需要任何人手动 OpenView，主 HUD 自动打开并显示。
	class FExpectOpened final : public IAutomationLatentCommand
	{
	public:
		explicit FExpectOpened(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			APlayerController* PC = FindPlayer();
			UWuwaUiManager* UiManager = PC ? UWuwaUiManager::Get(PC) : nullptr;
			const UWuwaUiView* Hud = UiManager ? UiManager->GetView(UWuwaMainHudController::MainHudViewName) : nullptr;
			if (!Hud || Hud->GetState() != EWuwaUiState::Shown)
			{
				if (Now - StartedAt > 20.0)
				{
					Test->AddError(FString::Printf(TEXT("MainHUD was not opened automatically (view=%s)."), *GetNameSafe(Hud)));
					return true;
				}
				return false;
			}
			Test->TestTrue(TEXT("The game map wants a main HUD"), UWuwaMainHudController::ShouldShowMainHud(PC));
			TArray<UWuwaUiView*> Views;
			UiManager->GetViews(UWuwaMainHudController::MainHudViewName, Views);
			Test->TestEqual(TEXT("Exactly one main HUD"), Views.Num(), 1);
			Test->TestNotNull(TEXT("MainHUD row exists in the view table"), UiManager->FindViewConfig(UWuwaMainHudController::MainHudViewName));

			// 主 HUD 铺满屏幕：根节点拉伸到整个 HUD 层。
			if (const UUIItem* Root = Hud->GetRootItem())
			{
				Test->TestEqual(TEXT("MainHUD root anchor min"), Root->GetAnchorMin(), FVector2D(0.f, 0.f));
				Test->TestEqual(TEXT("MainHUD root anchor max"), Root->GetAnchorMax(), FVector2D(1.f, 1.f));
			}
			// 根画布按设计分辨率随屏幕缩放。
			const ULGUICanvasScaler* Scaler = nullptr;
			for (TObjectIterator<ULGUICanvasScaler> It; It; ++It)
			{
				if (It->GetWorld() == PC->GetWorld()) { Scaler = *It; break; }
			}
			if (Test->TestNotNull(TEXT("The UI root has a canvas scaler"), Scaler))
			{
				ULGUICanvasScaler* Mutable = const_cast<ULGUICanvasScaler*>(Scaler);
				Test->TestTrue(TEXT("The UI scales with the screen"), Mutable->GetUIScaleMode() == ELGUICanvasScaleMode::ScaleWithScreenSize);
				Test->TestEqual(TEXT("Reference resolution"), Mutable->GetReferenceResolution(), FVector2D(1920.f, 1080.f));
			}
			return true;
		}

	private:
		FAutomationTestBase* Test;
		double StartedAt = -1.0;
	};

	// 没有队伍的地图（LGUI 测试地图）：Controller 判断不需要主 HUD。
	class FExpectNotOpened final : public IAutomationLatentCommand
	{
	public:
		explicit FExpectNotOpened(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			APlayerController* PC = FindPlayer();
			if (!PC)
			{
				if (++WaitFrames > 600) { Test->AddError(TEXT("No player controller.")); return true; }
				return false;
			}
			// 只检查 Controller 自己的判断：LGUI 测试地图的 BP_LGUIPlayerController 会自己打开 MainHUD，那不归这个 Controller 管。
			Test->TestFalse(TEXT("A map without a formation does not want a main HUD"), UWuwaMainHudController::ShouldShowMainHud(PC));
			return true;
		}

	private:
		FAutomationTestBase* Test;
		int32 WaitFrames = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaMainHudAutoOpenTest, "Wuwa.UI.MainHud.AutoOpen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWuwaMainHudAutoOpenTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Map/Wuwa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(WuwaMainHudTests::FExpectOpened(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaMainHudNotInLguiMapTest, "Wuwa.UI.MainHud.NotInMapsWithoutFormation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWuwaMainHudNotInLguiMapTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Map/LGUI"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(WuwaMainHudTests::FExpectNotOpened(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
