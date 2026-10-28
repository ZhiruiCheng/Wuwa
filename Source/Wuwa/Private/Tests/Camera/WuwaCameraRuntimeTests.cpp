#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Game/Camera/WuwaCameraMode.h"
#include "Game/Camera/WuwaCameraRuntimeBridge.h"
#include "UObject/StrongObjectPtr.h"

namespace WuwaCameraRuntimeTests
{
	TStrongObjectPtr<UWuwaCameraRuntimeBridge> CreateRuntime()
	{
		UClass* RuntimeClass = LoadClass<UWuwaCameraRuntimeBridge>(nullptr,
			TEXT("/Script/UnrealSharp.WuwaCameraRuntime_C"));
		return TStrongObjectPtr<UWuwaCameraRuntimeBridge>(RuntimeClass
			? NewObject<UWuwaCameraRuntimeBridge>(GetTransientPackage(), RuntimeClass) : nullptr);
	}

	FWuwaCameraSettings SnapSettings()
	{
		FWuwaCameraSettings Settings;
		Settings.Zoom.ZoomInterpSpeed = 0.f;
		Settings.Blend.BlendTime = 0.f;
		return Settings;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCameraFollowMathTest,
	"Wuwa.Camera.Runtime.FollowPose",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCameraFollowMathTest::RunTest(const FString& Parameters)
{
	auto Runtime = WuwaCameraRuntimeTests::CreateRuntime();
	if (!TestNotNull(TEXT("Managed camera policy loads through its native bridge"), Runtime.Get()))
	{
		return false;
	}
	FWuwaCameraSettings Settings = WuwaCameraRuntimeTests::SnapSettings();
	FWuwaCameraFrame Frame;
	Frame.TargetLocation = FVector(1000., 2000., 300.);
	Frame.ControlRotation = FRotator(0., 90., 0.);
	Settings.Offset.PivotOffset = FVector(0., 0., 70.);
	const FWuwaCameraView View = Runtime->EvaluateCamera(Frame, Settings);
	TestTrue(TEXT("Follow policy emits a valid view"), View.bValid);
	TestTrue(TEXT("World pivot offset is independent of target yaw"), View.Pivot.Equals(FVector(1000., 2000., 370.), 0.001));
	TestTrue(TEXT("Yaw 90 places camera behind the target along negative Y"), View.Location.Equals(FVector(1000., 1600., 370.), 0.001));
	TestTrue(TEXT("Control rotation reaches the view unchanged"), View.Rotation.Equals(Frame.ControlRotation, 0.001));
	TestEqual(TEXT("Default FOV is preserved"), View.FieldOfView, 90.f);

	Frame.ControlRotation = FRotator(-30., 0., 0.);
	const FWuwaCameraView PitchedView = Runtime->EvaluateCamera(Frame, Settings);
	TestTrue(TEXT("Looking down positions the camera above the pivot"),
		PitchedView.Location.Equals(View.Pivot - Frame.ControlRotation.Vector() * Settings.Arm.ArmLength, 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCameraZoomInputTest,
	"Wuwa.Camera.Runtime.ZoomDeltaAndLimits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCameraZoomInputTest::RunTest(const FString& Parameters)
{
	auto Runtime = WuwaCameraRuntimeTests::CreateRuntime();
	if (!TestNotNull(TEXT("Managed camera runtime loads"), Runtime.Get()))
	{
		return false;
	}
	const FWuwaCameraSettings Settings = WuwaCameraRuntimeTests::SnapSettings();
	FWuwaCameraFrame Frame;
	Frame.DeltaSeconds = 1.f / 240.f;
	Frame.ZoomDelta = 1.f;
	TestEqual(TEXT("One wheel step shortens the arm by ZoomStep"), Runtime->EvaluateCamera(Frame, Settings).ArmLength, 350.f);
	Frame.ZoomDelta = 0.f;
	TestEqual(TEXT("A subsequent frame does not replay an old wheel step"), Runtime->EvaluateCamera(Frame, Settings).ArmLength, 350.f);
	Runtime->ResetCamera();
	Frame.DeltaSeconds = 1.f / 15.f;
	Frame.ZoomDelta = 1.f;
	TestEqual(TEXT("Raw wheel displacement does not scale with frame time"), Runtime->EvaluateCamera(Frame, Settings).ArmLength, 350.f);
	Frame.ZoomDelta = 100.f;
	TestEqual(TEXT("Large positive input clamps at minimum distance"), Runtime->EvaluateCamera(Frame, Settings).ArmLength, Settings.Arm.MinArmLength);
	Frame.ZoomDelta = -100.f;
	TestEqual(TEXT("Large negative input clamps at maximum distance"), Runtime->EvaluateCamera(Frame, Settings).ArmLength, Settings.Arm.MaxArmLength);
	Runtime->ResetCamera();
	Frame.ZoomDelta = 0.f;
	TestEqual(TEXT("Reset restores user zoom preference"), Runtime->EvaluateCamera(Frame, Settings).ArmLength, Settings.Arm.ArmLength);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCameraModeOwnershipTest,
	"Wuwa.Camera.Runtime.ModeOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCameraModeOwnershipTest::RunTest(const FString& Parameters)
{
	auto Runtime = WuwaCameraRuntimeTests::CreateRuntime();
	if (!TestNotNull(TEXT("Managed camera runtime loads"), Runtime.Get()))
	{
		return false;
	}
	const FWuwaCameraSettings Base = WuwaCameraRuntimeTests::SnapSettings();
	FWuwaCameraSettings Low = Base;
	Low.Arm.ArmLength = 300.f;
	FWuwaCameraSettings High = Base;
	High.Arm.ArmLength = 200.f;
	FWuwaCameraSettings Latest = Base;
	Latest.Arm.ArmLength = 600.f;
	TStrongObjectPtr<UObject> LowOwner(NewObject<UWuwaCameraMode>());
	TStrongObjectPtr<UObject> HighOwner(NewObject<UWuwaCameraMode>());
	TStrongObjectPtr<UObject> LatestOwner(NewObject<UWuwaCameraMode>());
	TestEqual(TEXT("Invalid owner cannot acquire a camera mode"), Runtime->PushCameraMode(nullptr, High, 999), 0);
	const int32 LowHandle = Runtime->PushCameraMode(LowOwner.Get(), Low, 1);
	const int32 HighHandle = Runtime->PushCameraMode(HighOwner.Get(), High, 5);
	const int32 LatestHandle = Runtime->PushCameraMode(LatestOwner.Get(), Latest, 5);
	TestTrue(TEXT("Each accepted request receives a distinct nonzero handle"), LowHandle > 0 && HighHandle > LowHandle && LatestHandle > HighHandle);
	TestEqual(TEXT("Latest request wins equal priorities"), Runtime->ResolveSettings(Base).Arm.ArmLength, Latest.Arm.ArmLength);
	TestTrue(TEXT("A lower priority request can be released independently"), Runtime->PopCameraMode(LowHandle));
	TestEqual(TEXT("Releasing another owner does not release current mode"), Runtime->ResolveSettings(Base).Arm.ArmLength, Latest.Arm.ArmLength);
	TestFalse(TEXT("Duplicate release is harmless"), Runtime->PopCameraMode(LowHandle));
	TestTrue(TEXT("Latest mode can be released"), Runtime->PopCameraMode(LatestHandle));
	TestEqual(TEXT("Previous live high-priority request resumes"), Runtime->ResolveSettings(Base).Arm.ArmLength, High.Arm.ArmLength);
	HighOwner->MarkAsGarbage();
	TestEqual(TEXT("Destroyed source is pruned without an explicit release"), Runtime->ResolveSettings(Base).Arm.ArmLength, Base.Arm.ArmLength);
	TestFalse(TEXT("Expired mode handle no longer removes anything"), Runtime->PopCameraMode(HighHandle));

	const int32 BeforeReset = Runtime->PushCameraMode(LatestOwner.Get(), High, 5);
	Runtime->ResetCamera();
	TestEqual(TEXT("Reset removes active modes"), Runtime->ResolveSettings(Base).Arm.ArmLength, Base.Arm.ArmLength);
	const int32 AfterReset = Runtime->PushCameraMode(LatestOwner.Get(), Latest, 5);
	TestTrue(TEXT("Reset never recycles old ownership handles"), AfterReset > BeforeReset);
	TestFalse(TEXT("Late release from before reset cannot remove a new mode"), Runtime->PopCameraMode(BeforeReset));
	TestEqual(TEXT("New mode survives stale release"), Runtime->ResolveSettings(Base).Arm.ArmLength, Latest.Arm.ArmLength);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCameraSkillZoomRestoreTest,
	"Wuwa.Camera.Runtime.SkillRestoresUserZoom",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCameraSkillZoomRestoreTest::RunTest(const FString& Parameters)
{
	auto Runtime = WuwaCameraRuntimeTests::CreateRuntime();
	if (!TestNotNull(TEXT("Managed camera runtime loads"), Runtime.Get()))
	{
		return false;
	}
	const FWuwaCameraSettings Base = WuwaCameraRuntimeTests::SnapSettings();
	FWuwaCameraFrame Frame;
	Frame.ZoomDelta = 1.f;
	TestEqual(TEXT("Exploration camera records player zoom"), Runtime->EvaluateCamera(Frame, Base).ArmLength, 350.f);
	FWuwaCameraSettings Skill = Base;
	Skill.Arm.ArmLength = 200.f;
	Skill.Zoom.bUseUserZoom = false;
	Skill.Zoom.bAllowZoomInput = false;
	Skill.Rotation.bAllowRotationInput = false;
	TStrongObjectPtr<UObject> Ability(NewObject<UWuwaCameraMode>());
	const int32 Handle = Runtime->PushCameraMode(Ability.Get(), Skill, 100);
	Frame.ZoomDelta = -100.f;
	TestEqual(TEXT("Skill distance is independent of player zoom and wheel input"), Runtime->EvaluateCamera(Frame, Base).ArmLength, 200.f);
	TestFalse(TEXT("Skill advertises its rotation input lock"), Runtime->ResolveSettings(Base).Rotation.bAllowRotationInput);
	TestTrue(TEXT("Ability releases its own request"), Runtime->PopCameraMode(Handle));
	Frame.ZoomDelta = 0.f;
	TestEqual(TEXT("Exploration resumes the player's exact previous zoom"), Runtime->EvaluateCamera(Frame, Base).ArmLength, 350.f);
	TestTrue(TEXT("Exploration rotation input is restored"), Runtime->ResolveSettings(Base).Rotation.bAllowRotationInput);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCameraFramePartitionTest,
	"Wuwa.Camera.Runtime.FrameIndependentSmoothing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCameraFramePartitionTest::RunTest(const FString& Parameters)
{
	auto OneStep = WuwaCameraRuntimeTests::CreateRuntime();
	auto FiveSteps = WuwaCameraRuntimeTests::CreateRuntime();
	if (!TestNotNull(TEXT("First runtime loads"), OneStep.Get())
		|| !TestNotNull(TEXT("Second runtime loads"), FiveSteps.Get()))
	{
		return false;
	}
	FWuwaCameraSettings Settings = WuwaCameraRuntimeTests::SnapSettings();
	Settings.Zoom.ZoomInterpSpeed = 12.f;
	FWuwaCameraFrame Frame;
	OneStep->EvaluateCamera(Frame, Settings);
	FiveSteps->EvaluateCamera(Frame, Settings);
	Frame.ZoomDelta = 1.f;
	Frame.DeltaSeconds = 0.1f;
	const FWuwaCameraView LargeFrame = OneStep->EvaluateCamera(Frame, Settings);
	Frame.DeltaSeconds = 0.02f;
	FWuwaCameraView SmallFrames = FiveSteps->EvaluateCamera(Frame, Settings);
	Frame.ZoomDelta = 0.f;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		SmallFrames = FiveSteps->EvaluateCamera(Frame, Settings);
	}
	TestTrue(TEXT("Same elapsed time reaches the same smoothed zoom"),
		FMath::IsNearlyEqual(LargeFrame.ArmLength, SmallFrames.ArmLength, 0.001f));
	TestTrue(TEXT("Zoom converges without jumping or overshooting"), LargeFrame.ArmLength > 350.f && LargeFrame.ArmLength < 400.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCameraBlendFollowsTargetTest,
	"Wuwa.Camera.Runtime.ModeBlendFollowsMovingTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCameraBlendFollowsTargetTest::RunTest(const FString& Parameters)
{
	auto Runtime = WuwaCameraRuntimeTests::CreateRuntime();
	if (!TestNotNull(TEXT("Managed camera runtime loads"), Runtime.Get()))
	{
		return false;
	}
	const FWuwaCameraSettings Base = WuwaCameraRuntimeTests::SnapSettings();
	FWuwaCameraFrame Frame;
	Runtime->EvaluateCamera(Frame, Base);
	FWuwaCameraSettings Skill = Base;
	Skill.Arm.ArmLength = 200.f;
	Skill.Blend.BlendTime = .2f;
	TStrongObjectPtr<UObject> Owner(NewObject<UWuwaCameraMode>());
	Runtime->PushCameraMode(Owner.Get(), Skill, 10);
	Frame.DeltaSeconds = .1f;
	Frame.TargetLocation = FVector(1000., 0., 0.);
	const FWuwaCameraView Halfway = Runtime->EvaluateCamera(Frame, Base);
	TestTrue(TEXT("Halfway profile blend interpolates distance"), FMath::IsNearlyEqual(Halfway.ArmLength, 300.f, 0.001f));
	TestTrue(TEXT("Pivot uses current target position throughout the blend"), Halfway.Pivot.Equals(Frame.TargetLocation, 0.001));
	TestTrue(TEXT("World movement is not mistaken for a camera parameter transition"), Halfway.Location.Equals(FVector(700., 0., 0.), 0.001));
	return true;
}

#endif
