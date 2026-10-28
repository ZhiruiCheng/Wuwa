#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Game/Camera/WuwaCameraMode.h"
#include "Game/Camera/WuwaCameraRuntimeBridge.h"
#include "Game/Camera/WuwaPlayerCameraManager.h"
#include "Core/Tag/WuwaGameTags.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Input/WuwaInputRouterComponent.h"
#include "Game/Input/WuwaInputTypes.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "UObject/Script.h"

namespace WuwaCameraInputTests
{
	struct FFixture
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AWuwaPlayerController* Controller = nullptr;
		APawn* Pawn = nullptr;
		AWuwaPlayerCameraManager* Camera = nullptr;
		UInputAction* LookAction = nullptr;
		UInputAction* ZoomAction = nullptr;

		FFixture()
		{
			if (!World) return;
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Spawn.ObjectFlags |= RF_Transient;
			Controller = World->SpawnActor<AWuwaPlayerController>(Spawn);
			Pawn = World->SpawnActor<APawn>(Spawn);
			if (!Controller || !Pawn) return;
			// No GameMode login runs in this isolated world. Mark the controller local,
			// matching the real player creation path without creating a viewport.
			Controller->SetAsLocalPlayerController();
			ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
			LocalPlayer->PlayerController = Controller;
			Controller->Player = LocalPlayer;
			// CreateWorld does not initialize actors, so PlayerController::PostInitializeComponents
			// has not run its normal SpawnPlayerCameraManager step. Reproduce that setup here;
			// do not alter production lifecycle or initialize the unrelated game/player state.
			if (!Controller->PlayerCameraManager)
			{
				Spawn.Owner = Controller;
				Controller->PlayerCameraManager = World->SpawnActor<AWuwaPlayerCameraManager>(Spawn);
				if (Controller->PlayerCameraManager)
				{
					Controller->PlayerCameraManager->InitializeFor(Controller);
				}
			}
			Camera = Cast<AWuwaPlayerCameraManager>(Controller->PlayerCameraManager);
			if (!Camera) return;
			Camera->DefaultSettings.Arm.bCollisionTest = false;
			Camera->DefaultSettings.Zoom.ZoomInterpSpeed = 0.f;
			Camera->DefaultSettings.Blend.BlendTime = 0.f;
			Controller->SetPawn(Pawn);
			Pawn->SetController(Controller);
			Controller->SetControlRotation(FRotator::ZeroRotator);
			Controller->SetViewTarget(Pawn);
			Controller->RegisterInputRouteHandlers();
			LookAction = NewObject<UInputAction>(Controller);
			LookAction->ValueType = EInputActionValueType::Axis2D;
			ZoomAction = NewObject<UInputAction>(Controller);
			ZoomAction->ValueType = EInputActionValueType::Axis1D;
		}

		~FFixture()
		{
			if (IsValid(Controller)) Controller->SetPawn(nullptr);
			if (World) World->DestroyWorld(false);
		}

		bool IsReady(FAutomationTestBase& Test) const
		{
			return Test.TestNotNull(TEXT("Controller is available"), Controller)
				&& Test.TestEqual(TEXT("Controller defaults to Wuwa camera manager class"),
					Controller->PlayerCameraManagerClass.Get(), AWuwaPlayerCameraManager::StaticClass())
				&& Test.TestNotNull(TEXT("Camera manager is initialized for the controller"), Camera)
				&& Test.TestNotNull(TEXT("Follow target is available"), Pawn)
				&& Test.TestTrue(TEXT("Fixture uses local player input"), Controller->IsLocalController())
				&& Test.TestNotNull(TEXT("Managed camera runtime class is available"), Camera->RuntimeClass.LoadSynchronous());
		}

