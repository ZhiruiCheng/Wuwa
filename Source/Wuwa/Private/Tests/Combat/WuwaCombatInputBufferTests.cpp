#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "UObject/Script.h"
#include "Core/Tag/WuwaGameTags.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Input/WuwaInputRouterComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "Game/NewWorld/Character/Common/Component/Input/UWuwaCombatInputRuntimeBridge.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaAbilityInputHandlerComponent.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputCommandConfig.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputIntentComponent.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"
#include "WuwaSkillTestAbility.h"

namespace WuwaCombatInputBufferTests
{
	// Exercise the real generated C# input runtime through its production native bridge.
	// A native test ability supplies configurable defaults; all GAS lifecycle and policy
	// behavior still comes from the production ability, skill manager and FightState.
	struct FFixture
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AWuwaCharacter* Character = nullptr;
		AWuwaPlayerController* Controller = nullptr;
		UWuwaAbilitySystemComponent* ASC = nullptr;
		UWuwaSkillBridgeComponent* Skills = nullptr;
		UWuwaCombatInputRuntimeBridge* Runtime = nullptr;
		/** 按键规则表：Grant(InputTag) 为每个技能追加一行"这个输入 -> 这个技能 ID"。 */
		UWuwaInputCommandConfig* Config = nullptr;
		mutable int32 NextSkillId = 1;
		UWuwaSkillTestAbility* Defaults = GetMutableDefault<UWuwaSkillTestAbility>();
		const bool OriginalMainSkill = Defaults->bIsMainSkill;
		const int32 OriginalInterruptLevel = Defaults->InterruptLevel;
		const EWuwaSkillOverrideType OriginalOverrideType = Defaults->SkillOverrideType;
		const EWuwaMoveState OriginalStartMoveState = Defaults->StartMoveState;
		int32 Activations = 0;
		const FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(FName(TEXT("GAS.GA.Role.Attack1")));
		const FGameplayTag OtherTag = FGameplayTag::RequestGameplayTag(FName(TEXT("GAS.GA.SpeedUpIteam")));

		~FFixture()
		{
			if (IsValid(Controller)) Controller->SetPawn(nullptr);
			if (Runtime) Runtime->ResetInput();
			if (ASC)
			{
				ASC->CancelAllAbilities();
				ASC->ClearActorInfo();
			}
			if (World) World->DestroyWorld(false);
			Defaults->bIsMainSkill = OriginalMainSkill;
			Defaults->InterruptLevel = OriginalInterruptLevel;
			Defaults->SkillOverrideType = OriginalOverrideType;
			Defaults->StartMoveState = OriginalStartMoveState;
		}

		AWuwaCharacter* SpawnCharacter() const
		{
			FActorSpawnParameters Spawn;
			Spawn.ObjectFlags |= RF_Transient;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<AWuwaCharacter>(Spawn);
		}

		bool Initialize(FAutomationTestBase& Test)
		{
			if (!Test.TestNotNull(TEXT("Transient game world exists"), World)) return false;
			World->TimeSeconds = 1.0;
			Character = SpawnCharacter();
			if (!Test.TestNotNull(TEXT("Character exists"), Character)) return false;
			Character->GetWuwaMovementComponent()->MovementMode = MOVE_Walking;
			if (!Test.TestTrue(TEXT("Movement policy assembles"), Character->EnsureMovementStateSystem())
				|| !Test.TestTrue(TEXT("Skill policy assembles"), Character->EnsureSkillSystem())) return false;
			Character->RoleGaitComponent->RefreshPolicy();
			Skills = Character->SkillComponent;
			ASC = NewObject<UWuwaAbilitySystemComponent>(Character);
			ASC->RegisterComponent();
			ASC->InitAbilityActorInfo(Character, Character);
			ASC->AbilityActivatedCallbacks.AddLambda([this](UGameplayAbility*) { ++Activations; });
			UClass* RuntimeClass = LoadClass<UWuwaCombatInputRuntimeBridge>(nullptr,
				TEXT("/Script/UnrealSharp.WuwaCombatInputRuntime_C"));
			if (!Test.TestNotNull(TEXT("Managed input runtime class loads"), RuntimeClass)) return false;
			Runtime = NewObject<UWuwaCombatInputRuntimeBridge>(Character, RuntimeClass);
			if (!Test.TestNotNull(TEXT("Managed input runtime exists"), Runtime)
				|| !Test.TestEqual(TEXT("Runtime uses the real generated managed class"),
					Runtime->GetClass()->GetClass()->GetFName(), FName(TEXT("CSClass")))) return false;
			Config = NewObject<UWuwaInputCommandConfig>(Character);
			Character->InputCommandConfig = Config;
			Configure(100);
			return true;
		}

