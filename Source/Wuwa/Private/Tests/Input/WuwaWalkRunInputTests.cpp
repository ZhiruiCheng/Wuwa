#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/Movement/WuwaTestGait.h"
#include "Misc/ScopeExit.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaMoveInputHandler.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Game/Input/DataAsset/WuwaInputDataAsset.h"
#include "Tests/Input/WuwaTestMoveInput.h"
#include "Game/Input/WuwaInputRouterComponent.h"
#include "Game/Input/WuwaInputTypes.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "UObject/UnrealType.h"
#include "Core/Tag/WuwaGameTags.h"
#include "Game/Framework/WuwaGameMode.h"

namespace
{
	// CharacterMovement 的地面判断还要求 UpdatedComponent，不能仅 new 一个游离组件。
	struct FScopedWalkRunTestWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);

		~FScopedWalkRunTestWorld()
		{
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
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnParameters.ObjectFlags |= RF_Transient;
			return World->SpawnActor<AWuwaCharacter>(SpawnParameters);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaWalkRunConfigLookupTest, "Wuwa.Input.WalkRun.ConfigLookup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaWalkRunConfigLookupTest::RunTest(const FString& Parameters)
{
	const FWuwaGameTags Tags = FWuwaGameTags::Get();
	if (!TestTrue(TEXT("Walk/run input tag is registered"), Tags.Player_Common_Movement_WalkRun.IsValid()))
	{
		return false;
	}
	const UWuwaMoveInputConfig* Config = WuwaTestInput::MakeMoveInputConfig(GetTransientPackage());
	const FWuwaMoveInputBinding* Press = Config->FindBinding(Tags.Player_Common_Movement_WalkRun, EWuwaInputPhase::Pressed);
	TestTrue(TEXT("A walk/run press maps to the toggle action"),
		Press && Press->Action && Press->Action->IsA<UWuwaMoveInputAction_ToggleWalkPreference>());

	for (const EWuwaInputPhase Phase : { EWuwaInputPhase::Triggered, EWuwaInputPhase::Released, EWuwaInputPhase::Canceled })
	{
		TestNull(FString::Printf(TEXT("Phase %d has no walk/run action"), static_cast<int32>(Phase)),
			Config->FindBinding(Tags.Player_Common_Movement_WalkRun, Phase));
	}
	TestNull(TEXT("Move axis input is not a configured movement action"),
		Config->FindBinding(Tags.Player_Common_Movement_Move, EWuwaInputPhase::Pressed));
	TestNull(TEXT("An empty input tag has no action"), Config->FindBinding(FGameplayTag(), EWuwaInputPhase::Pressed));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaWalkRunExecuteTest, "Wuwa.Input.WalkRun.ToggleAction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaWalkRunExecuteTest::RunTest(const FString& Parameters)
{
	FScopedWalkRunTestWorld TestWorld;
	AWuwaCharacter* Character = TestWorld.SpawnCharacter();
	UWuwaMovementComponent* Movement = Character ? Character->GetWuwaMovementComponent() : nullptr;
	if (!TestNotNull(TEXT("Execution test has a character-owned movement component"), Movement))
	{
		return false;
	}
	Movement->MovementMode = MOVE_Walking;
	Movement->MovementSettings = NewObject<UWuwaMovementSettings>(Character);
	Movement->MovementSettings->Run.MaxSpeed = 650.f;
	// 未初始化 Actor 的测试世界不会调用 PostInitializeComponents：设好移动模式后显式装配运动状态。
	if (!TestTrue(TEXT("Execution test assembles its movement state"), Character->EnsureMovementStateSystem())) return false;
	Movement->RefreshMovementSettings();
	const FVector OriginalVelocity(123.f, 45.f, 67.f);
	Movement->Velocity = OriginalVelocity;

	const UWuwaMoveInputAction_ToggleWalkPreference* Toggle = NewObject<UWuwaMoveInputAction_ToggleWalkPreference>(Character);
	TestTrue(TEXT("Ground movement permits walk/run switching"), Movement->CanToggleWalkPreference());
	TestTrue(TEXT("Ground toggle action is accepted"), WuwaTestInput::ExecuteMoveAction(*Toggle, Character));
	// 走跑键只改偏好；真实游戏里本帧 CMC Tick 开头 RoleGait 才按新偏好决定状态，测试不跑 Tick，手动刷新一次。
	Character->RoleGaitComponent->RefreshPolicy();
	TestTrue(TEXT("Run switches to Walk"), Movement->GetDesiredGait() == EWuwaGait::Walk);
	TestTrue(TEXT("Walk has a lower speed cap"), Movement->GetMaxSpeed() < Movement->MovementSettings->Run.MaxSpeed);
	TestEqual(TEXT("Switching preserves Run configuration"), Movement->MovementSettings->Run.MaxSpeed, 650.f);
	TestEqual(TEXT("State event applies Walk speed to CMC"), Movement->MaxWalkSpeed, Movement->MovementSettings->Walk.MaxSpeed);
	TestEqual(TEXT("Switching does not overwrite actual velocity"), Movement->Velocity, OriginalVelocity);
	TestTrue(TEXT("A second toggle action is accepted"), WuwaTestInput::ExecuteMoveAction(*Toggle, Character));
	// 同上：本帧 Tick 开头 RoleGait 才读到新偏好。
	Character->RoleGaitComponent->RefreshPolicy();
	TestTrue(TEXT("Walk switches back to Run"), Movement->GetDesiredGait() == EWuwaGait::Run);
	TestEqual(TEXT("Run retains its configured speed"), Movement->GetMaxSpeed(), 650.f);

	TestFalse(TEXT("An action without a character is rejected"), WuwaTestInput::ExecuteMoveAction(*Toggle, nullptr));
	TestTrue(TEXT("A rejected action does not change gait"), Movement->GetDesiredGait() == EWuwaGait::Run);
	Character->bIsCrouched = true;
	TestFalse(TEXT("Crouching rejects switching"), Movement->CanToggleWalkPreference());
	TestFalse(TEXT("Crouching rejects the action"), WuwaTestInput::ExecuteMoveAction(*Toggle, Character));
	TestTrue(TEXT("A rejected crouching command preserves gait"), Movement->GetDesiredGait() == EWuwaGait::Run);
	Character->bIsCrouched = false;

	for (const EMovementMode Mode : { MOVE_Falling, MOVE_Custom, MOVE_Swimming, MOVE_Flying, MOVE_None })
	{
		Movement->SetMovementMode(Mode, static_cast<uint8>(ECustomMoveMode::MOVE_Climb));
		TestFalse(FString::Printf(TEXT("Movement mode %d rejects switching"), static_cast<int32>(Mode)), Movement->CanToggleWalkPreference());
		TestFalse(FString::Printf(TEXT("Movement mode %d rejects the action"), static_cast<int32>(Mode)), WuwaTestInput::ExecuteMoveAction(*Toggle, Character));
		TestTrue(TEXT("Rejected actions preserve gait"), Movement->GetDesiredGait() == EWuwaGait::Run);
	}


	Movement->SetMovementMode(MOVE_Walking);
	Movement->Velocity = FVector::ZeroVector;
	TestTrue(TEXT("Idle also permits switching the desired gait"), WuwaTestInput::ExecuteMoveAction(*Toggle, Character));
	TestTrue(TEXT("Idle switching records Walk"), Movement->GetDesiredGait() == EWuwaGait::Walk);
	TestEqual(TEXT("Idle switching does not create movement"), Movement->Velocity, FVector::ZeroVector);
	Movement->SetMovementMode(MOVE_Custom, static_cast<uint8>(ECustomMoveMode::MOVE_Climb));
	TestEqual(TEXT("Walk preference does not change existing climbing speed"), Movement->GetMaxSpeed(), 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaWalkRunRouterIntegrationTest, "Wuwa.Input.WalkRun.RouterIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaWalkRunRouterIntegrationTest::RunTest(const FString& Parameters)
{
	// 独立临时世界，不加载/修改关卡，也不调用 BeginPlay 或 Possess。
	// SetPawn 只建立本测试要验证的“当前角色”关系，避免触发无关的 GAS/HUD 初始化。
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("A temporary integration-test world was created"), TestWorld))
	{
		return false;
	}
	AWuwaPlayerController* Controller = nullptr;
	ON_SCOPE_EXIT
	{
		if (IsValid(Controller))
		{
			Controller->SetPawn(nullptr);
		}
		TestWorld->DestroyWorld(false);
	};

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.ObjectFlags |= RF_Transient;
	Controller = TestWorld->SpawnActor<AWuwaPlayerController>(SpawnParameters);
	AWuwaCharacter* FirstCharacter = TestWorld->SpawnActor<AWuwaCharacter>(SpawnParameters);
	AWuwaCharacter* SecondCharacter = TestWorld->SpawnActor<AWuwaCharacter>(SpawnParameters);
	const bool bHasController = TestNotNull(TEXT("The controller was spawned"), Controller);
	const bool bHasFirstCharacter = TestNotNull(TEXT("The first character was spawned"), FirstCharacter);
	const bool bHasSecondCharacter = TestNotNull(TEXT("The second character was spawned"), SecondCharacter);
	if (!bHasController || !bHasFirstCharacter || !bHasSecondCharacter)
	{
		return false;
	}

	UWuwaMovementComponent* FirstMovement = FirstCharacter->GetWuwaMovementComponent();
	UWuwaMovementComponent* SecondMovement = SecondCharacter->GetWuwaMovementComponent();
	UWuwaInputRouterComponent* Router = Controller->GetInputRouter();
	const bool bHasFirstMovement = TestNotNull(TEXT("The first character owns Wuwa movement"), FirstMovement);
	const bool bHasSecondMovement = TestNotNull(TEXT("The second character owns Wuwa movement"), SecondMovement);
	const bool bHasRouter = TestNotNull(TEXT("The controller owns the input router"), Router);
	if (!bHasFirstMovement || !bHasSecondMovement || !bHasRouter)
	{
		return false;
	}
	FirstMovement->MovementMode = MOVE_Walking;
	SecondMovement->MovementMode = MOVE_Walking;
	// 未初始化 Actor 的测试世界不会调用 PostInitializeComponents：设好移动模式后显式装配运动状态。
	if (!TestTrue(TEXT("The first character assembles its movement state"), FirstCharacter->EnsureMovementStateSystem())) return false;
	if (!TestTrue(TEXT("The second character assembles its movement state"), SecondCharacter->EnsureMovementStateSystem())) return false;
	Controller->RegisterInputRouteHandlers();
	WuwaTestInput::UseTestMoveInputConfig(Controller);
	Controller->SetPawn(FirstCharacter);

	const FWuwaGameTags Tags = FWuwaGameTags::Get();
	FInputDataAsset Binding{};
	Binding.InputAction = NewObject<UInputAction>(Controller);
	Binding.InputTag = Tags.Player_Common_Movement_WalkRun;
	Binding.RouteTag = Tags.Input_Route_Movement;
	if (!TestTrue(TEXT("The integration binding is configured"), Binding.IsConfigured()))
	{
		return false;
	}

	FWuwaInputEvent Event;
	// 与 PlayerController::HandleRoutedInput 一样由调用方构造完整事件；Router 不再代为查找 Binding。
	Event.InputTag = Binding.InputTag;
	Event.RouteTag = Binding.RouteTag;
	Event.SourceAction = Binding.InputAction;
	Event.Phase = EWuwaInputPhase::Pressed;
	Event.Value = FInputActionValue(true);
	TestTrue(TEXT("Router dispatch reaches the handler and movement component"), Router->DispatchInput(Event));
	TestTrue(TEXT("The current character switches to Walk"), FirstMovement->GetDesiredGait() == EWuwaGait::Walk);
	TestTrue(TEXT("The other character remains Run"), SecondMovement->GetDesiredGait() == EWuwaGait::Run);

	Event.Phase = EWuwaInputPhase::Released;
	Event.Value.Reset();
	TestFalse(TEXT("Release has no configured movement action"), Router->DispatchInput(Event));
	TestTrue(TEXT("Release does not toggle the current character again"), FirstMovement->GetDesiredGait() == EWuwaGait::Walk);

	Controller->SetPawn(SecondCharacter);
	Event.Phase = EWuwaInputPhase::Pressed;
	Event.Value = FInputActionValue(true);
	TestTrue(TEXT("After switching Pawn, dispatch uses the new character"), Router->DispatchInput(Event));
	TestTrue(TEXT("The new character switches to Walk"), SecondMovement->GetDesiredGait() == EWuwaGait::Walk);
	TestTrue(TEXT("The previous character is not toggled back to Run"), FirstMovement->GetDesiredGait() == EWuwaGait::Walk);

	Controller->SetPawn(nullptr);
	TestFalse(TEXT("No controlled Pawn safely rejects the input"), Router->DispatchInput(Event));
	TestTrue(TEXT("No-Pawn input leaves the first character unchanged"), FirstMovement->GetDesiredGait() == EWuwaGait::Walk);
	TestTrue(TEXT("No-Pawn input leaves the second character unchanged"), SecondMovement->GetDesiredGait() == EWuwaGait::Walk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaWalkRunAssetsTest, "Wuwa.Input.WalkRun.AssetConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaWalkRunAssetsTest::RunTest(const FString& Parameters)
{
	// 只读加载项目真实配置，覆盖“代码正确但 IA/IMC/路由未接线”的问题。
	const UInputAction* WalkRunAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/CoreInput/Actions/IA_WalkRun.IA_WalkRun"));
	const UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/CoreInput/Contexts/IMC_Character.IMC_Character"));
	const UWuwaInputDataAsset* InputTagMap = LoadObject<UWuwaInputDataAsset>(nullptr, TEXT("/Game/CoreInput/DataAsset/DA_IARouterMap.DA_IARouterMap"));
	const bool bHasAction = TestNotNull(TEXT("Walk/run input action exists"), WalkRunAction);
	const bool bHasContext = TestNotNull(TEXT("Character mapping context exists"), MappingContext);
	const bool bHasTagMap = TestNotNull(TEXT("Input tag map exists"), InputTagMap);
	if (!bHasAction || !bHasContext || !bHasTagMap)
	{
		return false;
	}

	TestTrue(TEXT("Walk/run is a Boolean action"), WalkRunAction->ValueType == EInputActionValueType::Boolean);
	// 走跑切换只绑一个键；LeftAlt 留给"按住显示鼠标"（IA_ShowMouse），不能再绑走跑。
	int32 WalkRunMappingCount = 0;
	bool bWalkRunOnAlt = false;
	for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
	{
		if (Mapping.Action == WalkRunAction)
		{
			++WalkRunMappingCount;
			bWalkRunOnAlt |= Mapping.Key == EKeys::LeftAlt;
			AddInfo(FString::Printf(TEXT("WalkRun key: %s"), *Mapping.Key.ToString()));
		}
	}
	TestEqual(TEXT("WalkRun is bound to exactly one key"), WalkRunMappingCount, 1);
	TestFalse(TEXT("WalkRun is not on Left Alt (reserved for showing the mouse)"), bWalkRunOnAlt);

	const FWuwaGameTags Tags = FWuwaGameTags::Get();
	int32 WalkRunRowCount = 0;
	int32 WalkRunTagCount = 0;
	for (const FInputDataAsset& Binding : InputTagMap->InputDataAssetMap)
	{
		if (Binding.InputTag == Tags.Player_Common_Movement_WalkRun)
		{
			++WalkRunTagCount;
		}
		if (Binding.InputAction == WalkRunAction)
		{
			++WalkRunRowCount;
			TestTrue(TEXT("WalkRun binding is configured"), Binding.IsConfigured());
			TestTrue(TEXT("WalkRun binding has the semantic input tag"), Binding.InputTag == Tags.Player_Common_Movement_WalkRun);
			TestTrue(TEXT("WalkRun is routed to Movement"), Binding.RouteTag == Tags.Input_Route_Movement);
		}
	}
	TestEqual(TEXT("The WalkRun action has exactly one binding"), WalkRunRowCount, 1);
	TestEqual(TEXT("The WalkRun semantic tag occurs exactly once"), WalkRunTagCount, 1);

	// 沿真实蓝图 CDO 确认这套配置被角色/控制器引用，而不仅是资产单独存在。
	UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, TEXT("/Game/Core/BP_WuwaGameMode.BP_WuwaGameMode_C"));
	UClass* ControllerClass = LoadClass<AWuwaPlayerController>(nullptr, TEXT("/Game/Core/BP_WuwaPlayerController.BP_WuwaPlayerController_C"));
	UClass* CharacterClass = LoadClass<AWuwaCharacter>(nullptr, TEXT("/Game/Characters/Role/changli/BP_Role_Changli.BP_Role_Changli_C"));
	const bool bHasGameModeClass = TestNotNull(TEXT("The project GameMode blueprint loads"), GameModeClass);
	const bool bHasControllerClass = TestNotNull(TEXT("The project controller blueprint loads"), ControllerClass);
	const bool bHasCharacterClass = TestNotNull(TEXT("The project character blueprint loads"), CharacterClass);
	if (!bHasGameModeClass || !bHasControllerClass || !bHasCharacterClass)
	{
		return false;
	}
	const AGameModeBase* GameModeDefaults = GameModeClass->GetDefaultObject<AGameModeBase>();
	const AWuwaPlayerController* ControllerDefaults = ControllerClass->GetDefaultObject<AWuwaPlayerController>();
	TestTrue(TEXT("GameMode uses the expected player controller"), GameModeDefaults->PlayerControllerClass.Get() == ControllerClass);
	// 角色由队伍组件生成（AWuwaGameMode::RestartPlayer），GameMode 不再生成默认 Pawn。
	TestTrue(TEXT("GameMode is the formation-aware Wuwa GameMode"), GameModeClass->IsChildOf(AWuwaGameMode::StaticClass()));
	TestNull(TEXT("GameMode does not spawn a default pawn besides the formation"), GameModeDefaults->DefaultPawnClass.Get());
	TestTrue(TEXT("The player controller uses the configured input tag map"), ControllerDefaults->InputTagMap == InputTagMap);
	// 走跑键 -> 指令的配置表：控制器默认指向它，且其中配置了切换走跑偏好。
	const UWuwaMoveInputConfig* MoveConfig = ControllerDefaults->MoveInputHandler
		? ControllerDefaults->MoveInputHandler->Config.LoadSynchronous() : nullptr;
	if (TestNotNull(TEXT("The player controller references the move input config"), MoveConfig))
	{
		const FWuwaMoveInputBinding* Toggle = MoveConfig->FindBinding(Tags.Player_Common_Movement_WalkRun, EWuwaInputPhase::Pressed);
		TestTrue(TEXT("The move input config maps a WalkRun press to the toggle action"),
			Toggle && Toggle->Action && Toggle->Action->IsA<UWuwaMoveInputAction_ToggleWalkPreference>());
	}
	// 映射上下文属于本地玩家，由控制器注册；角色不再持有输入组件。
	TestTrue(TEXT("The player controller registers IMC_Character"), ControllerDefaults->DefaultMappingContext == MappingContext);
	return true;
}

#endif
