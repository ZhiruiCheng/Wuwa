#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Animation/ActiveMontageInstanceScope.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "UObject/Script.h"
#include "Game/Animation/Notifies/WuwaAnimNotifyState_SkillAcceptInputBridge.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_SkillReadyEndBridge.h"
#include "Game/Common/WuwaGameTags.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Input/WuwaInputRouterComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"
#include "Game/NewWorld/Character/Common/Component/Combat/WuwaFightStateBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaAbilityInputHandlerComponent.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputCommandConfig.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "WuwaSkillNotifyTestTypes.h"
#include "WuwaSkillTestAbility.h"

namespace WuwaSkillNotifyTests
{
	struct FFixture
	{
		UClass* AbilityClass;
		UClass* AnimClass;
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AWuwaCharacter* Character = nullptr;
		UWuwaAbilitySystemComponent* ASC = nullptr;
		UAnimInstance* AnimInstance = nullptr;
		UAnimMontage* Montage = nullptr;
		UWuwaAnimNotifyState_SkillAcceptInputBridge* WindowA = nullptr;
		UWuwaAnimNotifyState_SkillAcceptInputBridge* WindowB = nullptr;
		UWuwaAnimNotify_SkillReadyEndBridge* Ready = nullptr;
		FGameplayAbilitySpecHandle SpecHandle;
		static constexpr int32 TestSkillId = 1;
		UWuwaGameplayAbilityBase* Ability = nullptr;
		int32 MontageInstanceId = INDEX_NONE;
		UWuwaSkillTestAbility* Defaults;
		TGuardValue<bool> MainGuard{Defaults->bIsMainSkill, true};
		TGuardValue<int32> LevelGuard{Defaults->InterruptLevel, 100};
		TGuardValue<EWuwaSkillOverrideType> OverrideGuard{Defaults->SkillOverrideType, EWuwaSkillOverrideType::None};
		TGuardValue<EWuwaMoveState> MovementGuard{Defaults->StartMoveState, EWuwaMoveState::Other};

		explicit FFixture(UClass* InAbilityClass = UWuwaSkillTestAbility::StaticClass(),
			UClass* InAnimClass = UAnimInstance::StaticClass())
			: AbilityClass(InAbilityClass), AnimClass(InAnimClass),
			  Defaults(InAbilityClass->GetDefaultObject<UWuwaSkillTestAbility>()) {}

		~FFixture()
		{
			if (ASC) ASC->CancelAllAbilities();
			if (AnimInstance) AnimInstance->UninitializeAnimation();
			if (ASC) ASC->ClearActorInfo();
			if (World) World->DestroyWorld(false);
		}