		void Configure(int32 InterruptLevel) const
		{
			Defaults->bIsMainSkill = true;
			Defaults->InterruptLevel = InterruptLevel;
			Defaults->SkillOverrideType = EWuwaSkillOverrideType::None;
			Defaults->StartMoveState = EWuwaMoveState::Other;
		}

		bool AttachLocalController(FAutomationTestBase& Test)
		{
			FActorSpawnParameters Spawn;
			Spawn.ObjectFlags |= RF_Transient;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Controller = World->SpawnActor<AWuwaPlayerController>(Spawn);
			if (!Test.TestNotNull(TEXT("The real player controller exists"), Controller)) return false;
			// Match local-player setup without unrelated GameMode, HUD or viewport initialization.
			Controller->SetAsLocalPlayerController();
			ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
			LocalPlayer->PlayerController = Controller;
			Controller->Player = LocalPlayer;
			Controller->SetPawn(Character);
			Character->SetController(Controller);
			ASC->InitAbilityActorInfo(Character, Character);
			Controller->AscComponent = ASC;
			Controller->RegisterInputRouteHandlers();
			return Test.TestTrue(TEXT("The fixture controller is local"), Controller->IsLocalController());
		}

		/** 有输入 Tag：按技能 ID 授予，并在规则表末尾加一行"输入 -> 该 ID"；没有：普通能力（只能直接启动）。 */
		FGameplayAbilitySpecHandle Grant(FGameplayTag InputTag = {}, bool bProductionBase = false) const
		{
			UClass* Class = bProductionBase ? UWuwaGameplayAbilityBase::StaticClass() : UWuwaSkillTestAbility::StaticClass();
			if (!InputTag.IsValid()) return ASC->GiveAbility(FGameplayAbilitySpec(Class, 1));
			FWuwaSkillInfo Info;
			Info.Ability = Class;
			const int32 SkillId = NextSkillId++;
			FWuwaInputCommandRule Rule;
			Rule.RuleName = *FString::Printf(TEXT("Skill%d"), SkillId);
			Rule.InputTag = InputTag;
			Rule.TargetSkillId = SkillId;
			Config->Rules.Add(Rule);
			return ASC->GiveSkill(SkillId, Info, nullptr);
		}

		UWuwaGameplayAbilityBase* Active(FGameplayAbilitySpecHandle Handle) const
		{
			const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
			if (Spec) for (UGameplayAbility* Instance : Spec->GetAbilityInstances())
			{
				if (UWuwaGameplayAbilityBase* Ability = Cast<UWuwaGameplayAbilityBase>(Instance);
					IsValid(Ability) && Ability->IsActive()) return Ability;
			}
			return nullptr;
		}

		UWuwaGameplayAbilityBase* Start(FAutomationTestBase& Test, FGameplayAbilitySpecHandle Handle) const
		{
			if (!Test.TestTrue(TEXT("Initial skill starts through GAS"), ASC->TryActivateAbility(Handle, false))) return nullptr;
			UWuwaGameplayAbilityBase* Ability = Active(Handle);
			return Test.TestNotNull(TEXT("Initial skill retains an active instance"), Ability) ? Ability : nullptr;
		}

