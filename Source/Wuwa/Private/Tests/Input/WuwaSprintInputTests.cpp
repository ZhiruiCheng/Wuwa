#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/Movement/WuwaTestGait.h"
#include "Tests/Input/WuwaTestMoveInput.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Engine/World.h"
#include "Game/Input/DataAsset/WuwaInputDataAsset.h"
#include "Game/Input/WuwaInputRouterComponent.h"
#include "Game/Input/WuwaInputTypes.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaPlayerInputState.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputIntentComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Common/WuwaGameTags.h"

namespace
{
	struct FScopedSprintInputWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AWuwaPlayerController* Controller = nullptr;

		FScopedSprintInputWorld()
		{
			if (World)
			{
				FActorSpawnParameters Parameters;
				Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				Parameters.ObjectFlags |= RF_Transient;
				Controller = World->SpawnActor<AWuwaPlayerController>(Parameters);
			}
		}

		~FScopedSprintInputWorld()
		{
			if (IsValid(Controller))
			{
				Controller->SetPawn(nullptr);
			}
			if (World)
			{
				World->DestroyWorld(false);
			}
		}

		AWuwaCharacter* SpawnCharacter() const
		{
			if (!World)
			{
				return nullptr;
			}
			FActorSpawnParameters Parameters;
			Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Parameters.ObjectFlags |= RF_Transient;
			return World->SpawnActor<AWuwaCharacter>(Parameters);
		}

		UWuwaInputRouterComponent* Router() const
		{
			return Controller ? Controller->GetInputRouter() : nullptr;
		}