		bool Initialize(FAutomationTestBase& Test)
		{
			if (!Test.TestNotNull(TEXT("Transient game world exists"), World)) return false;
			FActorSpawnParameters Spawn;
			Spawn.ObjectFlags |= RF_Transient;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Character = World->SpawnActor<AWuwaCharacter>(Spawn);
			if (!Test.TestNotNull(TEXT("Character exists"), Character)
				|| !Test.TestTrue(TEXT("Real managed skill system assembles"), Character->EnsureSkillSystem())) return false;
			USkeletalMesh* MeshAsset = LoadObject<USkeletalMesh>(nullptr,
				TEXT("/Game/Characters/Role/changli/Model/Changli.Changli"));
			UAnimMontage* SourceMontage = LoadObject<UAnimMontage>(nullptr,
				TEXT("/Game/Characters/Role/changli/AnimMontage/AM_Changli_Dash_F.AM_Changli_Dash_F"));
			UClass* WindowClass = LoadClass<UWuwaAnimNotifyState_SkillAcceptInputBridge>(nullptr,
				TEXT("/Script/UnrealSharp.AnimNotifyState_SkillAcceptInput_C"));
			UClass* ReadyClass = LoadClass<UWuwaAnimNotify_SkillReadyEndBridge>(nullptr,
				TEXT("/Script/UnrealSharp.AnimNotify_SkillReadyEnd_C"));
			if (!Test.TestNotNull(TEXT("Existing character mesh loads"), MeshAsset)
				|| !Test.TestNotNull(TEXT("Existing montage loads"), SourceMontage)
				|| !Test.TestNotNull(TEXT("Managed accept-input notify class loads"), WindowClass)
				|| !Test.TestNotNull(TEXT("Managed ready-end notify class loads"), ReadyClass)) return false;
			Test.TestEqual(TEXT("Accept-input notify uses the generated C# class"), WindowClass->GetClass()->GetFName(), FName(TEXT("CSClass")));
			Test.TestEqual(TEXT("Ready-end notify uses the generated C# class"), ReadyClass->GetClass()->GetFName(), FName(TEXT("CSClass")));

			// Modify only a transient duplicate; the user's montage and notify tracks are never edited.
			Montage = DuplicateObject<UAnimMontage>(SourceMontage, GetTransientPackage());
			Montage->SetFlags(RF_Transient);
			Montage->Notifies.Reset();
			WindowA = NewObject<UWuwaAnimNotifyState_SkillAcceptInputBridge>(Montage, WindowClass);
			WindowB = NewObject<UWuwaAnimNotifyState_SkillAcceptInputBridge>(Montage, WindowClass);
			Ready = NewObject<UWuwaAnimNotify_SkillReadyEndBridge>(Montage, ReadyClass);
			FAnimNotifyEvent A;
			A.NotifyStateClass = WindowA;
			A.Link(Montage, 0.05f);
			A.SetDuration(0.15f);
			Montage->Notifies.Add(A);
			FAnimNotifyEvent B;
			B.NotifyStateClass = WindowB;
			B.Link(Montage, 0.1f);
			B.SetDuration(0.15f);
			B.TrackIndex = 1;
			Montage->Notifies.Add(B);
			FAnimNotifyEvent ReadyEvent;
			ReadyEvent.Notify = Ready;
			ReadyEvent.Link(Montage, 0.2f);
			Montage->Notifies.Add(ReadyEvent);

			USkeletalMeshComponent* Mesh = Character->GetMesh();
			Mesh->SetSkeletalMeshAsset(MeshAsset);
			Mesh->SetAnimInstanceClass(AnimClass);
			Mesh->InitAnim(true);
			AnimInstance = Mesh->GetAnimInstance();
			if (!Test.TestNotNull(TEXT("Native animation instance is initialized"), AnimInstance)) return false;
			ASC = NewObject<UWuwaAbilitySystemComponent>(Character);
			ASC->RegisterComponent();
			ASC->InitAbilityActorInfo(Character, Character);
			// 按技能表的方式授予（技能 ID = TestSkillId），按键规则表才能选中它
			FWuwaSkillInfo Info;
			Info.Ability = AbilityClass;
			SpecHandle = ASC->GiveSkill(TestSkillId, Info, nullptr);
			return Test.TestTrue(TEXT("Test skill is granted"), SpecHandle.IsValid());
		}

		bool Start(FAutomationTestBase& Test)
		{
			if (!Test.TestTrue(TEXT("Real GAS skill activates"), ASC->TryActivateAbility(SpecHandle, false))) return false;
			Ability = Character->SkillComponent->GetCurrentSkillData().ActiveAbility.Get();
			if (!Test.TestNotNull(TEXT("Managed manager records the active GA"), Ability)) return false;
			if (ASC->GetAnimatingAbility() != Ability || ASC->GetCurrentMontage() != Montage)
			{
				const float Duration = ASC->PlayMontage(Ability, FGameplayAbilityActivationInfo(), Montage, 1.f);
				if (!Test.TestTrue(TEXT("ASC starts actual montage playback"), Duration > 0.f)) return false;
			}
			FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(Montage);
			if (!Test.TestNotNull(TEXT("Actual montage playback instance exists"), Instance)) return false;
			MontageInstanceId = Instance->GetInstanceID();
			return Test.TestTrue(TEXT("Montage instance id can include zero"), MontageInstanceId >= 0);
		}

		FAnimNotifyEventReference QueuedReference(int32 EventIndex, int32 InstanceId) const
		{
			FAnimNotifyEventReference Reference(&Montage->Notifies[EventIndex], Montage);
			Reference.AddContextData<UE::Anim::FAnimNotifyMontageInstanceContext>(InstanceId);
			return Reference;
		}

