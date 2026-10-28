#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Game/Camera/WuwaCameraMode.h"
#include "Game/Camera/WuwaCameraRuntimeBridge.h"
#include "Game/Camera/WuwaPlayerCameraManager.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

namespace WuwaCameraPlayTests
{
	UWorld* FindPlayWorld()
	{
		if (!GEngine) return nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
				return Context.World();
		}
		return nullptr;
	}

	class FVerifyCameraStartup final : public IAutomationLatentCommand
	{
	public:
		explicit FVerifyCameraStartup(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			if (StartedAt == 0.0) StartedAt = FPlatformTime::Seconds();
			UWorld* World = FindPlayWorld();
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			APawn* Pawn = IsValid(Player) ? Player->GetPawn() : nullptr;
			AWuwaPlayerCameraManager* Camera = IsValid(Player) ? Cast<AWuwaPlayerCameraManager>(Player->PlayerCameraManager) : nullptr;
			UWuwaCameraRuntimeBridge* Runtime = Camera ? Camera->GetCameraRuntime() : nullptr;
			// 加载界面结束、角色被控制后，相机要到下一次更新才创建运行时；等到它出现再检查
			if (!World || !World->HasBegunPlay() || !IsValid(Player) || !IsValid(Pawn) || !Runtime)
			{
				if (FPlatformTime::Seconds() - StartedAt < 120.0) return false;
				Test->TestNotNull(TEXT("PIE produces a possessed local player within 120 seconds"), Pawn);
				Test->TestNotNull(TEXT("Actual player camera manager uses the new adapter"), Camera);
				Test->TestNotNull(TEXT("Actual camera update initializes the managed runtime"), Runtime);
				return true;
			}

			Test->TestTrue(TEXT("Real map creates the Wuwa player controller"), Player->IsA<AWuwaPlayerController>());
			Test->TestTrue(TEXT("PIE player controller is local"), Player->IsLocalController());
			Test->TestTrue(TEXT("Real map creates the Wuwa character"), Pawn->IsA<AWuwaCharacter>());

			Test->TestTrue(TEXT("Gameplay view target remains the possessed pawn"), Player->GetViewTarget() == Pawn);
			Test->TestNull(TEXT("Character no longer owns the old rendering camera"), Pawn->FindComponentByClass<UCameraComponent>());
			Test->TestNull(TEXT("Character no longer owns the old spring arm"), Pawn->FindComponentByClass<USpringArmComponent>());

			const FWuwaCameraSettings Base = IsValid(Camera->DefaultMode)
				? Camera->DefaultMode->Settings : Camera->DefaultSettings;
			const FWuwaCameraSettings Settings = Runtime->ResolveSettings(Base);
			const FVector CameraLocation = Camera->GetCameraLocation();
			const double Distance = FVector::Distance(CameraLocation, Pawn->GetActorLocation());
			Test->TestTrue(TEXT("Actual gameplay camera location and rotation remain finite"),
				!CameraLocation.ContainsNaN() && !Camera->GetCameraRotation().ContainsNaN());
			// Collision may shorten the arm below the input minimum; it must never exceed the requested maximum.
			Test->TestTrue(TEXT("Gameplay camera is behind the pawn at a nonzero bounded distance"),
				FMath::IsFinite(Distance) && Distance > 1.0
				&& Distance <= Settings.Arm.MaxArmLength + Settings.Offset.PivotOffset.Size() + 1.0);
			Test->AddInfo(FString::Printf(TEXT("PIE camera=%s pawn=%s distance=%.2f location=%s"),
				*Camera->GetClass()->GetName(), *Pawn->GetClass()->GetName(), Distance, *CameraLocation.ToString()));
			return true;
		}

	private:
		FAutomationTestBase* Test;
		double StartedAt = 0.0;
	};

	class FWaitForPlayWorldToEnd final : public IAutomationLatentCommand
	{
	public:
		explicit FWaitForPlayWorldToEnd(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			if (!FindPlayWorld()) return true;
			if (StartedAt == 0.0) StartedAt = FPlatformTime::Seconds();
			if (FPlatformTime::Seconds() - StartedAt < 120.0) return false;
			Test->AddError(TEXT("PIE world did not finish shutting down within 120 seconds."));
			return true;
		}

	private:
		FAutomationTestBase* Test;
		double StartedAt = 0.0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCameraPlayStartupTest,
	"Wuwa.Camera.Play.RealMapStartup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCameraPlayStartupTest::RunTest(const FString& Parameters)
{
	// In editor builds AutomationOpenMap already opens the map AND starts PIE.
	// Queueing FStartPIECommand as well would issue a second play request.
	const bool bStarted = AutomationOpenMap(TEXT("/Game/Map/Wuwa"));
	if (bStarted)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));
		ADD_LATENT_AUTOMATION_COMMAND(WuwaCameraPlayTests::FVerifyCameraStartup(this));
	}
	else
	{
		AddError(TEXT("AutomationOpenMap could not start /Game/Map/Wuwa."));
	}
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(WuwaCameraPlayTests::FWaitForPlayWorldToEnd(this));
	return bStarted;
}

#endif
