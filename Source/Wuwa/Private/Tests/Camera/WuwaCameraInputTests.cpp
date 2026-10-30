#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Game/Camera/WuwaCameraRuntimeBridge.h"
#include "Game/Camera/WuwaPlayerCameraManager.h"
#include "Core/Tag/WuwaGameTags.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Input/WuwaInputRouterComponent.h"
#include "Game/Input/WuwaInputTypes.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "UObject/Script.h"

/*
 * 镜头输入的路径：PCM 只把这一帧的转镜头 / 滚轮量攒起来交给 C#，
 * C# 的输入控制器改战斗镜头的目标角度 / 缩放倍率，ControlRotation 再每帧从镜头角度抄过来。
 */
namespace WuwaCameraInputTests
{
	constexpr float FrameTime = 1.f / 60.f;

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
			Controller->SetPawn(Pawn);
			Pawn->SetController(Controller);
			Controller->SetControlRotation(FRotator::ZeroRotator);
			Controller->RegisterInputRouteHandlers();
			LookAction = NewObject<UInputAction>(Controller);
			LookAction->ValueType = EInputActionValueType::Axis2D;
			ZoomAction = NewObject<UInputAction>(Controller);
			ZoomAction->ValueType = EInputActionValueType::Axis1D;
			// 第一次更新时建好 C# 相机系统、套上配置、让屏幕看战斗镜头
			Update();
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
				&& Test.TestNotNull(TEXT("Managed camera runtime class is available"), Camera->RuntimeClass.LoadSynchronous())
				&& Test.TestTrue(TEXT("Screen views the fight camera"), Camera->IsViewingFightCamera());
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