		FBranchingPointNotifyPayload BranchingPayload(int32 EventIndex, int32 InstanceId) const
		{
			return FBranchingPointNotifyPayload(Character->GetMesh(), Montage,
				&Montage->Notifies[EventIndex], InstanceId);
		}

		bool AcceptsInput() const { return Character->SkillComponent->GetCurrentSkillData().bSkillAcceptInput; }
	};

	struct FTimelineFixture : FFixture
	{
		UWuwaSkillNotifyTestAbility* TimelineDefaults;
		TGuardValue<TObjectPtr<UAnimMontage>> MontageGuard;
		TGuardValue<bool> PausedGuard;
		AWuwaPlayerController* Controller = nullptr;
		UWuwaInputRouterComponent* Router = nullptr;
		UInputAction* AttackAction = nullptr;
		int32 Activations = 0;
		FWuwaSkillData BeforeSecondActivation;
		FWuwaFightStateData FightBeforeSecondActivation;
		FDelegateHandle ActivationDelegate;
		const FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(FName(TEXT("GAS.GA.Role.Attack1")));

		FTimelineFixture()
			: FFixture(UWuwaSkillNotifyTestAbility::StaticClass(), UWuwaSkillNotifyTestAnimInstance::StaticClass()),
			  TimelineDefaults(CastChecked<UWuwaSkillNotifyTestAbility>(Defaults)),
			  MontageGuard(TimelineDefaults->TestMontage, nullptr), PausedGuard(TimelineDefaults->bPauseTestMontage, false) {}

		~FTimelineFixture()
		{
			if (Controller) Controller->SetPawn(nullptr);
			if (ASC) ASC->AbilityActivatedCallbacks.Remove(ActivationDelegate);
		}

		bool Initialize(FAutomationTestBase& Test)
		{
			if (!FFixture::Initialize(Test)) return false;
			World->TimeSeconds = 1.0;
			TimelineDefaults->TestMontage = Montage;
			// The tests author notification times in seconds, independently of the
			// imported Dash's playback tuning. Keep all test windows before blend-out.
			Montage->RateScale = 1.f;
			Montage->BlendOutTriggerTime = 0.f;
			UWuwaInputCommandConfig* Config = NewObject<UWuwaInputCommandConfig>(Character);
			FWuwaInputCommandRule Rule;
			Rule.RuleName = TEXT("Attack");
			Rule.InputTag = AttackTag;
			Rule.TargetSkillId = TestSkillId;
			Config->Rules.Add(Rule);
			Character->InputCommandConfig = Config;
			ActivationDelegate = ASC->AbilityActivatedCallbacks.AddLambda([this](UGameplayAbility*)
			{
				++Activations;
				if (Activations == 2)
				{
					// GAS publishes activation before the new GA registers with SkillComponent.
					BeforeSecondActivation = Character->SkillComponent->GetCurrentSkillData();
					FightBeforeSecondActivation = Character->FightStateComponent->GetStateData();
				}
			});

			FActorSpawnParameters Spawn;
			Spawn.ObjectFlags |= RF_Transient;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Controller = World->SpawnActor<AWuwaPlayerController>(Spawn);
			if (!Test.TestNotNull(TEXT("Real input controller exists"), Controller)) return false;
			Controller->SetAsLocalPlayerController();
			ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
			LocalPlayer->PlayerController = Controller;
			Controller->Player = LocalPlayer;
			Controller->SetPawn(Character);
			Character->SetController(Controller);
			ASC->InitAbilityActorInfo(Character, Character);
			Controller->AscComponent = ASC;
			Controller->RegisterInputRouteHandlers();
			Router = Controller->GetInputRouter();
			UWuwaAbilityInputHandlerComponent* Handler = Controller->AbilityInputHandler;
			if (!Test.TestTrue(TEXT("Input controller is local"), Controller->IsLocalController())
				|| !Test.TestNotNull(TEXT("Production input router exists"), Router)
				|| !Test.TestNotNull(TEXT("Production ability input handler exists"), Handler)) return false;
			Handler->DefaultBufferLifetimeSeconds = 1.f;
			AttackAction = NewObject<UInputAction>(Controller);
			return true;
		}

