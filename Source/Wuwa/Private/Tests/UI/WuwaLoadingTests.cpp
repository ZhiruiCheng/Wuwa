#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Core/ActorComponent/UIText.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Game/LGUI/Core/WuwaUiManager.h"
#include "Game/LGUI/View/WuwaUiState.h"
#include "Game/Module/Hud/WuwaMainHudController.h"
#include "Game/Module/Loading/WuwaLoadingController.h"
#include "Game/Module/Loading/WuwaLoadingModel.h"
#include "Game/Module/Loading/WuwaLoadingView.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaLoadingPhaseMathTest, "Wuwa.UI.Loading.PhaseMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWuwaLoadingPhaseMathTest::RunTest(const FString& Parameters)
{
	float Total = 0.f;
	for (int32 Index = 0; Index < static_cast<int32>(EWuwaLoadingPhase::Count); ++Index)
	{
		Total += WuwaLoading::PhaseWeight(static_cast<EWuwaLoadingPhase>(Index));
	}
	TestEqual(TEXT("Phase weights add up to 1"), Total, 1.f, 0.0001f);

	// Model 是 LocalPlayerSubsystem，外层必须是 LocalPlayer（LocalPlayer 的外层又必须是 GEngine）。
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	UWuwaLoadingModel* Model = NewObject<UWuwaLoadingModel>(LocalPlayer);
	Model->Begin(FWuwaLoadingShowRow());
	Model->SetPhaseProgress(EWuwaLoadingPhase::Formation, 0.5f);
	TestEqual(TEXT("Half of the formation phase"), Model->GetTargetProgress(), 0.325f, 0.0001f);
	Model->SetPhaseProgress(EWuwaLoadingPhase::Formation, 0.2f);
	TestEqual(TEXT("Progress never goes back"), Model->GetTargetProgress(), 0.325f, 0.0001f);
	Model->SetPhaseProgress(EWuwaLoadingPhase::MainHud, 0.5f);
	TestEqual(TEXT("Half of the HUD phase"), Model->GetTargetProgress(), 0.9f, 0.0001f);
	TestFalse(TEXT("Not finished before Finish"), Model->IsFinished());
	Model->Finish();
	TestEqual(TEXT("Finish fills the bar"), Model->GetTargetProgress(), 1.f);
	TestTrue(TEXT("Finished"), Model->IsFinished());
	return true;
}

namespace WuwaLoadingTests
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

	// 进入游戏地图：加载界面打开 → 进度只增不减 → 主 HUD 显示出来 → 加载界面走满 100% 后关闭。
	class FWatchWorldLoad final : public IAutomationLatentCommand
	{
	public:
		explicit FWatchWorldLoad(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			if (Now - StartedAt > 30.0)
			{
				Test->AddError(FString::Printf(TEXT("World load did not finish (seen loading view=%d, max displayed=%.2f)."), bSawLoadingView, MaxDisplayed));
				return true;
			}
			APlayerController* PC = FindPlayer();
			ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
			UWuwaUiManager* UiManager = PC ? UWuwaUiManager::Get(PC) : nullptr;
			if (!LocalPlayer || !UiManager)
			{
				return false;
			}
			const UWuwaLoadingModel* Model = LocalPlayer->GetSubsystem<UWuwaLoadingModel>();
			const UWuwaLoadingView* Loading = Cast<UWuwaLoadingView>(UiManager->GetView(UWuwaLoadingController::LoadingViewName));
			if (Loading)
			{
				if (!bSawLoadingView)
				{
					bSawLoadingView = true;
					Test->TestTrue(TEXT("LoadingView is a UWuwaLoadingView on the loading layer"), Loading->GetLayer() == EWuwaUiLayer::Loading);
				}
				if (Loading->GetState() == EWuwaUiState::Shown)
				{
					const float Displayed = Loading->GetDisplayedProgress();
					if (Displayed + 0.0001f < MaxDisplayed)
					{
						Test->AddError(FString::Printf(TEXT("Displayed progress went back from %.3f to %.3f."), MaxDisplayed, Displayed));
					}
					MaxDisplayed = FMath::Max(MaxDisplayed, Displayed);
					if (!bCheckedTexts && Model && Model->IsLoading())
					{
						bCheckedTexts = true;
						Test->TestFalse(TEXT("A show row was picked (title is not empty)"), Model->GetShow().Title.IsEmpty());
					}
				}
				return false;
			}
			if (!bSawLoadingView)
			{
				return false;
			}

			// 加载界面已经关了：主 HUD 必须已经显示，进度显示到过 100%。
			const UWuwaUiView* Hud = UiManager->GetView(UWuwaMainHudController::MainHudViewName);
			Test->TestTrue(TEXT("MainHUD is shown when the loading screen closes"), Hud && Hud->GetState() == EWuwaUiState::Shown);
			Test->TestEqual(TEXT("The bar reached 100% before closing"), MaxDisplayed, 1.f, 0.0001f);
			Test->TestFalse(TEXT("Loading state is reset"), Model && Model->IsLoading());
			return true;
		}

	private:
		FAutomationTestBase* Test;
		double StartedAt = -1.0;
		bool bSawLoadingView = false;
		bool bCheckedTexts = false;
		float MaxDisplayed = 0.f;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaLoadingWorldFlowTest, "Wuwa.UI.Loading.WorldLoadFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWuwaLoadingWorldFlowTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Map/Wuwa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(WuwaLoadingTests::FWatchWorldLoad(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