		bool Dispatch(const FInputDataAsset& Binding, EWuwaInputPhase Phase, double Time) const
		{
			World->TimeSeconds = Time;
			// Match PlayerController::HandleRoutedInput: build the complete event, then let the controller
			// record it on the controlled Pawn's input intent and dispatch it through the router.
			FWuwaInputEvent Event;
			Event.InputTag = Binding.InputTag;
			Event.RouteTag = Binding.RouteTag;
			Event.SourceAction = Binding.InputAction;
			Event.Phase = Phase;
			Event.Timestamp = Time;
			return Controller->RouteInputEvent(Event);
		}
	};

	FInputDataAsset MakeSprintBinding(UObject* Outer, FGameplayTag Tag, FGameplayTag Route)
	{
		FInputDataAsset Binding;
		Binding.InputAction = NewObject<UInputAction>(Outer);
		Binding.InputTag = Tag;
		Binding.RouteTag = Route;
		return Binding;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSprintInputSourcesTest, "Wuwa.Input.Sprint.SemanticHoldSources",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSprintInputSourcesTest::RunTest(const FString& Parameters)
{
	FScopedSprintInputWorld Fixture;
	AWuwaCharacter* Character = Fixture.SpawnCharacter();
	if (!TestNotNull(TEXT("The controller owns a router"), Fixture.Router())
		|| !TestNotNull(TEXT("A character receives the semantic input"), Character)
		|| !TestNotNull(TEXT("The character owns an input intent component"), Character->InputIntent.Get()))
	{
		return false;
	}
	// Held state belongs to the controlled character, not to the controller's router.
	Fixture.Controller->SetPawn(Character);
	Character->Controller = Fixture.Controller;
	UWuwaInputIntentComponent* Intent = Character->InputIntent;
	const FWuwaGameTags Tags = FWuwaGameTags::Get();
	const FInputDataAsset First = MakeSprintBinding(Fixture.Controller, Tags.Input_Combat_Dash, Tags.Input_Route_Ability);
	const FInputDataAsset Second = MakeSprintBinding(Fixture.Controller, Tags.Input_Combat_Dash, Tags.Input_Route_Ability);
	if (!TestTrue(TEXT("The semantic input binding is configured"), First.IsConfigured()))
	{
		return false;
	}

	// State must be recorded independently of whether a gameplay handler accepts the command.
	TestFalse(TEXT("Without a handler, dispatch still reports unhandled"), Fixture.Dispatch(First, EWuwaInputPhase::Pressed, 1.0));
	TestTrue(TEXT("An unhandled action still has a held input state"), Intent->GetActionState(First.InputTag).bHeld);
	Fixture.Dispatch(First, EWuwaInputPhase::Pressed, 1.1);
	Fixture.Dispatch(First, EWuwaInputPhase::Triggered, 1.12);
	Fixture.Dispatch(Second, EWuwaInputPhase::Pressed, 1.15);
	Fixture.Dispatch(First, EWuwaInputPhase::Released, 1.2);
	Fixture.World->TimeSeconds = 1.3;
	const FWuwaInputActionState Overlapping = Intent->GetActionState(First.InputTag);
	TestTrue(TEXT("Releasing one action leaves the other source held"), Overlapping.bHeld);
	TestTrue(TEXT("Duplicate presses, triggers and overlapping sources preserve continuous semantic hold time"),
		FMath::IsNearlyEqual(Overlapping.HeldSeconds, 0.3f, 0.0001f));

	Fixture.Dispatch(Second, EWuwaInputPhase::Released, 1.4);
	TestFalse(TEXT("Releasing the final source clears the semantic hold"), Intent->GetActionState(First.InputTag).bHeld);
	TestEqual(TEXT("A released action reports zero held time"), Intent->GetActionState(First.InputTag).HeldSeconds, 0.f);
	Fixture.Dispatch(Second, EWuwaInputPhase::Triggered, 1.5);
	TestFalse(TEXT("A late Triggered event cannot resurrect a released action"), Intent->GetActionState(First.InputTag).bHeld);
	Fixture.Dispatch(Second, EWuwaInputPhase::Pressed, 1.6);
	Fixture.Dispatch(Second, EWuwaInputPhase::Canceled, 1.7);
	TestFalse(TEXT("Canceled clears the source just like release"), Intent->GetActionState(First.InputTag).bHeld);

	// The intent records the complete event it receives, keyed by the event's tag and source action.
	Fixture.World->TimeSeconds = 2.0;
	FWuwaInputEvent OtherEvent;
	OtherEvent.InputTag = Tags.Player_Common_Movement_WalkRun;
	OtherEvent.SourceAction = NewObject<UInputAction>(Fixture.Controller);
	OtherEvent.Phase = EWuwaInputPhase::Pressed;
	OtherEvent.Timestamp = 2.0;
	Fixture.Controller->RouteInputEvent(OtherEvent);
	TestTrue(TEXT("An event is recorded under the tag it carries"), Intent->GetActionState(OtherEvent.InputTag).bHeld);
	TestFalse(TEXT("Another semantic action is not affected"), Intent->GetActionState(First.InputTag).bHeld);
	Fixture.Dispatch(First, EWuwaInputPhase::Released, 2.1);
	TestTrue(TEXT("Releasing a different action cannot clear this hold"), Intent->GetActionState(OtherEvent.InputTag).bHeld);
	OtherEvent.Phase = EWuwaInputPhase::Released;
	OtherEvent.Timestamp = 2.2;
	Fixture.Controller->RouteInputEvent(OtherEvent);
	TestFalse(TEXT("Releasing the recorded source clears the hold"), Intent->GetActionState(OtherEvent.InputTag).bHeld);

	Fixture.Dispatch(First, EWuwaInputPhase::Pressed, 3.0);
	Intent->ResetInputs();
	TestFalse(TEXT("Reset clears all action states"), Intent->GetActionState(First.InputTag).bHeld);
	Fixture.Dispatch(First, EWuwaInputPhase::Triggered, 3.1);
	TestFalse(TEXT("Triggered cannot recreate an input cleared by reset"), Intent->GetActionState(First.InputTag).bHeld);
	TestFalse(TEXT("An invalid tag has no held input"), Intent->GetActionState(FGameplayTag()).bHeld);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSprintInputLifecycleTest, "Wuwa.Input.Sprint.ConfigurationAndLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSprintInputLifecycleTest::RunTest(const FString& Parameters)
{
	FScopedSprintInputWorld Fixture;
	if (!TestNotNull(TEXT("A controller was spawned"), Fixture.Controller)
		|| !TestNotNull(TEXT("The controller has a router"), Fixture.Router()))
	{
		return false;
	}
	const FWuwaGameTags Tags = FWuwaGameTags::Get();
	const FInputDataAsset Dash = MakeSprintBinding(Fixture.Controller, Tags.Input_Combat_Dash, Tags.Input_Route_Ability);
	Fixture.Controller->RegisterInputRouteHandlers();

	// Without a controlled Pawn there is nowhere to record semantic input; routing must still be safe.
	TestNull(TEXT("The fixture starts without an ASC"), Fixture.Controller->GetASC());
	Fixture.Dispatch(Dash, EWuwaInputPhase::Pressed, 1.0);
	Fixture.World->TimeSeconds = 1.3;
	Fixture.Controller->FlushPressedKeys();

	AWuwaCharacter* FirstCharacter = Fixture.SpawnCharacter();
	AWuwaCharacter* SecondCharacter = Fixture.SpawnCharacter();
	if (!TestNotNull(TEXT("The first character was spawned"), FirstCharacter)
		|| !TestNotNull(TEXT("The second character was spawned"), SecondCharacter))
	{
		return false;
	}
	Fixture.Dispatch(Dash, EWuwaInputPhase::Pressed, 1.31);
	// Establish the snapshot association without Possess/PlayerState initialization or a world tick.
	Fixture.Controller->SetPawn(FirstCharacter);
	FirstCharacter->Controller = Fixture.Controller;
	TestFalse(TEXT("Taking control of a Pawn discards input held before possession"), FirstCharacter->GetPlayerInputState().bSprintHeld);
	Fixture.Dispatch(Dash, EWuwaInputPhase::Pressed, 1.4);
	Fixture.World->TimeSeconds = 1.6;
	TestTrue(TEXT("A controlled Pawn receives the default Dash semantic input as its sprint request"), FirstCharacter->GetPlayerInputState().bSprintHeld);
	FWuwaPlayerInputState Snapshot = FirstCharacter->GetPlayerInputState();
	TestTrue(TEXT("The character snapshot carries the routed sprint hold"), Snapshot.bSprintHeld);
	TestTrue(TEXT("The character snapshot carries the action's held duration"),
		FMath::IsNearlyEqual(Snapshot.SprintHeldSeconds, 0.2f, 0.0001f));
	TestFalse(TEXT("An uncontrolled character receives no sprint hold"), SecondCharacter->GetPlayerInputState().bSprintHeld);

	UWuwaMovementComponent* Movement = FirstCharacter->GetWuwaMovementComponent();
	if (!TestNotNull(TEXT("The character has a movement component for its notify window"), Movement))
	{
		return false;
	}
	Movement->MovementMode = MOVE_Walking;
	// 未初始化 Actor 的测试世界不会调用 PostInitializeComponents：设好移动模式后显式装配运动状态。
	if (!TestTrue(TEXT("The character assembles its movement state"), FirstCharacter->EnsureMovementStateSystem())) return false;
	UObject* WindowSource = NewObject<UInputAction>(FirstCharacter);
	WuwaTestGait::Of(Movement)->OpenSprintWindow(WindowSource);
	Fixture.World->TimeSeconds = 1.7;
	Snapshot = FirstCharacter->GetPlayerInputState();
	WuwaTestGait::Of(Movement)->SampleSprintWindow(WindowSource, Snapshot.bSprintHeld ? Snapshot.SprintHeldSeconds : 0.f, 0.2f);
	TestTrue(TEXT("The snapshot's pre-window hold cannot skip the 0.2 second window requirement"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Temporary);
	Fixture.World->TimeSeconds = 1.81;
	Snapshot = FirstCharacter->GetPlayerInputState();
	WuwaTestGait::Of(Movement)->SampleSprintWindow(WindowSource, Snapshot.bSprintHeld ? Snapshot.SprintHeldSeconds : 0.f, 0.2f);
	TestTrue(TEXT("More than 0.2 seconds of window hold reaches sustained desire through the snapshot"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Sustained);
	Fixture.Dispatch(Dash, EWuwaInputPhase::Released, 1.82);
	Snapshot = FirstCharacter->GetPlayerInputState();
	TestFalse(TEXT("Release propagates to the character snapshot"), Snapshot.bSprintHeld);
	TestEqual(TEXT("The released snapshot clears held duration"), Snapshot.SprintHeldSeconds, 0.f);
	WuwaTestGait::Of(Movement)->SampleSprintWindow(WindowSource, Snapshot.bSprintHeld ? Snapshot.SprintHeldSeconds : 0.f, 0.2f);
	TestTrue(TEXT("A released semantic action demotes an open window to temporary desire"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Temporary);
	WuwaTestGait::Of(Movement)->CloseSprintWindow(WindowSource);
	TestTrue(TEXT("The completed temporary window retains its sprint request"), WuwaTestGait::Of(Movement)->ReadSprintDesire() == EWuwaSprintDesire::Temporary);

	Fixture.Dispatch(Dash, EWuwaInputPhase::Pressed, 3.0);
	Fixture.Controller->SetPawn(SecondCharacter);
	SecondCharacter->Controller = Fixture.Controller;
	TestFalse(TEXT("The former Pawn cannot read input through its stale controller pointer"), FirstCharacter->GetPlayerInputState().bSprintHeld);
	TestFalse(TEXT("The newly controlled Pawn starts with an empty sprint snapshot"), SecondCharacter->GetPlayerInputState().bSprintHeld);
	Fixture.Dispatch(Dash, EWuwaInputPhase::Triggered, 3.1);
	TestFalse(TEXT("A stale Triggered callback cannot restart sprint after switching Pawn"), SecondCharacter->GetPlayerInputState().bSprintHeld);

	// Existing semantic commands continue through the same router and affect only the current Pawn.
	FirstCharacter->GetWuwaMovementComponent()->MovementMode = MOVE_Walking;
	SecondCharacter->GetWuwaMovementComponent()->MovementMode = MOVE_Walking;
	if (!TestTrue(TEXT("The second character assembles its movement state"), SecondCharacter->EnsureMovementStateSystem())) return false;
	WuwaTestInput::UseTestMoveInputConfig(Fixture.Controller);
	const FInputDataAsset WalkRun = MakeSprintBinding(Fixture.Controller, Tags.Player_Common_Movement_WalkRun, Tags.Input_Route_Movement);
	TestTrue(TEXT("Walk/run is still handled by the existing movement command route"), Fixture.Dispatch(WalkRun, EWuwaInputPhase::Pressed, 4.0));
	TestTrue(TEXT("The controlled character switches to Walk"), SecondCharacter->GetWuwaMovementComponent()->GetDesiredGait() == EWuwaGait::Walk);
	TestTrue(TEXT("The other character retains Run"), FirstCharacter->GetWuwaMovementComponent()->GetDesiredGait() == EWuwaGait::Run);
	TestFalse(TEXT("Walk/run input does not become a sprint request"), SecondCharacter->GetPlayerInputState().bSprintHeld);
	Fixture.Dispatch(WalkRun, EWuwaInputPhase::Released, 4.1);
	TestTrue(TEXT("Releasing walk/run does not toggle the gait again"), SecondCharacter->GetWuwaMovementComponent()->GetDesiredGait() == EWuwaGait::Walk);

	Fixture.Dispatch(Dash, EWuwaInputPhase::Pressed, 5.0);
	TestTrue(TEXT("A fresh press reaches the newly controlled character"), SecondCharacter->GetPlayerInputState().bSprintHeld);
	TestFalse(TEXT("A fresh press does not leak to the previous character"), FirstCharacter->GetPlayerInputState().bSprintHeld);
	Fixture.Controller->FlushPressedKeys();
	TestFalse(TEXT("Flushing input clears the current character snapshot"), SecondCharacter->GetPlayerInputState().bSprintHeld);
	Fixture.Dispatch(Dash, EWuwaInputPhase::Pressed, 5.1);
	Fixture.Controller->SetPawn(nullptr);
	TestFalse(TEXT("An unpossessed character exposes no held sprint input"), SecondCharacter->GetPlayerInputState().bSprintHeld);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSprintRetainedInputLifecycleTest, "Wuwa.Input.Sprint.RetainedDesireControlReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSprintRetainedInputLifecycleTest::RunTest(const FString& Parameters)
{
	FScopedSprintInputWorld Fixture;
	AWuwaCharacter* FirstCharacter = Fixture.SpawnCharacter();
	AWuwaCharacter* SecondCharacter = Fixture.SpawnCharacter();
	if (!TestNotNull(TEXT("The retained desire fixture has a controller"), Fixture.Controller)
		|| !TestNotNull(TEXT("The retained desire fixture has a router"), Fixture.Router())
		|| !TestNotNull(TEXT("The first character exists"), FirstCharacter)
		|| !TestNotNull(TEXT("The second character exists"), SecondCharacter))
	{
		return false;
	}
	UWuwaMovementComponent* FirstMovement = FirstCharacter->GetWuwaMovementComponent();
	UWuwaMovementComponent* SecondMovement = SecondCharacter->GetWuwaMovementComponent();
	if (!TestNotNull(TEXT("The first character owns movement"), FirstMovement)
		|| !TestNotNull(TEXT("The second character owns movement"), SecondMovement))
	{
		return false;
	}
	FirstMovement->MovementMode = MOVE_Walking;
	SecondMovement->MovementMode = MOVE_Walking;
	// 未初始化 Actor 的测试世界不会调用 PostInitializeComponents：设好移动模式后显式装配运动状态。
	if (!TestTrue(TEXT("The first character assembles its movement state"), FirstCharacter->EnsureMovementStateSystem())) return false;
	if (!TestTrue(TEXT("The second character assembles its movement state"), SecondCharacter->EnsureMovementStateSystem())) return false;
	Fixture.Controller->RegisterInputRouteHandlers();
	Fixture.Controller->SetPawn(FirstCharacter);
	FirstCharacter->Controller = Fixture.Controller;
	const FWuwaGameTags Tags = FWuwaGameTags::Get();
	const FInputDataAsset Dash = MakeSprintBinding(Fixture.Controller, Tags.Input_Combat_Dash, Tags.Input_Route_Ability);
	UObject* CompletedWindow = NewObject<UInputAction>(FirstCharacter);
	UObject* OpenWindow = NewObject<UInputAction>(FirstCharacter);
	const FInputActionValue MovingInput(FVector2D(0.f, 1.f));
	const FInputActionValue StoppedInput(FVector2D::ZeroVector);
	const auto SampleWindow = [](AWuwaCharacter* Character, UObject* Source)
	{
		const FWuwaPlayerInputState Snapshot = Character->GetPlayerInputState();
		WuwaTestGait::Of(Character->GetWuwaMovementComponent())->SampleSprintWindow(
			Source, Snapshot.bSprintHeld ? Snapshot.SprintHeldSeconds : 0.f, 0.2f);
	};

	// The notify's animation branch may stop updating after End; committed desire belongs to movement.
	WuwaTestInput::SetMoveAxis(FirstCharacter, MovingInput.Get<FVector2D>());
	Fixture.Dispatch(Dash, EWuwaInputPhase::Pressed, 10.0);
	WuwaTestGait::Of(FirstMovement)->OpenSprintWindow(CompletedWindow);
	Fixture.World->TimeSeconds = 10.3;
	SampleWindow(FirstCharacter, CompletedWindow);
	WuwaTestGait::Of(FirstMovement)->CloseSprintWindow(CompletedWindow);
	TestTrue(TEXT("Ending a held window commits sustained desire"), WuwaTestGait::Of(FirstMovement)->ReadSprintDesire() == EWuwaSprintDesire::Sustained);
	Fixture.Dispatch(Dash, EWuwaInputPhase::Released, 10.4);
	TestFalse(TEXT("The sprint input snapshot reflects the subsequent release"), FirstCharacter->GetPlayerInputState().bSprintHeld);
	TestTrue(TEXT("Releasing sprint after the window does not demote the committed desire"), WuwaTestGait::Of(FirstMovement)->ReadSprintDesire() == EWuwaSprintDesire::Sustained);
	FirstMovement->Velocity = FVector(450.f, 0.f, 0.f);
	WuwaTestInput::SetMoveAxis(FirstCharacter, StoppedInput.Get<FVector2D>());
	TestTrue(TEXT("Releasing movement clears sustained desire despite residual character velocity"), WuwaTestGait::Of(FirstMovement)->ReadSprintDesire() == EWuwaSprintDesire::None);
	Fixture.Dispatch(Dash, EWuwaInputPhase::Pressed, 10.5);
	WuwaTestGait::Of(FirstMovement)->OpenSprintWindow(OpenWindow);
	Fixture.World->TimeSeconds = 10.8;
	SampleWindow(FirstCharacter, OpenWindow);
	WuwaTestGait::Of(FirstMovement)->CloseSprintWindow(OpenWindow);
	TestTrue(TEXT("A later window End cannot commit sustained desire while movement remains released"), WuwaTestGait::Of(FirstMovement)->ReadSprintDesire() == EWuwaSprintDesire::None);
	Fixture.Dispatch(Dash, EWuwaInputPhase::Released, 10.9);

	// Flush must discard both a committed request and an unrelated still-open sampling window.
	WuwaTestInput::SetMoveAxis(FirstCharacter, MovingInput.Get<FVector2D>());
	Fixture.Dispatch(Dash, EWuwaInputPhase::Pressed, 11.0);
	WuwaTestGait::Of(FirstMovement)->OpenSprintWindow(CompletedWindow);
	Fixture.World->TimeSeconds = 11.3;
	SampleWindow(FirstCharacter, CompletedWindow);
	WuwaTestGait::Of(FirstMovement)->CloseSprintWindow(CompletedWindow);
	WuwaTestGait::Of(FirstMovement)->OpenSprintWindow(OpenWindow);
	TestTrue(TEXT("A weaker open window does not replace retained sustained desire"), WuwaTestGait::Of(FirstMovement)->ReadSprintDesire() == EWuwaSprintDesire::Sustained);
	Fixture.Controller->FlushPressedKeys();
	TestFalse(TEXT("Flush clears cached movement intent"), FirstCharacter->GetPlayerInputState().bHasMoveInput);
	TestTrue(TEXT("Flush clears both open and retained sprint desire"), WuwaTestGait::Of(FirstMovement)->ReadSprintDesire() == EWuwaSprintDesire::None);
	Fixture.World->TimeSeconds = 11.6;
	WuwaTestGait::Of(FirstMovement)->SampleSprintWindow(OpenWindow, 1.f, 0.2f);
	WuwaTestGait::Of(FirstMovement)->CloseSprintWindow(OpenWindow);
	WuwaTestGait::Of(FirstMovement)->CloseSprintWindow(CompletedWindow);
	TestTrue(TEXT("Late notify callbacks cannot restore desire after flush"), WuwaTestGait::Of(FirstMovement)->ReadSprintDesire() == EWuwaSprintDesire::None);

	// Changing Pawn resets the previous character even when its animation never sends another End.
	WuwaTestInput::SetMoveAxis(FirstCharacter, MovingInput.Get<FVector2D>());
	Fixture.Dispatch(Dash, EWuwaInputPhase::Pressed, 12.0);
	WuwaTestGait::Of(FirstMovement)->OpenSprintWindow(CompletedWindow);
	Fixture.World->TimeSeconds = 12.3;
	SampleWindow(FirstCharacter, CompletedWindow);
	WuwaTestGait::Of(FirstMovement)->CloseSprintWindow(CompletedWindow);
	WuwaTestGait::Of(FirstMovement)->OpenSprintWindow(OpenWindow);
	Fixture.Controller->SetPawn(SecondCharacter);
	SecondCharacter->Controller = Fixture.Controller;
	TestFalse(TEXT("Pawn change clears the previous character's cached move intent"), FirstCharacter->GetPlayerInputState().bHasMoveInput);
	TestTrue(TEXT("Pawn change clears open and retained desire on the previous character"), WuwaTestGait::Of(FirstMovement)->ReadSprintDesire() == EWuwaSprintDesire::None);
	TestTrue(TEXT("The new character does not inherit sprint desire"), WuwaTestGait::Of(SecondMovement)->ReadSprintDesire() == EWuwaSprintDesire::None);
	Fixture.World->TimeSeconds = 12.6;
	WuwaTestGait::Of(FirstMovement)->SampleSprintWindow(OpenWindow, 1.f, 0.2f);
	WuwaTestGait::Of(FirstMovement)->CloseSprintWindow(OpenWindow);
	WuwaTestGait::Of(FirstMovement)->CloseSprintWindow(CompletedWindow);
	TestTrue(TEXT("Late callbacks on the old Pawn cannot recreate desire"), WuwaTestGait::Of(FirstMovement)->ReadSprintDesire() == EWuwaSprintDesire::None);

	WuwaTestInput::SetMoveAxis(SecondCharacter, MovingInput.Get<FVector2D>());
	Fixture.Dispatch(Dash, EWuwaInputPhase::Pressed, 13.0);
	WuwaTestGait::Of(SecondMovement)->OpenSprintWindow(CompletedWindow);
	Fixture.World->TimeSeconds = 13.3;
	SampleWindow(SecondCharacter, CompletedWindow);
	WuwaTestGait::Of(SecondMovement)->CloseSprintWindow(CompletedWindow);
	WuwaTestGait::Of(SecondMovement)->OpenSprintWindow(OpenWindow);
	TestTrue(TEXT("The second character can commit its own sustained desire"), WuwaTestGait::Of(SecondMovement)->ReadSprintDesire() == EWuwaSprintDesire::Sustained);
	Fixture.Controller->SetPawn(nullptr);
	TestFalse(TEXT("Unpossessing clears movement intent on the former Pawn"), SecondCharacter->GetPlayerInputState().bHasMoveInput);
	TestTrue(TEXT("Unpossessing clears its retained and open sprint state"), WuwaTestGait::Of(SecondMovement)->ReadSprintDesire() == EWuwaSprintDesire::None);
	WuwaTestGait::Of(SecondMovement)->CloseSprintWindow(OpenWindow);
	TestTrue(TEXT("Late End after unpossession cannot commit a request"), WuwaTestGait::Of(SecondMovement)->ReadSprintDesire() == EWuwaSprintDesire::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSprintInputAssetsTest, "Wuwa.Input.Sprint.AssetConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSprintInputAssetsTest::RunTest(const FString& Parameters)
{
	const UInputAction* DodgeAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/CoreInput/Actions/IA_Dodge.IA_Dodge"));
	const UWuwaInputDataAsset* InputTagMap = LoadObject<UWuwaInputDataAsset>(nullptr, TEXT("/Game/CoreInput/DataAsset/DA_IARouterMap.DA_IARouterMap"));
	const UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/CoreInput/Contexts/IMC_Character.IMC_Character"));
	if (!TestNotNull(TEXT("The existing Dodge action loads"), DodgeAction)
		|| !TestNotNull(TEXT("The current input tag asset loads"), InputTagMap)
		|| !TestNotNull(TEXT("The character mapping context loads"), MappingContext))
	{
		return false;
	}
	const FWuwaGameTags Tags = FWuwaGameTags::Get();
	int32 DodgeBindings = 0;
	for (const FInputDataAsset& Binding : InputTagMap->InputDataAssetMap)
	{
		if (Binding.InputAction == DodgeAction)
		{
			++DodgeBindings;
			TestTrue(TEXT("Dodge maps to the existing Dash semantic input"), Binding.InputTag == Tags.Input_Combat_Dash);
			TestTrue(TEXT("Dodge remains on the ability input route"), Binding.RouteTag == Tags.Input_Route_Ability);
		}
	}
	TestEqual(TEXT("Dodge has one configured semantic binding"), DodgeBindings, 1);
	bool bDodgeMapped = false;
	for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
	{
		bDodgeMapped |= Mapping.Action == DodgeAction;
	}
	TestTrue(TEXT("A device mapping supplies Dodge without the gameplay code depending on a particular key"), bDodgeMapped);
	return true;
}

#endif