		bool PressAttack() const
		{
			FWuwaInputEvent Event;
			Event.InputTag = AttackTag;
			Event.RouteTag = FWuwaGameTags::Get().Input_Route_Ability;
			Event.Phase = EWuwaInputPhase::Pressed;
			Event.Timestamp = World->GetTimeSeconds();
			Event.SourceAction = AttackAction;
			return Router->DispatchInput(Event);
		}

		void Advance(float DeltaSeconds) const
		{
			World->TimeSeconds += DeltaSeconds;
			CastChecked<UWuwaSkillNotifyTestAnimInstance>(AnimInstance)->AdvanceNotifyTimeline(DeltaSeconds);
		}

		void ConfigureWindow(EMontageNotifyTickType::Type TickType) const
		{
			Montage->Notifies.SetNum(1);
			FAnimNotifyEvent& Event = Montage->Notifies[0];
			Event.Link(Montage, 0.05f);
			Event.SetDuration(0.01f);
			Event.MontageTickType = TickType;
			Event.TriggerWeightThreshold = 0.f;
			Montage->RefreshCacheData();
		}

		void ConfigureReadyEnd(EMontageNotifyTickType::Type TickType) const
		{
			FAnimNotifyEvent Event = Montage->Notifies[2];
			Event.Link(Montage, 0.05f);
			Event.MontageTickType = TickType;
			Event.TriggerWeightThreshold = 0.f;
			Montage->Notifies = {Event};
			Montage->RefreshCacheData();
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSkillNotifyWindowsTest, "Wuwa.Combat.SkillNotify.AcceptWindowsAndPlaybackOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSkillNotifyWindowsTest::RunTest(const FString& Parameters)
{
	using namespace WuwaSkillNotifyTests;
	FEditorScriptExecutionGuard ScriptExecutionGuard;
	FFixture F;
	if (!F.Initialize(*this) || !F.Start(*this)) return false;
	USkeletalMeshComponent* Mesh = F.Character->GetMesh();
	const int32 FirstInstance = F.MontageInstanceId;
	const int32 FirstSkill = F.Ability->GetSkillHandle();
	TestFalse(TEXT("No input window is open initially"), F.AcceptsInput());
	FAnimNotifyEventReference MissingContext(&F.Montage->Notifies[0], F.Montage);
	F.WindowA->NotifyBegin(Mesh, F.Montage, 0.15f, MissingContext);
	TestFalse(TEXT("Notify without a montage instance context is ignored"), F.AcceptsInput());
	F.WindowA->NotifyBegin(Mesh, F.Montage, 0.15f, F.QueuedReference(0, FirstInstance + 100000));
	TestFalse(TEXT("Notify from another playback instance is ignored"), F.AcceptsInput());
	F.WindowA->NotifyBegin(Mesh, F.Montage, 0.15f, F.QueuedReference(0, FirstInstance));
	TestTrue(TEXT("Native queued context reaches the C# window Begin"), F.AcceptsInput());
	const int64 OpenSerial = F.Character->SkillComponent->GetCurrentSkillData().InputOpportunitySerial;
	F.WindowA->NotifyBegin(Mesh, F.Montage, 0.15f, F.QueuedReference(0, FirstInstance));
	TestEqual(TEXT("Duplicate Begin does not publish another input opportunity"),
		F.Character->SkillComponent->GetCurrentSkillData().InputOpportunitySerial, OpenSerial);
	FBranchingPointNotifyPayload BranchB = F.BranchingPayload(1, FirstInstance);
	F.WindowB->BranchingPointNotifyBegin(BranchB);
	TestTrue(TEXT("Overlapping branching-point window opens"), F.AcceptsInput());

	// AnimInstance supplies a copied event on queued End; it no longer points into Montage.Notifies.
	FAnimNotifyEvent CopiedA = F.Montage->Notifies[0];
	FAnimNotifyEventReference CopiedEnd(&CopiedA, F.Montage);
	CopiedEnd.AddContextData<UE::Anim::FAnimNotifyMontageInstanceContext>(FirstInstance);
	F.WindowA->NotifyEnd(Mesh, F.Montage, CopiedEnd);
	TestTrue(TEXT("Ending one window preserves the overlapping window"), F.AcceptsInput());
	F.WindowB->BranchingPointNotifyEnd(BranchB);
	TestFalse(TEXT("Ending the final window closes acceptance"), F.AcceptsInput());
	TestEqual(TEXT("Window changes do not end the current skill"), F.Character->SkillComponent->GetCurrentSkillData().FightStateHandle, FirstSkill);

	{
		FFixture Other;
		if (!Other.Initialize(*this)) return false;
		Other.Montage = F.Montage;
		if (!Other.Start(*this)) return false;
		F.WindowA->NotifyBegin(Mesh, F.Montage, 0.15f, F.QueuedReference(0, FirstInstance));
		F.WindowA->NotifyBegin(Other.Character->GetMesh(), F.Montage, 0.15f,
			Other.QueuedReference(0, Other.MontageInstanceId));
		TestTrue(TEXT("One shared notify object can open a window on each character"), F.AcceptsInput() && Other.AcceptsInput());
		F.WindowA->NotifyEnd(Mesh, F.Montage, F.QueuedReference(0, FirstInstance));
		TestFalse(TEXT("First character closes its own shared-notify window"), F.AcceptsInput());
		TestTrue(TEXT("First character's End cannot close the other character's window"), Other.AcceptsInput());
		F.WindowA->NotifyEnd(Other.Character->GetMesh(), F.Montage, Other.QueuedReference(0, Other.MontageInstanceId));
		TestFalse(TEXT("Other character closes its own window independently"), Other.AcceptsInput());
	}

	F.WindowA->NotifyBegin(Mesh, F.Montage, 0.15f, F.QueuedReference(0, FirstInstance));
	F.Ability->TryEndSkillExecution(FirstSkill);
	TestFalse(TEXT("Ending a skill clears its open windows even before NotifyEnd"), F.AcceptsInput());
	if (!F.Start(*this)) return false;
	const int32 SecondInstance = F.MontageInstanceId;
	TestNotEqual(TEXT("Replaying the same montage has a distinct playback id"), SecondInstance, FirstInstance);
	F.WindowA->NotifyBegin(Mesh, F.Montage, 0.15f, F.QueuedReference(0, SecondInstance));
	F.WindowA->NotifyEnd(Mesh, F.Montage, CopiedEnd);
	TestTrue(TEXT("Late queued End from the old playback cannot close the new window"), F.AcceptsInput());
	F.WindowB->BranchingPointNotifyBegin(BranchB);
	F.WindowB->BranchingPointNotifyEnd(BranchB);
	TestTrue(TEXT("Old branching-point callbacks cannot close the new window"), F.AcceptsInput());
	F.WindowA->NotifyEnd(Mesh, F.Montage, F.QueuedReference(0, SecondInstance));
	TestFalse(TEXT("Only the new playback's matching End closes its window"), F.AcceptsInput());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSkillNotifyReadyEndTest, "Wuwa.Combat.SkillNotify.ReadyEndKeepsExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSkillNotifyReadyEndTest::RunTest(const FString& Parameters)
{
	using namespace WuwaSkillNotifyTests;
	FEditorScriptExecutionGuard ScriptExecutionGuard;
	FFixture F;
	if (!F.Initialize(*this) || !F.Start(*this)) return false;
	UWuwaSkillBridgeComponent* Skills = F.Character->SkillComponent;
	UWuwaFightStateBridgeComponent* Fight = F.Character->FightStateComponent;
	const int32 FirstSkill = F.Ability->GetSkillHandle();
	const int32 FirstInstance = F.MontageInstanceId;
	const FWuwaFightStateData Before = Fight->GetStateData();
	TestEqual(TEXT("Skill starts at the configured interrupt level"), Skills->GetCurrentSkillData().InterruptLevel, 100);
	F.Ready->Notify(F.Character->GetMesh(), F.Montage, F.QueuedReference(2, FirstInstance));
	const FWuwaSkillData ReadySkill = Skills->GetCurrentSkillData();
	TestTrue(TEXT("Queued ReadyEnd notification opens the ready-end flag"), ReadySkill.bMainSkillReadyEnd);
	TestEqual(TEXT("ReadyEnd lowers only the current skill's runtime interrupt level"), ReadySkill.InterruptLevel, 0);
	TestEqual(TEXT("ReadyEnd retains the same skill handle"), ReadySkill.FightStateHandle, FirstSkill);
	TestTrue(TEXT("ReadyEnd does not end the GA"), F.Ability->IsSkillExecutionActive());
	TestEqual(TEXT("ReadyEnd does not rewrite GA configuration"), F.Ability->InterruptLevel, 100);
	TestEqual(TEXT("ReadyEnd retains fight-state priority"), Fight->GetStateData().SubStatePriority, Before.SubStatePriority);
	TestEqual(TEXT("ReadyEnd retains fight-state ownership"), Fight->GetStateData().Handle, Before.Handle);
	FAnimMontageInstance* Instance = F.AnimInstance->GetActiveInstanceForMontage(F.Montage);
	if (!TestNotNull(TEXT("ReadyEnd leaves montage playback active"), Instance)) return false;
	TestEqual(TEXT("ReadyEnd does not replace montage playback"), Instance->GetInstanceID(), FirstInstance);
	TestFalse(TEXT("ReadyEnd does not begin montage blend-out"), Instance->IsStopped());
	TestEqual(TEXT("ASC retains the same animating GA"), F.ASC->GetAnimatingAbility(), static_cast<UGameplayAbility*>(F.Ability));

	F.Ability->TryEndSkillExecution(FirstSkill);
	if (!F.Start(*this)) return false;
	TestFalse(TEXT("The next skill starts before ready-end"), Skills->GetCurrentSkillData().bMainSkillReadyEnd);
	TestEqual(TEXT("The next skill restores configured interrupt priority"), Skills->GetCurrentSkillData().InterruptLevel, 100);
	FBranchingPointNotifyPayload OldReady = F.BranchingPayload(2, FirstInstance);
	F.Ready->BranchingPointNotify(OldReady);
	TestFalse(TEXT("Late ReadyEnd from the old playback cannot change the new skill"), Skills->GetCurrentSkillData().bMainSkillReadyEnd);
	TestEqual(TEXT("Late ReadyEnd preserves the new skill's interrupt priority"), Skills->GetCurrentSkillData().InterruptLevel, 100);
	FBranchingPointNotifyPayload NewReady = F.BranchingPayload(2, F.MontageInstanceId);
	F.Ready->BranchingPointNotify(NewReady);
	TestTrue(TEXT("Current branching-point ReadyEnd reaches the managed handler"), Skills->GetCurrentSkillData().bMainSkillReadyEnd);
	TestEqual(TEXT("Branching-point ReadyEnd lowers the current runtime priority"), Skills->GetCurrentSkillData().InterruptLevel, 0);
	TestTrue(TEXT("Branching-point ReadyEnd keeps the GA running"), F.Ability->IsSkillExecutionActive());
	TestEqual(TEXT("Branching-point ReadyEnd leaves fight priority unchanged"), Fight->GetStateData().SubStatePriority, 100);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSkillNotifyTimelineReplayTest,
	"Wuwa.Combat.SkillNotify.ShortWindowsConsumeWithoutInputTick",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSkillNotifyTimelineReplayTest::RunTest(const FString& Parameters)
{
	using namespace WuwaSkillNotifyTests;
	FEditorScriptExecutionGuard ScriptExecutionGuard;
	for (const auto TickType : {EMontageNotifyTickType::Queued, EMontageNotifyTickType::BranchingPoint})
	{
		const FString Mode = TickType == EMontageNotifyTickType::Queued ? TEXT("Queued") : TEXT("BranchingPoint");
		AddInfo(FString::Printf(TEXT("Real montage timeline / %s"), *Mode));
		FTimelineFixture F;
		if (!F.Initialize(*this)) return false;
		F.ConfigureWindow(TickType);
		if (!F.Start(*this)) return false;
		UWuwaSkillBridgeComponent* Skills = F.Character->SkillComponent;
		UWuwaGameplayAbilityBase* OldAbility = F.Ability;
		const int32 OldSkill = OldAbility->GetSkillHandle();
		const int32 OldPlayback = F.MontageInstanceId;
		F.TimelineDefaults->bPauseTestMontage = true;
		TestTrue(*FString::Printf(TEXT("%s: first attack enters the actual controller route"), *Mode), F.PressAttack());
		TestTrue(*FString::Printf(TEXT("%s: second attack enters the same pending batch"), *Mode), F.PressAttack());
		TestEqual(*FString::Printf(TEXT("%s: closed window keeps both presses buffered"), *Mode), F.Activations, 1);

		// One engine animation update crosses the complete [0.05, 0.06] window.
		// No handler/component/world Tick is performed: the animation breakpoint
		// must consume the batch synchronously, while this window is still open.
		F.Advance(0.08f);
		TestEqual(*FString::Printf(TEXT("%s: crossing a complete short window executes exactly one buffered attack"), *Mode),
			F.Activations, 2);
		const FWuwaSkillData Replacement = Skills->GetCurrentSkillData();
		if (!TestNotNull(*FString::Printf(TEXT("%s: replacement skill remains active"), *Mode), Replacement.ActiveAbility.Get())) return false;
		TestNotEqual(*FString::Printf(TEXT("%s: replay receives a new skill handle"), *Mode), Replacement.FightStateHandle, OldSkill);
		TestFalse(*FString::Printf(TEXT("%s: handoff ends the old GA"), *Mode), OldAbility->IsSkillExecutionActive());
		TestTrue(*FString::Printf(TEXT("%s: candidate observes acceptance before synchronous handoff"), *Mode),
			F.BeforeSecondActivation.bSkillAcceptInput);
		TestEqual(*FString::Printf(TEXT("%s: permission was supplied by the outgoing skill"), *Mode),
			F.BeforeSecondActivation.FightStateHandle, OldSkill);
		FAnimMontageInstance* NewPlayback = F.AnimInstance->GetActiveInstanceForMontage(F.Montage);
		if (!TestNotNull(*FString::Printf(TEXT("%s: replacement actually plays the same montage asset"), *Mode), NewPlayback)) return false;
		TestNotEqual(*FString::Printf(TEXT("%s: replacement owns a distinct playback instance"), *Mode), NewPlayback->GetInstanceID(), OldPlayback);
		TestFalse(*FString::Printf(TEXT("%s: old window permissions do not leak into the replacement"), *Mode), F.AcceptsInput());

		// Queued ANS End is dispatched on the next animation update; branching End
		// may already have run during the stopping callback. Both must be harmless
		// to the new skill. Its montage is paused, so it has not opened a new window.
		F.Advance(0.01f);
		TestEqual(*FString::Printf(TEXT("%s: deferred old callbacks preserve new ownership"), *Mode),
			Skills->GetCurrentSkillData().FightStateHandle, Replacement.FightStateHandle);
		TestFalse(*FString::Printf(TEXT("%s: deferred old callbacks do not open acceptance"), *Mode), F.AcceptsInput());

		F.AnimInstance->Montage_Resume(F.Montage);
		F.Advance(0.055f);
		TestTrue(*FString::Printf(TEXT("%s: the replacement opens its own window through the actual timeline"), *Mode), F.AcceptsInput());
		TestEqual(*FString::Printf(TEXT("%s: the second cached press was consumed with the first batch"), *Mode), F.Activations, 2);
		// Deliberately deliver an additional delayed callback after the new window
		// opens. Same asset/event index, different native playback identity.
		F.WindowA->NotifyEnd(F.Character->GetMesh(), F.Montage, F.QueuedReference(0, OldPlayback));
		TestTrue(*FString::Printf(TEXT("%s: late old End cannot close the new playback's window"), *Mode), F.AcceptsInput());
		F.Advance(0.02f);
		F.Advance(0.01f);
		TestFalse(*FString::Printf(TEXT("%s: the matching new End closes its window"), *Mode), F.AcceptsInput());
		TestEqual(*FString::Printf(TEXT("%s: timeline completion does not replay a consumed input"), *Mode), F.Activations, 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSkillNotifyReadyEndReplayOrderTest,
	"Wuwa.Combat.SkillNotify.ReadyEndUpdatesBeforeSynchronousReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSkillNotifyReadyEndReplayOrderTest::RunTest(const FString& Parameters)
{
	using namespace WuwaSkillNotifyTests;
	FEditorScriptExecutionGuard ScriptExecutionGuard;
	for (const auto TickType : {EMontageNotifyTickType::Queued, EMontageNotifyTickType::BranchingPoint})
	{
		const FString Mode = TickType == EMontageNotifyTickType::Queued ? TEXT("Queued") : TEXT("BranchingPoint");
		AddInfo(FString::Printf(TEXT("ReadyEnd timeline / %s"), *Mode));
		FTimelineFixture F;
		if (!F.Initialize(*this)) return false;
		F.ConfigureReadyEnd(TickType);
		F.Defaults->InterruptLevel = 200;
		if (!F.Start(*this)) return false;
		const int32 OldSkill = F.Ability->GetSkillHandle();
		const int32 OldPlayback = F.MontageInstanceId;
		F.Defaults->InterruptLevel = 100;
		F.TimelineDefaults->bPauseTestMontage = true;
		TestTrue(*FString::Printf(TEXT("%s: lower priority input enters the real route"), *Mode), F.PressAttack());
		TestEqual(*FString::Printf(TEXT("%s: lower priority input waits for ReadyEnd"), *Mode), F.Activations, 1);
		F.Advance(0.08f);
		TestEqual(*FString::Printf(TEXT("%s: ReadyEnd synchronously submits the valid lower priority candidate"), *Mode), F.Activations, 2);
		TestEqual(*FString::Printf(TEXT("%s: snapshot was captured before the old skill was replaced"), *Mode),
			F.BeforeSecondActivation.FightStateHandle, OldSkill);
		TestTrue(*FString::Printf(TEXT("%s: ReadyEnd flag is set before input replay"), *Mode), F.BeforeSecondActivation.bMainSkillReadyEnd);
		TestEqual(*FString::Printf(TEXT("%s: runtime interrupt level is zero before input replay"), *Mode),
			F.BeforeSecondActivation.InterruptLevel, 0);
		TestEqual(*FString::Printf(TEXT("%s: ReadyEnd does not rewrite the outgoing fight priority"), *Mode),
			F.FightBeforeSecondActivation.SubStatePriority, 200);
		TestEqual(*FString::Printf(TEXT("%s: outgoing fight owner remains intact until actual skill handoff"), *Mode),
			F.FightBeforeSecondActivation.Handle, OldSkill);
		const FWuwaSkillData Replacement = F.Character->SkillComponent->GetCurrentSkillData();
		if (!TestNotNull(*FString::Printf(TEXT("%s: lower priority replacement remains active"), *Mode), Replacement.ActiveAbility.Get())) return false;
		TestNotEqual(*FString::Printf(TEXT("%s: lower priority replacement receives its own handle"), *Mode), Replacement.FightStateHandle, OldSkill);
		TestEqual(*FString::Printf(TEXT("%s: replacement uses its own configured priority"), *Mode), Replacement.InterruptLevel, 100);
		TestFalse(*FString::Printf(TEXT("%s: post-callback writes cannot mark the replacement ReadyEnd"), *Mode), Replacement.bMainSkillReadyEnd);
		FAnimMontageInstance* Instance = F.AnimInstance->GetActiveInstanceForMontage(F.Montage);
		if (!TestNotNull(*FString::Printf(TEXT("%s: replacement montage remains active after old callback returns"), *Mode), Instance)) return false;
		TestNotEqual(*FString::Printf(TEXT("%s: ReadyEnd handoff starts a new playback instance"), *Mode), Instance->GetInstanceID(), OldPlayback);
	}
	return true;
}

#endif