		void Update(int32 Frames = 1) const
		{
			for (int32 Index = 0; Index < Frames; ++Index) Camera->UpdateCamera(FrameTime);
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
	// 滚轮往前 = 拉近（ZoomModifier 减小），往后 = 拉远；按键被清空（FlushPressedKeys）时这一帧攒的量作废。
	// 每帧先按上一帧的 Current 算这一帧的距离，再让 Current 跟上 Desired，所以距离下一帧才变：更新两帧再比
	struct FCase { float Delta; int32 ExpectedSign; bool bFlush; };
	const FCase Cases[] = {{1.f, -1, false}, {-1.f, 1, false}, {1.f, 0, true}};
	for (const FCase& Case : Cases)
	{
		WuwaCameraInputTests::FFixture F;
		if (!F.IsReady(*this)) return false;
		const double Before = F.DistanceToTarget();
		TestTrue(TEXT("Started reaches camera route"), F.Dispatch(false, EWuwaInputPhase::Pressed, FInputActionValue(Case.Delta)));
		TestTrue(TEXT("Triggered reaches camera route"), F.Dispatch(false, EWuwaInputPhase::Triggered, FInputActionValue(Case.Delta)));
		if (Case.bFlush) F.Controller->FlushPressedKeys();
		F.Update(2);
		const double Change = F.DistanceToTarget() - Before;
		const FString What = FString::Printf(TEXT("Wheel %.0f with flush %d changes the distance by %.2f"), Case.Delta, Case.bFlush, Change);
		if (Case.ExpectedSign == 0) TestTrue(What, FMath::IsNearlyZero(Change, 0.01));
		else TestTrue(What, Change * Case.ExpectedSign > 1.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCameraRoutedLookTest,
	"Wuwa.Camera.Input.LookRotatesCameraAndControl",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCameraRoutedLookTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptExecutionGuard;
	WuwaCameraInputTests::FFixture F;
	if (!F.IsReady(*this)) return false;
	const FRotator Start = F.Camera->GetCameraRotation();

	TestTrue(TEXT("Started is acknowledged"), F.Dispatch(true, EWuwaInputPhase::Pressed, FInputActionValue(FVector2D(3.f, 0.f))));
	F.Update();
	TestTrue(TEXT("Started does not rotate the camera"), F.Camera->GetCameraRotation().Equals(Start, 0.001));

	TestTrue(TEXT("Triggered is routed"), F.Dispatch(true, EWuwaInputPhase::Triggered, FInputActionValue(FVector2D(3.f, 0.f))));
	F.Update();
	const FRotator AfterLook = F.Camera->GetCameraRotation();
	TestTrue(TEXT("Look right turns the camera to the right"), FRotator::NormalizeAxis(AfterLook.Yaw - Start.Yaw) > 0.1);
	TestFalse(TEXT("PCM no longer adds look input to the controller"), F.Controller->RotationInput.Yaw > 0.f);
	TestTrue(TEXT("Control rotation is copied from the camera"), F.Controller->GetControlRotation().Equals(AfterLook, 0.01));

	// 俯仰最大 ±89.9
	for (int32 Index = 0; Index < 60; ++Index)
	{
		F.Dispatch(true, EWuwaInputPhase::Triggered, FInputActionValue(FVector2D(0.f, 50.f)));
		F.Update();
	}
	const double Pitch = FRotator::NormalizeAxis(F.Camera->GetCameraRotation().Pitch);
	TestTrue(FString::Printf(TEXT("Pitch stays inside the limit (%.2f)"), Pitch), FMath::Abs(Pitch) <= 89.9 + 0.01);
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
		const double BaseDistance = F.DistanceToTarget();
		F.Dispatch(false, EWuwaInputPhase::Triggered, FInputActionValue(1.f));
		APawn* NewPawn = F.World->SpawnActor<APawn>(FVector(500.f, 0.f, 0.f), FRotator::ZeroRotator);
		if (!TestNotNull(TEXT("Replacement target spawns"), NewPawn)) return false;
		F.Controller->SetPawn(NewPawn);
		NewPawn->SetController(F.Controller);
		F.Pawn = NewPawn;
		F.Update();
		TestTrue(TEXT("Camera follows the new pawn"), F.Camera->IsViewingFightCamera());
		TestTrue(TEXT("New pawn starts with the base distance and no pending old wheel input"),
			FMath::IsNearlyEqual(F.DistanceToTarget(), BaseDistance, 0.01));
	}
	{
		WuwaCameraInputTests::FFixture F;
		if (!F.IsReady(*this)) return false;
		const double BaseDistance = F.DistanceToTarget();
		ACameraActor* External = F.World->SpawnActor<ACameraActor>();
		if (!TestNotNull(TEXT("External camera spawns"), External)) return false;
		const FVector ExternalLocation(1200.f, -600.f, 300.f);
		const FRotator ExternalRotation(-15.f, 45.f, 0.f);
		External->SetActorLocationAndRotation(ExternalLocation, ExternalRotation);
		External->GetCameraComponent()->SetFieldOfView(55.f);
		F.Controller->SetViewTarget(External);
		TestTrue(TEXT("External camera input is consumed"), F.Dispatch(true, EWuwaInputPhase::Triggered, FInputActionValue(FVector2D(10.f, 10.f))));
		F.Dispatch(false, EWuwaInputPhase::Triggered, FInputActionValue(1.f));
		F.Update();
		TestTrue(TEXT("External camera supplies its own location"), F.Camera->GetCameraLocation().Equals(ExternalLocation, 0.001));
		TestTrue(TEXT("External camera supplies its own rotation"), F.Camera->GetCameraRotation().Equals(ExternalRotation, 0.001));
		TestTrue(TEXT("External camera supplies its own FOV"), FMath::IsNearlyEqual(F.Camera->GetFOVAngle(), 55.f, 0.001f));
		F.Controller->SetViewTarget(F.Camera->GetCameraRuntime()->GetFightCameraActor());
		F.Update();
		TestTrue(TEXT("Returning to the fight camera does not replay external-view wheel input"),
			FMath::IsNearlyEqual(F.DistanceToTarget(), BaseDistance, 0.01));
	}
	return true;
}

#endif
