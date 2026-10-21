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

// 字幕不属于主 HUD：是 Float 层（提示，同一时刻只显示一个）的界面 SubtitleTips（预制体 Tips_Subtitle，
// 按原作 PC_UiView_PlotsD 的字幕块还原）。
namespace WuwaSubtitleTipsTests
{
	const FName ViewName(TEXT("SubtitleTips"));

	class FOpenAndClose final : public IAutomationLatentCommand
	{
	public:
		explicit FOpenAndClose(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			APlayerController* PC = nullptr;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) PC = Context.World()->GetFirstPlayerController();
			}
			UWuwaUiManager* UiManager = PC ? UWuwaUiManager::Get(PC) : nullptr;
			if (!UiManager)
			{
				if (Now - StartedAt > 20.0) { Test->AddError(TEXT("No UiManager.")); return true; }
				return false;
			}
			// 先等主 HUD 自动打开（编队生成之后），再开字幕
			const UWuwaUiView* Hud = UiManager->GetView(UWuwaMainHudController::MainHudViewName);
			if (!Hud || Hud->GetState() != EWuwaUiState::Shown)
			{
				if (Now - StartedAt > 30.0) { Test->AddError(TEXT("MainHUD never opened.")); return true; }
				return false;
			}
			if (!View.IsValid())
			{
				const FWuwaUiViewConfig* Config = UiManager->FindViewConfig(ViewName);
				if (!Test->TestNotNull(TEXT("SubtitleTips row exists in the view table"), Config)) return true;
				Test->TestTrue(TEXT("Subtitles live in the Float (tips) layer"), Config->Layer == EWuwaUiLayer::Float);
				View = UiManager->OpenView(ViewName);
				if (!Test->TestNotNull(TEXT("SubtitleTips opens"), View.Get())) return true;
				return false;
			}
			if (View->GetState() != EWuwaUiState::Shown)
			{
				if (Now - StartedAt > 40.0) { Test->AddError(TEXT("SubtitleTips never reached Shown.")); return true; }
				return false;
			}
			// 主 HUD 不受影响（字幕是另一层的界面）
			Test->TestTrue(TEXT("MainHUD stays shown"), Hud->GetState() == EWuwaUiState::Shown);
			Test->TestTrue(TEXT("SubtitleTips closes"), UiManager->CloseView(ViewName));
			return true;
		}

	private:
		FAutomationTestBase* Test;
		double StartedAt = -1.0;
		TWeakObjectPtr<UWuwaUiView> View;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSubtitleTipsTest, "Wuwa.UI.SubtitleTips.OpensInFloatLayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWuwaSubtitleTipsTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Map/Wuwa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(WuwaSubtitleTipsTests::FOpenAndClose(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