		FWuwaInputEvent Input(FGameplayTag Tag, EWuwaInputPhase Phase = EWuwaInputPhase::Pressed) const
		{
			FWuwaInputEvent Event;
			Event.InputTag = Tag;
			Event.Phase = Phase;
			Event.Timestamp = World->GetTimeSeconds();
			return Event;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCombatInputSingleSubmissionTest,
	"Wuwa.Combat.InputBuffer.DoublePressAndSameSpecReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCombatInputSingleSubmissionTest::RunTest(const FString& Parameters)
{
	using namespace WuwaCombatInputBufferTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	const FGameplayAbilitySpecHandle Spec = F.Grant(F.AttackTag);
	UWuwaGameplayAbilityBase* First = F.Start(*this, Spec);
	if (!First) return false;
	const int32 FirstHandle = First->GetSkillHandle();
	TestTrue(TEXT("First press before the input window is buffered"),
		F.Runtime->ProcessInput(F.ASC, F.Input(F.AttackTag), 0.5f) == EWuwaCombatInputResult::Buffered);
	TestTrue(TEXT("Second press is retained as another raw input"),
		F.Runtime->ProcessInput(F.ASC, F.Input(F.AttackTag), 0.5f) == EWuwaCombatInputResult::Buffered);
	TestEqual(TEXT("Both raw inputs are retained until selection"), F.Runtime->GetBufferedInputCount(), 2);
	F.Runtime->ProcessPendingInput(F.ASC);
	TestEqual(TEXT("A normal pending-input tick cannot replay before a breakpoint"), F.Activations, 1);
	TestTrue(TEXT("Active skill opens its input window"), F.Skills->SetSkillAcceptInput(First->GetSkillHandle(), true));
	TestTrue(TEXT("Breakpoint submits the same spec for a fresh per-execution activation"),
		F.Runtime->ProcessPendingInput(F.ASC) == EWuwaCombatInputResult::ActivationRequested);
	TestEqual(TEXT("Two buffered presses produce exactly one additional activation"), F.Activations, 2);
	TestEqual(TEXT("Selecting one command consumes the entire old batch"), F.Runtime->GetBufferedInputCount(), 0);
	UWuwaGameplayAbilityBase* Second = F.Active(Spec);
	if (!TestNotNull(TEXT("The replay has an active execution"), Second)) return false;
	TestTrue(TEXT("The replay receives a newer ownership handle"), Second->GetSkillHandle() > FirstHandle);
	TestFalse(TEXT("A replay does not inherit the old accept-input window"), F.Skills->GetCurrentSkillData().bSkillAcceptInput);
	F.Skills->SetSkillAcceptInput(Second->GetSkillHandle(), true);
	F.Runtime->ProcessPendingInput(F.ASC);
	TestEqual(TEXT("A later breakpoint cannot replay the already consumed second press"), F.Activations, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCombatInputExpiryAndOpportunityTest,
	"Wuwa.Combat.InputBuffer.ExpiryAndExplicitOpportunity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCombatInputExpiryAndOpportunityTest::RunTest(const FString& Parameters)
{
	using namespace WuwaCombatInputBufferTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	F.Configure(200);
	const FGameplayAbilitySpecHandle OldSpec = F.Grant();
	UWuwaGameplayAbilityBase* Old = F.Start(*this, OldSpec);
	if (!Old) return false;
	const FGameplayAbilitySpecHandle Candidate = F.Grant(F.AttackTag, true);
	TestTrue(TEXT("Lower-priority candidate buffers"),
		F.Runtime->ProcessInput(F.ASC, F.Input(F.AttackTag), 0.2f) == EWuwaCombatInputResult::Buffered);
	F.World->TimeSeconds = 2.0;
	F.Runtime->ProcessPendingInput(F.ASC);
	TestEqual(TEXT("Expiry is pruned even without a new breakpoint"), F.Runtime->GetBufferedInputCount(), 0);
	TestEqual(TEXT("Expired input never activates"), F.Activations, 1);
	TestTrue(TEXT("A fresh press is buffered"),
		F.Runtime->ProcessInput(F.ASC, F.Input(F.AttackTag), 1.0f) == EWuwaCombatInputResult::Buffered);
	const int64 Before = F.Skills->GetCurrentSkillData().InputOpportunitySerial;
	TestTrue(TEXT("Owner can change its active interrupt level"),
		F.Skills->SetSkillInterruptLevel(Old->GetSkillHandle(), 50));
	TestEqual(TEXT("Changing priority alone is not an animation input breakpoint"),
		F.Skills->GetCurrentSkillData().InputOpportunitySerial, Before);
	TestTrue(TEXT("The cached candidate now passes the pure request check"), F.ASC->IsSpecAvailableForActivation(Candidate));
	F.Runtime->ProcessPendingInput(F.ASC);
	TestEqual(TEXT("Eligibility alone does not trigger automatic frame-by-frame replay"), F.Activations, 1);
	TestEqual(TEXT("The raw input stays pending until an explicit opportunity"), F.Runtime->GetBufferedInputCount(), 1);
	F.Skills->SetSkillAcceptInput(Old->GetSkillHandle(), true);
	TestTrue(TEXT("An explicit opportunity re-evaluates and submits the candidate"),
		F.Runtime->ProcessPendingInput(F.ASC) == EWuwaCombatInputResult::ActivationRequested);
	TestEqual(TEXT("The explicit opportunity causes only one activation"), F.Activations, 2);
	TestNotNull(TEXT("The selected candidate is active"), F.Active(Candidate));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCombatInputPrioritySelectionTest,
	"Wuwa.Combat.InputBuffer.HighestPriorityAndStableTie",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCombatInputPrioritySelectionTest::RunTest(const FString& Parameters)
{
	using namespace WuwaCombatInputBufferTests;
	FEditorScriptExecutionGuard Guard;
	for (const bool bEqualPriority : {false, true})
	{
		FFixture F;
		if (!F.Initialize(*this)) return false;
		F.Configure(220);
		UWuwaGameplayAbilityBase* Old = F.Start(*this, F.Grant());
		if (!Old) return false;
		const FGameplayAbilitySpecHandle First = F.Grant(F.AttackTag, true); // Production default priority 100.
		F.Configure(bEqualPriority ? 100 : 150);
		const FGameplayAbilitySpecHandle Later = F.Grant(F.OtherTag);
		TestTrue(TEXT("Earlier candidate is buffered"),
			F.Runtime->ProcessInput(F.ASC, F.Input(F.AttackTag), 1.0f) == EWuwaCombatInputResult::Buffered);
		TestTrue(TEXT("Later candidate is buffered"),
			F.Runtime->ProcessInput(F.ASC, F.Input(F.OtherTag), 1.0f) == EWuwaCombatInputResult::Buffered);
		F.Skills->SetMainSkillReadyEnd(Old->GetSkillHandle(), true);
		TestTrue(TEXT("Ready-end reselects one candidate from the batch"),
			F.Runtime->ProcessPendingInput(F.ASC) == EWuwaCombatInputResult::ActivationRequested);
		TestEqual(TEXT("Selection submits exactly once"), F.Activations, 2);
		TestEqual(TEXT("Selection clears all contenders"), F.Runtime->GetBufferedInputCount(), 0);
		TestNotNull(bEqualPriority ? TEXT("Equal priorities retain the earlier input") : TEXT("A later higher-priority candidate wins"),
			F.Active(bEqualPriority ? First : Later));
		TestNull(TEXT("The losing candidate is never activated"), F.Active(bEqualPriority ? Later : First));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCombatInputInvalidAndResetTest,
	"Wuwa.Combat.InputBuffer.InvalidCandidatesAndReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCombatInputInvalidAndResetTest::RunTest(const FString& Parameters)
{
	using namespace WuwaCombatInputBufferTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	TestTrue(TEXT("Unknown input has no candidate"),
		F.Runtime->ProcessInput(F.ASC, F.Input(F.AttackTag), 1.0f) == EWuwaCombatInputResult::NoCandidate);
	TestEqual(TEXT("Unknown input is not cached"), F.Runtime->GetBufferedInputCount(), 0);
	// 同一技能 ID 不能重复授予（身份唯一），旧的"同 Tag 多个 Spec = Ambiguous"已不存在，见 InputCommand 测试
	const FGameplayAbilitySpecHandle First = F.Grant(F.AttackTag);
	TestTrue(TEXT("Released input remains outside this pressed-only stage"),
		F.Runtime->ProcessInput(F.ASC, F.Input(F.AttackTag, EWuwaInputPhase::Released), 1.0f) == EWuwaCombatInputResult::Ignored);
	UWuwaGameplayAbilityBase* Old = F.Start(*this, First);
	if (!Old) return false;
	F.Grant(F.OtherTag);
	TestTrue(TEXT("Zero lifetime disables caching for a blocked command"),
		F.Runtime->ProcessInput(F.ASC, F.Input(F.OtherTag), 0.f) != EWuwaCombatInputResult::Buffered);
	TestEqual(TEXT("Zero lifetime does not create a pending input"), F.Runtime->GetBufferedInputCount(), 0);
	TestTrue(TEXT("A uniquely mapped command can be cached"),
		F.Runtime->ProcessInput(F.ASC, F.Input(F.OtherTag), 1.f) == EWuwaCombatInputResult::Buffered);
	F.Runtime->ResetInput();
	TestEqual(TEXT("Reset clears pending raw inputs"), F.Runtime->GetBufferedInputCount(), 0);
	F.Skills->SetMainSkillReadyEnd(Old->GetSkillHandle(), true);
	F.Runtime->ProcessPendingInput(F.ASC);
	TestEqual(TEXT("Reset input cannot fire at a later ready-end point"), F.Activations, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCombatInputAvatarIsolationTest,
	"Wuwa.Combat.InputBuffer.AvatarSwitchDiscardsPending",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCombatInputAvatarIsolationTest::RunTest(const FString& Parameters)
{
	using namespace WuwaCombatInputBufferTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	const FGameplayAbilitySpecHandle Spec = F.Grant(F.AttackTag);
	if (!F.Start(*this, Spec)) return false;
	TestTrue(TEXT("The original avatar owns a buffered input"),
		F.Runtime->ProcessInput(F.ASC, F.Input(F.AttackTag), 1.0f) == EWuwaCombatInputResult::Buffered);
	AWuwaCharacter* Other = F.SpawnCharacter();
	if (!TestNotNull(TEXT("Replacement avatar exists"), Other)) return false;
	Other->GetWuwaMovementComponent()->MovementMode = MOVE_Walking;
	if (!TestTrue(TEXT("Replacement movement policy assembles"), Other->EnsureMovementStateSystem())
		|| !TestTrue(TEXT("Replacement skill policy assembles"), Other->EnsureSkillSystem())) return false;
	Other->RoleGaitComponent->RefreshPolicy();
	Other->InputCommandConfig = F.Config;
	F.ASC->CancelAllAbilities();
	F.ASC->InitAbilityActorInfo(Other, Other);
	F.Runtime->ProcessPendingInput(F.ASC);
	TestEqual(TEXT("An ASC avatar change discards input from the previous avatar"), F.Runtime->GetBufferedInputCount(), 0);
	TestEqual(TEXT("Old buffered input never starts a skill on the replacement"), F.Activations, 1);
	TestEqual(TEXT("The replacement has no active main skill"), Other->SkillComponent->GetCurrentSkillData().FightStateHandle, 0);
	TestTrue(TEXT("Fresh input still works on the replacement avatar"),
		F.Runtime->ProcessInput(F.ASC, F.Input(F.AttackTag), 1.0f) == EWuwaCombatInputResult::ActivationRequested);
	TestEqual(TEXT("Only the fresh input starts a replacement-avatar execution"), F.Activations, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCombatInputEndAbilityOpportunityTest,
	"Wuwa.Combat.InputBuffer.CompletedAbilityCreatesOpportunity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCombatInputEndAbilityOpportunityTest::RunTest(const FString& Parameters)
{
	using namespace WuwaCombatInputBufferTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	F.Configure(200);
	UWuwaGameplayAbilityBase* Old = F.Start(*this, F.Grant());
	if (!Old) return false;
	const FGameplayAbilitySpecHandle Candidate = F.Grant(F.AttackTag, true);
	TestTrue(TEXT("An unavailable lower-priority command buffers before the old skill ends"),
		F.Runtime->ProcessInput(F.ASC, F.Input(F.AttackTag), 1.0f) == EWuwaCombatInputResult::Buffered);
	const int64 Before = F.Skills->GetCurrentSkillData().InputOpportunitySerial;
	TestTrue(TEXT("The original execution completes through its real EndAbility path"),
		Old->TryEndSkillExecution(Old->GetSkillHandle()));
	TestEqual(TEXT("Completing the old execution clears its skill handle"),
		F.Skills->GetCurrentSkillData().FightStateHandle, 0);
	TestTrue(TEXT("The empty manager retains the new opportunity serial"),
		F.Skills->GetCurrentSkillData().InputOpportunitySerial > Before);
	TestEqual(TEXT("Completion does not reactivate inside EndAbility cleanup"), F.Activations, 1);
	TestEqual(TEXT("Pending input survives the transition from the old skill to None"), F.Runtime->GetBufferedInputCount(), 1);
	TestTrue(TEXT("The next safe processing point consumes the completion opportunity"),
		F.Runtime->ProcessPendingInput(F.ASC) == EWuwaCombatInputResult::ActivationRequested);
	TestNotNull(TEXT("Buffered skill becomes active after old cleanup has completed"), F.Active(Candidate));
	TestEqual(TEXT("Completion replays exactly one input"), F.Activations, 2);
	TestEqual(TEXT("Completion replay clears the original batch"), F.Runtime->GetBufferedInputCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCombatInputRoutedHandlerTest,
	"Wuwa.Combat.InputBuffer.ControllerRouteTickAndFlush",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCombatInputRoutedHandlerTest::RunTest(const FString& Parameters)
{
	using namespace WuwaCombatInputBufferTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	if (!F.AttachLocalController(*this)) return false;
	UWuwaAbilityInputHandlerComponent* Handler = F.Controller->AbilityInputHandler;
	UWuwaInputRouterComponent* Router = F.Controller->GetInputRouter();
	if (!TestTrue(TEXT("The fixture controller is local"), F.Controller->IsLocalController())
		|| !TestNotNull(TEXT("The controller owns its production ability handler"), Handler)
		|| !TestNotNull(TEXT("The controller owns its production input router"), Router)) return false;
	TestTrue(TEXT("Pending combat input has a component tick"), Handler->PrimaryComponentTick.bCanEverTick);
	TestTrue(TEXT("Pending combat input is scheduled after animation update"),
		Handler->PrimaryComponentTick.TickGroup == TG_PostUpdateWork);
	UInputAction* Action = NewObject<UInputAction>(F.Controller);
	auto DispatchPress = [&]()
	{
		FWuwaInputEvent Event = F.Input(F.AttackTag);
		Event.RouteTag = FWuwaGameTags::Get().Input_Route_Ability;
		Event.SourceAction = Action;
		return F.Controller->RouteInputEvent(Event);
	};
	auto TickHandler = [&]()
	{
		Handler->TickComponent(1.f / 60.f, LEVELTICK_All, &Handler->PrimaryComponentTick);
	};
	const FGameplayAbilitySpecHandle Spec = F.Grant(F.AttackTag);
	UWuwaGameplayAbilityBase* Old = F.Start(*this, Spec);
	if (!Old) return false;

	Handler->DefaultBufferLifetimeSeconds = 1.f;
	Handler->BufferLifetimeOverrides.Add(F.AttackTag, 0.f);
	TestTrue(TEXT("The configured ability route accepts the press"), DispatchPress());
	TestTrue(TEXT("The character records semantic hold independently of activation"), F.Character->InputIntent->GetActionState(F.AttackTag).bHeld);
	F.Skills->SetSkillAcceptInput(Old->GetSkillHandle(), true);
	TickHandler();
	TestEqual(TEXT("A per-tag zero lifetime disables replay despite a positive default"), F.Activations, 1);
	F.Skills->SetSkillAcceptInput(Old->GetSkillHandle(), false);

	Handler->DefaultBufferLifetimeSeconds = 0.f;
	Handler->BufferLifetimeOverrides.Add(F.AttackTag, 1.f);
	TestTrue(TEXT("A nonzero per-tag override enters the production buffer route"), DispatchPress());
	TickHandler();
	TestEqual(TEXT("The production tick leaves buffered input waiting before the window"), F.Activations, 1);
	F.Skills->SetSkillAcceptInput(Old->GetSkillHandle(), true);
	TickHandler();
	TestEqual(TEXT("The production handler tick submits the buffered command at the window"), F.Activations, 2);
	UWuwaGameplayAbilityBase* Next = F.Active(Spec);
	if (!TestNotNull(TEXT("The handler-driven replay has an active execution"), Next)) return false;
	TickHandler();
	TestEqual(TEXT("Subsequent handler ticks do not submit the consumed batch again"), F.Activations, 2);

	TestTrue(TEXT("A new press buffers behind the replacement's closed window"), DispatchPress());
	F.Controller->FlushPressedKeys();
	TestFalse(TEXT("Focus-loss flushing clears the character's semantic hold"), F.Character->InputIntent->GetActionState(F.AttackTag).bHeld);
	F.Skills->SetSkillAcceptInput(Next->GetSkillHandle(), true);
	TickHandler();
	TestEqual(TEXT("FlushPressedKeys also prevents pending combat input from replaying"), F.Activations, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCombatInputTransientSkillStartTest,
	"Wuwa.Combat.InputBuffer.InterveningSkillStartDiscardsOldBatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCombatInputTransientSkillStartTest::RunTest(const FString& Parameters)
{
	using namespace WuwaCombatInputBufferTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	F.Configure(200);
	UWuwaGameplayAbilityBase* First = F.Start(*this, F.Grant());
	if (!First) return false;
	F.Grant(F.AttackTag, true);
	TestTrue(TEXT("The first skill leaves a blocked command pending"),
		F.Runtime->ProcessInput(F.ASC, F.Input(F.AttackTag), 1.f) == EWuwaCombatInputResult::Buffered);
	const int64 FirstStart = F.Skills->GetCurrentSkillData().SkillStartSerial;
	TestTrue(TEXT("Ending the first execution succeeds"), First->TryEndSkillExecution(First->GetSkillHandle()));
	TestEqual(TEXT("An empty manager preserves the last successful skill start"),
		F.Skills->GetCurrentSkillData().SkillStartSerial, FirstStart);

	// No input-runtime call observes this entire intervening execution.
	UWuwaGameplayAbilityBase* Intervening = F.Start(*this, F.Grant());
	if (!Intervening) return false;
	const int64 InterveningStart = F.Skills->GetCurrentSkillData().SkillStartSerial;
	TestTrue(TEXT("A successful external skill start advances the serial"), InterveningStart > FirstStart);
	TestTrue(TEXT("The intervening skill completes before input is processed again"),
		Intervening->TryEndSkillExecution(Intervening->GetSkillHandle()));
	TestEqual(TEXT("Returning to None retains the intervening start identity"),
		F.Skills->GetCurrentSkillData().SkillStartSerial, InterveningStart);
	TestEqual(TEXT("The runtime has not yet observed the external start/end"), F.Runtime->GetBufferedInputCount(), 1);
	F.Runtime->ProcessPendingInput(F.ASC);
	TestEqual(TEXT("A full intervening skill execution invalidates the older batch"), F.Runtime->GetBufferedInputCount(), 0);
	TestEqual(TEXT("The older input cannot activate after the unseen execution ends"), F.Activations, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCombatInputTargetedClearTest,
	"Wuwa.Combat.InputBuffer.ClearExactTagAndAll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCombatInputTargetedClearTest::RunTest(const FString& Parameters)
{
	using namespace WuwaCombatInputBufferTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	if (!F.Start(*this, F.Grant())) return false;
	F.Grant(F.AttackTag);
	F.Grant(F.OtherTag);
	F.Runtime->ProcessInput(F.ASC, F.Input(F.AttackTag), 1.f);
	F.Runtime->ProcessInput(F.ASC, F.Input(F.AttackTag), 1.f);
	F.Runtime->ProcessInput(F.ASC, F.Input(F.OtherTag), 1.f);
	TestEqual(TEXT("The batch contains two attack events and one other event"), F.Runtime->GetBufferedInputCount(), 3);
	const FGameplayTag ParentTag = FGameplayTag::RequestGameplayTag(TEXT("GAS.GA.Role"));
	TestEqual(TEXT("Clearing a parent tag does not remove a child-tag event"), F.Runtime->ClearBufferedInput(ParentTag), 0);
	TestEqual(TEXT("Exact tag clearing removes all matching raw events"), F.Runtime->ClearBufferedInput(F.AttackTag), 2);
	TestEqual(TEXT("The other tag remains pending"), F.Runtime->GetBufferedInputCount(), 1);
	TestEqual(TEXT("Repeated exact clearing reports no additional removal"), F.Runtime->ClearBufferedInput(F.AttackTag), 0);
	TestEqual(TEXT("An empty tag clears every remaining event"), F.Runtime->ClearBufferedInput(FGameplayTag()), 1);
	TestEqual(TEXT("The whole batch is now empty"), F.Runtime->GetBufferedInputCount(), 0);
	TestEqual(TEXT("Clearing an already empty batch reports zero"), F.Runtime->ClearBufferedInput(FGameplayTag()), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCombatInputImmediateSkillEventsTest,
	"Wuwa.Combat.InputBuffer.ImmediateBreakPointAndOwnedClear",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCombatInputImmediateSkillEventsTest::RunTest(const FString& Parameters)
{
	using namespace WuwaCombatInputBufferTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this) || !F.AttachLocalController(*this)) return false;
	UWuwaAbilityInputHandlerComponent* Handler = F.Controller->AbilityInputHandler;
	Handler->DefaultBufferLifetimeSeconds = 1.f;
	UInputAction* AttackAction = NewObject<UInputAction>(F.Controller);
	UInputAction* OtherAction = NewObject<UInputAction>(F.Controller);
	auto Dispatch = [&](FGameplayTag Tag, UInputAction* Action)
	{
		FWuwaInputEvent Event = F.Input(Tag);
		Event.RouteTag = FWuwaGameTags::Get().Input_Route_Ability;
		Event.SourceAction = Action;
		return F.Controller->RouteInputEvent(Event);
	};
	F.Configure(200);
	UWuwaGameplayAbilityBase* Old = F.Start(*this, F.Grant());
	if (!Old) return false;
	const int32 OldHandle = Old->GetSkillHandle();
	const FGameplayAbilitySpecHandle AttackSpec = F.Grant(F.AttackTag, true);
	F.Configure(100);
	const FGameplayAbilitySpecHandle OtherSpec = F.Grant(F.OtherTag);
	TestTrue(TEXT("Attack enters the real handler buffer"), Dispatch(F.AttackTag, AttackAction));
	TestTrue(TEXT("Another tag enters the same handler buffer"), Dispatch(F.OtherTag, OtherAction));
	TestTrue(TEXT("A live owner can request a standalone breakpoint"), F.Skills->CallAnimBreakPoint(Old->GetSkillHandle()));
	TestEqual(TEXT("A standalone breakpoint cannot bypass closed interrupt permissions"), F.Activations, 1);
	TestFalse(TEXT("The breakpoint does not open accept-input"), F.Skills->GetCurrentSkillData().bSkillAcceptInput);
	TestFalse(TEXT("The breakpoint does not mark ready-end"), F.Skills->GetCurrentSkillData().bMainSkillReadyEnd);
	TestTrue(TEXT("The owning GA can request exact-tag cache clearing"), F.Skills->RequestInputCacheClear(Old->GetSkillHandle(), F.AttackTag));
	TestEqual(TEXT("The skill event already removed the matching buffered event"), Handler->ClearBufferedInput(F.AttackTag), 0);
	TestTrue(TEXT("Clearing preinput leaves the attack key semantically held"), F.Character->InputIntent->GetActionState(F.AttackTag).bHeld);
	TestTrue(TEXT("The other key remains semantically held too"), F.Character->InputIntent->GetActionState(F.OtherTag).bHeld);
	F.Skills->SetMainSkillReadyEnd(Old->GetSkillHandle(), true);
	TestTrue(TEXT("The owning GA broadcasts the new eligible breakpoint"), F.Skills->CallAnimBreakPoint(Old->GetSkillHandle()));
	TestEqual(TEXT("The skill event activates immediately without any handler tick"), F.Activations, 2);
	TestNull(TEXT("The cleared attack event never activates"), F.Active(AttackSpec));
	UWuwaGameplayAbilityBase* Next = F.Active(OtherSpec);
	if (!TestNotNull(TEXT("The uncleared candidate is the new current execution"), Next)) return false;
	TestTrue(TEXT("A fresh event is buffered behind the replacement"), Dispatch(F.OtherTag, OtherAction));
	TestFalse(TEXT("An old skill handle cannot clear the replacement's pending input"),
		F.Skills->RequestInputCacheClear(OldHandle, F.OtherTag));
	TestFalse(TEXT("An old skill handle cannot emit a new breakpoint"), F.Skills->CallAnimBreakPoint(OldHandle));
	F.Skills->SetSkillAcceptInput(Next->GetSkillHandle(), true);
	TestTrue(TEXT("The current skill can request its own breakpoint"), F.Skills->CallAnimBreakPoint(Next->GetSkillHandle()));
	TestEqual(TEXT("Stale clear did not erase the new skill's pending same-spec replay"), F.Activations, 3);
	return true;
}

#endif