		bool Dispatch(bool bLook, EWuwaInputPhase Phase, const FInputActionValue& Value) const
		{
			const FWuwaGameTags InputTags = FWuwaGameTags::Get();
			FWuwaInputEvent Event;
			Event.InputTag = bLook ? InputTags.Player_Common_Camera_Rotate : InputTags.Player_Common_Camera_Zoom;
			Event.RouteTag = InputTags.Input_Route_Camera;
			Event.SourceAction = bLook ? LookAction : ZoomAction;
			Event.Phase = Phase;
			Event.Value = Value;
			Event.Timestamp = 1.0;
			return Controller->GetInputRouter()->DispatchInput(Event);
		}

		double DistanceToTarget() const
		{
			return FVector::Distance(Camera->GetCameraLocation(), Pawn->GetActorLocation());
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCameraRoutedZoomTest,
	"Wuwa.Camera.Input.RoutedZoomAndFlush",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCameraRoutedZoomTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptExecutionGuard;
	// Each case owns a fresh manager and performs its first gameplay update once. This also
	// exercises the real frame cache without changing the engine's global frame counter.
	struct FCase { float Delta; float Expected; bool bFlush; };
	const FCase Cases[] = {{1.f, 350.f, false}, {100.f, 150.f, false}, {-100.f, 800.f, false}, {1.f, 400.f, true}};
	for (const FCase& Case : Cases)
	{
		WuwaCameraInputTests::FFixture F;
		if (!F.IsReady(*this)) return false;
		TestTrue(TEXT("Started reaches camera route"), F.Dispatch(false, EWuwaInputPhase::Pressed, FInputActionValue(Case.Delta)));
		TestTrue(TEXT("Triggered reaches camera route"), F.Dispatch(false, EWuwaInputPhase::Triggered, FInputActionValue(Case.Delta)));
		if (Case.bFlush) F.Controller->FlushPressedKeys();
		F.Camera->UpdateCamera(1.f / 60.f);
		TestTrue(FString::Printf(TEXT("Wheel %.0f with flush %d produces arm length %.0f"), Case.Delta, Case.bFlush, Case.Expected),
			FMath::IsNearlyEqual(F.DistanceToTarget(), static_cast<double>(Case.Expected), 0.001));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCameraRoutedLookTest,
	"Wuwa.Camera.Input.LookSensitivityAndPitchLimits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCameraRoutedLookTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptExecutionGuard;
	WuwaCameraInputTests::FFixture F;
	if (!F.IsReady(*this)) return false;
	const FVector2D Axis(3.f, -2.f);
	F.Controller->RotationInput = FRotator::ZeroRotator;
	F.Controller->AddYawInput(Axis.X);
	F.Controller->AddPitchInput(Axis.Y);
	const FRotator Expected = F.Controller->RotationInput;
	TestFalse(TEXT("UE accepts local look input"), Expected.IsNearlyZero());
	F.Controller->RotationInput = FRotator::ZeroRotator;
	TestTrue(TEXT("Started is acknowledged"), F.Dispatch(true, EWuwaInputPhase::Pressed, FInputActionValue(Axis)));
	TestTrue(TEXT("Started does not rotate the view a second time"), F.Controller->RotationInput.IsNearlyZero());
	TestTrue(TEXT("Triggered is routed"), F.Dispatch(true, EWuwaInputPhase::Triggered, FInputActionValue(Axis)));
	TestTrue(TEXT("Look preserves the controller's existing sensitivity and inversion"), F.Controller->RotationInput.Equals(Expected, 0.001));
	F.Dispatch(true, EWuwaInputPhase::Released, FInputActionValue(FVector2D::ZeroVector));
	TestTrue(TEXT("Release does not add another rotation"), F.Controller->RotationInput.Equals(Expected, 0.001));

	F.Camera->DefaultSettings.Rotation.PitchMin = -40.f;
	F.Camera->DefaultSettings.Rotation.PitchMax = 30.f;
	FRotator View(25.f, 10.f, 0.f);
	FRotator Delta(20.f, 5.f, 0.f);
	F.Camera->ProcessViewRotation(1.f / 60.f, View, Delta);
	TestTrue(TEXT("Pitch is clamped to camera profile maximum"), FMath::IsNearlyEqual(View.Pitch, 30.0, 0.001));
	TestTrue(TEXT("Yaw remains responsive at the pitch limit"), FMath::IsNearlyEqual(View.Yaw, 15.0, 0.001));
	View = FRotator(-35.f, 0.f, 0.f);
	Delta = FRotator(-20.f, 0.f, 0.f);
	F.Camera->ProcessViewRotation(1.f / 60.f, View, Delta);
	TestTrue(TEXT("Pitch is clamped to camera profile minimum"), FMath::IsNearlyEqual(FRotator::NormalizeAxis(View.Pitch), -40.0, 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCameraLifecycleInputTest,
	"Wuwa.Camera.Input.PawnChangeAndExternalView",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCameraLifecycleInputTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptExecutionGuard;
	{
		WuwaCameraInputTests::FFixture F;
		if (!F.IsReady(*this)) return false;
		UWuwaCameraMode* Skill = NewObject<UWuwaCameraMode>(F.Camera);
		Skill->Settings = F.Camera->DefaultSettings;
		Skill->Settings.Arm.ArmLength = 200.f;
		Skill->Priority = 100;
		const int32 OldHandle = F.Camera->PushCameraMode(F.Pawn, Skill);
		TestTrue(TEXT("Skill mode is accepted"), OldHandle > 0);
		F.Dispatch(false, EWuwaInputPhase::Triggered, FInputActionValue(1.f));
		APawn* NewPawn = F.World->SpawnActor<APawn>();
		if (!TestNotNull(TEXT("Replacement target spawns"), NewPawn)) return false;
		F.Controller->SetPawn(NewPawn);
		NewPawn->SetController(F.Controller);
		F.Pawn = NewPawn;
		F.Controller->SetViewTarget(NewPawn);
		TestFalse(TEXT("Pawn change invalidates the previous skill handle"), F.Camera->PopCameraMode(OldHandle));
		F.Camera->UpdateCamera(1.f / 60.f);
		TestTrue(TEXT("New pawn starts with base distance and no pending old wheel input"), FMath::IsNearlyEqual(F.DistanceToTarget(), 400.0, 0.001));
	}
	{
		WuwaCameraInputTests::FFixture F;
		if (!F.IsReady(*this)) return false;
		ACameraActor* External = F.World->SpawnActor<ACameraActor>();
		if (!TestNotNull(TEXT("External camera spawns"), External)) return false;
		const FVector ExternalLocation(1200.f, -600.f, 300.f);
		const FRotator ExternalRotation(-15.f, 45.f, 0.f);
		External->SetActorLocationAndRotation(ExternalLocation, ExternalRotation);
		External->GetCameraComponent()->SetFieldOfView(55.f);
		F.Controller->SetViewTarget(External);
		F.Controller->RotationInput = FRotator::ZeroRotator;
		F.Dispatch(true, EWuwaInputPhase::Triggered, FInputActionValue(FVector2D(10.f, 10.f)));
		F.Dispatch(false, EWuwaInputPhase::Triggered, FInputActionValue(100.f));
		F.Camera->UpdateCamera(1.f / 60.f);
		TestTrue(TEXT("Gameplay look does not alter external camera input"), F.Controller->RotationInput.IsNearlyZero());
		TestTrue(TEXT("External camera supplies its own location"), F.Camera->GetCameraLocation().Equals(ExternalLocation, 0.001));
		TestTrue(TEXT("External camera supplies its own rotation"), F.Camera->GetCameraRotation().Equals(ExternalRotation, 0.001));
		TestTrue(TEXT("External camera supplies its own FOV"), FMath::IsNearlyEqual(F.Camera->GetFOVAngle(), 55.f, 0.001f));
		F.Controller->SetViewTarget(F.Pawn);
		F.Camera->UpdateCamera(1.f / 60.f);
		TestTrue(TEXT("Returning to gameplay does not replay external-view wheel input"), FMath::IsNearlyEqual(F.DistanceToTarget(), 400.0, 0.001));
	}
	return true;
}

#endif
