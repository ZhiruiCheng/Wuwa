#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"
#include "Game/Common/WuwaGameTags.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
#include "Game/NewWorld/Character/Common/Component/Input/UWuwaCombatInputRuntimeBridge.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputCommandConfig.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "WuwaInputConditionTestTypes.h"
#include "WuwaSkillTestAbility.h"

namespace WuwaInputCommandTests
{
	struct FFixture
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AWuwaCharacter* Character = nullptr;
		UWuwaAbilitySystemComponent* ASC = nullptr;
		UWuwaSkillBridgeComponent* Skills = nullptr;
		UWuwaCombatInputRuntimeBridge* Runtime = nullptr;
		UWuwaInputCommandConfig* Config = nullptr;
		UWuwaSkillTestAbility* Defaults = GetMutableDefault<UWuwaSkillTestAbility>();
		TGuardValue<bool> MainGuard{Defaults->bIsMainSkill, true};
		TGuardValue<int32> LevelGuard{Defaults->InterruptLevel, 100};
		TGuardValue<EWuwaSkillOverrideType> OverrideGuard{Defaults->SkillOverrideType, EWuwaSkillOverrideType::None};
		TGuardValue<EWuwaMoveState> MovementGuard{Defaults->StartMoveState, EWuwaMoveState::Other};
		int32 Activations = 0;
		const FGameplayTag InputTag = FGameplayTag::RequestGameplayTag(TEXT("GAS.GA.Role.Attack1"));
		// 测试用技能 ID（身份）；DirectSkill 是一个与规则无关的已授予技能，验证规则之外不会被激活
		const int32 Skill1 = 101;
		const int32 Skill2 = 102;
		const int32 Skill3 = 103;
		const int32 MissingSkill = 105;
		const int32 DirectSkill = 199;
		const FGameplayTag NormalAttack = FGameplayTag::RequestGameplayTag(TEXT("Skill.Type.NormalAttack"));
		const FGameplayTag OwnerTag = FGameplayTag::RequestGameplayTag(TEXT("WuwaASC.GE.SpeedUp"));
		const FGameplayTag UnknownInput = FGameplayTag::RequestGameplayTag(TEXT("Animation.Montage.Event"));

		~FFixture()
		{
			if (Runtime) Runtime->ResetInput();
			if (ASC)
			{
				ASC->CancelAllAbilities();
				ASC->ClearActorInfo();
			}
			if (World) World->DestroyWorld(false);
		}

		bool Initialize(FAutomationTestBase& Test)
		{
			if (!Test.TestNotNull(TEXT("Transient game world exists"), World)) return false;
			World->TimeSeconds = 1.0;
			FActorSpawnParameters Spawn;
			Spawn.ObjectFlags |= RF_Transient;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Character = World->SpawnActor<AWuwaCharacter>(Spawn);
			if (!Test.TestNotNull(TEXT("Character exists"), Character)
				|| !Test.TestTrue(TEXT("Real managed skill policy assembles"), Character->EnsureSkillSystem())) return false;
			Skills = Character->SkillComponent;
			ASC = NewObject<UWuwaAbilitySystemComponent>(Character);
			ASC->RegisterComponent();
			ASC->InitAbilityActorInfo(Character, Character);
			ASC->AbilityActivatedCallbacks.AddLambda([this](UGameplayAbility*) { ++Activations; });
			UClass* RuntimeClass = LoadClass<UWuwaCombatInputRuntimeBridge>(nullptr,
				TEXT("/Script/UnrealSharp.WuwaCombatInputRuntime_C"));
			if (!Test.TestNotNull(TEXT("Production managed input runtime loads"), RuntimeClass)) return false;
			Runtime = NewObject<UWuwaCombatInputRuntimeBridge>(Character, RuntimeClass);
			Config = NewObject<UWuwaInputCommandConfig>(Character);
			Character->InputCommandConfig = Config;
			return Test.TestNotNull(TEXT("Production input runtime exists"), Runtime)
				&& Test.TestEqual(TEXT("Input runtime uses the generated managed implementation"),
					Runtime->GetClass()->GetClass()->GetFName(), FName(TEXT("CSClass")));
		}

		/** SkillId > 0：按技能表的方式授予（带身份与分类 Tag）；0：普通能力，没有技能身份。 */
		FGameplayAbilitySpecHandle Grant(int32 SkillId = 0, FGameplayTagContainer Tags = {}) const
		{
			if (SkillId <= 0) return ASC->GiveAbility(FGameplayAbilitySpec(UWuwaSkillTestAbility::StaticClass(), 1));
			FWuwaSkillInfo Info;
			Info.Ability = UWuwaSkillTestAbility::StaticClass();
			Info.SkillTags = Tags;
			return ASC->GiveSkill(SkillId, Info, nullptr);
		}

		UWuwaGameplayAbilityBase* Active(FGameplayAbilitySpecHandle Handle) const
		{
			if (const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle))
				for (UGameplayAbility* Instance : Spec->GetAbilityInstances())
					if (auto* Ability = Cast<UWuwaGameplayAbilityBase>(Instance); IsValid(Ability) && Ability->IsActive()) return Ability;
			return nullptr;
		}

		UWuwaGameplayAbilityBase* Start(FAutomationTestBase& Test, FGameplayAbilitySpecHandle Handle) const
		{
			if (!Test.TestTrue(TEXT("Initial skill activates through real GAS"), ASC->TryActivateAbility(Handle, false))) return nullptr;
			UWuwaGameplayAbilityBase* Ability = Active(Handle);
			return Test.TestNotNull(TEXT("Initial skill retains an active execution"), Ability) ? Ability : nullptr;
		}

		bool EndCurrent(FAutomationTestBase& Test) const
		{
			const FWuwaSkillData Current = Skills->GetCurrentSkillData();
			return Test.TestNotNull(TEXT("There is a skill to end"), Current.ActiveAbility.Get())
				&& Test.TestTrue(TEXT("Current skill completes through production EndAbility"),
					Current.ActiveAbility->TryEndSkillExecution(Current.FightStateHandle));
		}

		FWuwaInputCommandRule Rule(FName Name, int32 Target) const
		{
			FWuwaInputCommandRule Result;
			Result.RuleName = Name;
			Result.InputTag = InputTag;
			Result.TargetSkillId = Target;
			return Result;
		}

		FWuwaInputEvent Input(FGameplayTag Tag, EWuwaInputPhase Phase = EWuwaInputPhase::Pressed) const
		{
			FWuwaInputEvent Result;
			Result.InputTag = Tag;
			Result.Phase = Phase;
			Result.Timestamp = World->GetTimeSeconds();
			return Result;
		}

		EWuwaCombatInputResult Press(float LifetimeSeconds = 1.f) const
		{
			return Runtime->ProcessInput(ASC, Input(InputTag), LifetimeSeconds);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaInputCommandOrderedConditionsTest,
	"Wuwa.Combat.InputCommand.OrderedOwnerQueryAndConditions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaInputCommandOrderedConditionsTest::RunTest(const FString& Parameters)
{
	using namespace WuwaInputCommandTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	const auto First = F.Grant(F.Skill1);
	const auto Second = F.Grant(F.Skill2);
	const auto Third = F.Grant(F.Skill3);
	auto* Pass = NewObject<UWuwaInputConditionTest>(F.Config);
	auto* Fail = NewObject<UWuwaInputConditionTest>(F.Config);
	auto* ContextCheck = NewObject<UWuwaInputConditionTest>(F.Config);
	Fail->bResult = false;
	ContextCheck->CheckContext = [&F](const FWuwaInputCommandContext& Context)
	{
		return Context.ASC == F.ASC && Context.Avatar == F.Character && Context.InputTag == F.InputTag
			&& Context.CurrentSkill.FightStateHandle == 0 && Context.CurrentSkill.ActiveAbility == nullptr;
	};
	FWuwaInputCommandRule Rejected = F.Rule(TEXT("FalseCondition"), F.Skill3);
	Rejected.OwnerTagQuery = FGameplayTagQuery::MakeQuery_MatchTag(F.OwnerTag);
	Rejected.Conditions = {Pass, Fail};
	FWuwaInputCommandRule Selected = F.Rule(TEXT("OwnerWithValidContext"), F.Skill2);
	Selected.OwnerTagQuery = FGameplayTagQuery::MakeQuery_MatchTag(F.OwnerTag);
	Selected.Conditions = {Pass, ContextCheck};
	F.Config->Rules = {Rejected, Selected, F.Rule(TEXT("Fallback"), F.Skill1)};
	TestTrue(TEXT("Empty-query fallback runs when conditional owner tag is absent"),
		F.Press() == EWuwaCombatInputResult::ActivationRequested);
	TestNotNull(TEXT("Fallback chooses the explicitly configured first attack"), F.Active(First));
	TestEqual(TEXT("False owner queries do not invoke the extension conditions"), Pass->EvaluationCount, 0);
	if (!F.EndCurrent(*this)) return false;
	F.ASC->AddLooseGameplayTag(F.OwnerTag);
	TestTrue(TEXT("The first fully matching rule activates"), F.Press() == EWuwaCombatInputResult::ActivationRequested);
	TestNotNull(TEXT("AND conditions reject the earlier rule and choose the second attack"), F.Active(Second));
	TestNull(TEXT("A single false extension condition blocks that rule"), F.Active(Third));
	TestNull(TEXT("A later fallback does not override the first matching rule"), F.Active(First));
	TestEqual(TEXT("Both eligible rules evaluate their first AND condition"), Pass->EvaluationCount, 2);
	TestEqual(TEXT("A false custom condition was actually evaluated through reflection"), Fail->EvaluationCount, 1);
	TestEqual(TEXT("The selected rule receives the real ASC/avatar/current-skill/input context"), ContextCheck->EvaluationCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaInputCommandCurrentSkillTest,
	"Wuwa.Combat.InputCommand.ComboUsesCurrentSkillIdAndResetsAfterEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaInputCommandCurrentSkillTest::RunTest(const FString& Parameters)
{
	using namespace WuwaInputCommandTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	const auto First = F.Grant(F.Skill1);
	const auto Second = F.Grant(F.Skill2);
	const auto Third = F.Grant(F.Skill3);
	FWuwaInputCommandRule OneToTwo = F.Rule(TEXT("Attack01To02"), F.Skill2);
	OneToTwo.RequiredCurrentSkillId = F.Skill1;
	FWuwaInputCommandRule TwoToThree = F.Rule(TEXT("Attack02To03"), F.Skill3);
	TwoToThree.RequiredCurrentSkillId = F.Skill2;
	F.Config->Rules = {OneToTwo, TwoToThree, F.Rule(TEXT("StartAttack01"), F.Skill1)};
	TestTrue(TEXT("Initial attack input selects the first stage"), F.Press() == EWuwaCombatInputResult::ActivationRequested);
	UWuwaGameplayAbilityBase* Old = F.Active(First);
	if (!TestNotNull(TEXT("First stage is active"), Old)) return false;
	// 三次授予故意共用同一个 GA 类：身份只来自授予时的技能 ID，不来自 GA 类
	TestEqual(TEXT("The executing ability reports the skill ID it was granted with"), Old->GetSkillId(), F.Skill1);
	TestEqual(TEXT("The current-skill record carries the skill ID"), F.Skills->GetCurrentSkillData().SkillId, F.Skill1);
	TestTrue(TEXT("Attack before the first stage's window is buffered"), F.Press() == EWuwaCombatInputResult::Buffered);
	F.Skills->SetSkillAcceptInput(Old->GetSkillHandle(), true);
	TestTrue(TEXT("The first stage's breakpoint selects the second stage"),
		F.Runtime->ProcessPendingInput(F.ASC) == EWuwaCombatInputResult::ActivationRequested);
	UWuwaGameplayAbilityBase* Next = F.Active(Second);
	if (!TestNotNull(TEXT("Second stage is active"), Next)) return false;
	TestFalse(TEXT("The first stage ended through normal skill handoff"), Old->IsSkillExecutionActive());
	TestEqual(TEXT("After handoff the current-skill record is the second stage"), F.Skills->GetCurrentSkillData().SkillId, F.Skill2);
	TestTrue(TEXT("A new raw attack waits behind the second stage"), F.Press() == EWuwaCombatInputResult::Buffered);
	if (!F.EndCurrent(*this)) return false;
	TestTrue(TEXT("At normal skill end the same raw input is resolved again"),
		F.Runtime->ProcessPendingInput(F.ASC) == EWuwaCombatInputResult::ActivationRequested);
	TestNotNull(TEXT("Without a current skill, the command starts attack01 again"), F.Active(First));
	TestNull(TEXT("No implicit continuation timer retains attack03 after attack02 ends"), F.Active(Third));
	TestEqual(TEXT("One input submission occurs at each of the three intended opportunities"), F.Activations, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaInputCommandBufferedReevaluationTest,
	"Wuwa.Combat.InputCommand.BufferedRawInputReevaluatesOwnerState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaInputCommandBufferedReevaluationTest::RunTest(const FString& Parameters)
{
	using namespace WuwaInputCommandTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	UWuwaGameplayAbilityBase* Old = F.Start(*this, F.Grant());
	if (!Old) return false;
	const auto First = F.Grant(F.Skill1);
	const auto Second = F.Grant(F.Skill2);
	FWuwaInputCommandRule Derived = F.Rule(TEXT("OwnerDerivedAttack"), F.Skill2);
	Derived.OwnerTagQuery = FGameplayTagQuery::MakeQuery_MatchTag(F.OwnerTag);
	F.Config->Rules = {Derived, F.Rule(TEXT("DefaultAttack"), F.Skill1)};
	TestTrue(TEXT("Closed-window input caches before the owner tag changes"), F.Press() == EWuwaCombatInputResult::Buffered);
	TestEqual(TEXT("One raw semantic input is cached"), F.Runtime->GetBufferedInputCount(), 1);
	F.ASC->AddLooseGameplayTag(F.OwnerTag);
	F.Runtime->ProcessPendingInput(F.ASC);
	TestEqual(TEXT("A condition change alone does not create an input opportunity"), F.Activations, 1);
	F.Skills->SetSkillAcceptInput(Old->GetSkillHandle(), true);
	TestTrue(TEXT("Breakpoint reevaluates the cached raw input against current owner tags"),
		F.Runtime->ProcessPendingInput(F.ASC) == EWuwaCombatInputResult::ActivationRequested);
	TestNotNull(TEXT("The command chooses the newly eligible derived attack"), F.Active(Second));
	TestNull(TEXT("The originally resolved default Spec was not frozen into the buffer"), F.Active(First));
	TestEqual(TEXT("Submitting the selected command consumes the raw batch"), F.Runtime->GetBufferedInputCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaInputCommandNoMatchBufferTest,
	"Wuwa.Combat.InputCommand.ConfiguredNoMatchBuffersButNeverBypassesRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaInputCommandNoMatchBufferTest::RunTest(const FString& Parameters)
{
	using namespace WuwaInputCommandTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	UWuwaGameplayAbilityBase* Old = F.Start(*this, F.Grant());
	if (!Old) return false;
	const auto Direct = F.Grant(F.DirectSkill);
	const auto Derived = F.Grant(F.Skill2);
	FWuwaInputCommandRule Conditional = F.Rule(TEXT("OnlyWhenOwnerTagPresent"), F.Skill2);
	Conditional.OwnerTagQuery = FGameplayTagQuery::MakeQuery_MatchTag(F.OwnerTag);
	F.Config->Rules = {Conditional};
	TestTrue(TEXT("Configured but unmatched input reports NoMatchingRule when caching is disabled"),
		F.Press(0.f) == EWuwaCombatInputResult::NoMatchingRule);
	TestEqual(TEXT("No-match with zero lifetime does not cache"), F.Runtime->GetBufferedInputCount(), 0);
	TestTrue(TEXT("A recognized input may cache before any rule becomes eligible"), F.Press() == EWuwaCombatInputResult::Buffered);
	TestEqual(TEXT("Recognized no-match retains the raw event"), F.Runtime->GetBufferedInputCount(), 1);
	TestTrue(TEXT("Completely unknown input reports no candidate"),
		F.Runtime->ProcessInput(F.ASC, F.Input(F.UnknownInput), 1.f) == EWuwaCombatInputResult::NoCandidate);
	TestEqual(TEXT("Unknown input does not enter the recognized pending batch"), F.Runtime->GetBufferedInputCount(), 1);
	for (const auto Phase : {EWuwaInputPhase::Released, EWuwaInputPhase::Triggered, EWuwaInputPhase::Canceled})
		TestTrue(TEXT("This stage ignores non-Pressed phases"),
			F.Runtime->ProcessInput(F.ASC, F.Input(F.InputTag, Phase), 1.f) == EWuwaCombatInputResult::Ignored);
	F.Skills->SetSkillAcceptInput(Old->GetSkillHandle(), true);
	F.Runtime->ProcessPendingInput(F.ASC);
	TestNull(TEXT("An open skill window cannot bypass unmatched configured conditions by direct-tag lookup"), F.Active(Direct));
	TestEqual(TEXT("No-match remains pending while its lifetime is valid"), F.Runtime->GetBufferedInputCount(), 1);
	F.ASC->AddLooseGameplayTag(F.OwnerTag);
	F.Skills->CallAnimBreakPoint(Old->GetSkillHandle());
	TestTrue(TEXT("A later explicit breakpoint can resolve the previously unmatched raw input"),
		F.Runtime->ProcessPendingInput(F.ASC) == EWuwaCombatInputResult::ActivationRequested);
	TestNotNull(TEXT("The newly eligible configured target activates"), F.Active(Derived));
	TestNull(TEXT("Direct-tag bypass remains unused"), F.Active(Direct));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaInputCommandFirstMatchFailureTest,
	"Wuwa.Combat.InputCommand.FirstMatchedTargetMissingDoesNotFallbackAndIdsAreUnique",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaInputCommandFirstMatchFailureTest::RunTest(const FString& Parameters)
{
	using namespace WuwaInputCommandTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	const auto Fallback = F.Grant(F.Skill1);
	const auto Direct = F.Grant(F.DirectSkill);
	F.Config->Rules = {F.Rule(TEXT("FirstMatchedTarget"), F.MissingSkill), F.Rule(TEXT("LaterFallback"), F.Skill1)};
	TestTrue(TEXT("First matched rule with an ungranted target reports NoCandidate"), F.Press() == EWuwaCombatInputResult::NoCandidate);
	TestEqual(TEXT("Missing selected target does not execute another rule or the old direct route"), F.Activations, 0);
	TestEqual(TEXT("Missing target is not misreported as condition-no-match input"), F.Runtime->GetBufferedInputCount(), 0);
	const auto Chosen = F.Grant(F.MissingSkill);
	TestFalse(TEXT("A skill ID can be granted only once per ASC (identity is unique)"), F.Grant(F.MissingSkill).IsValid());
	TestTrue(TEXT("Once the selected target is granted, it can activate"), F.Press() == EWuwaCombatInputResult::ActivationRequested);
	TestNotNull(TEXT("The first matching target is the executed Spec"), F.Active(Chosen));
	TestNull(TEXT("Later fallback remains inactive"), F.Active(Fallback));
	TestNull(TEXT("Same-input-tag direct grant remains inactive"), F.Active(Direct));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaInputCommandNoDirectRouteTest,
	"Wuwa.Combat.InputCommand.UnconfiguredInputHasNoDirectRoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaInputCommandNoDirectRouteTest::RunTest(const FString& Parameters)
{
	using namespace WuwaInputCommandTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	// 技能身份是 ID：输入 Tag 不再兜底当作技能身份，每个输入都要在规则表里有一行
	const FGameplayTag DashTag = FWuwaGameTags::Get().Input_Combat_Dash;
	const auto Dash = F.Grant(F.DirectSkill);
	F.Config->Rules = {F.Rule(TEXT("ConfiguredAttack"), F.Skill2)};
	TestTrue(TEXT("An input without any rule reports NoCandidate"),
		F.Runtime->ProcessInput(F.ASC, F.Input(DashTag), 1.f) == EWuwaCombatInputResult::NoCandidate);
	TestNull(TEXT("No skill is activated by input tag alone"), F.Active(Dash));
	TestEqual(TEXT("An unconfigured input is not buffered"), F.Runtime->GetBufferedInputCount(), 0);
	F.Character->InputCommandConfig = nullptr;
	TestTrue(TEXT("A character without a command config cannot select any skill"),
		F.Press(1.f) == EWuwaCombatInputResult::NoCandidate);
	TestEqual(TEXT("Nothing was activated"), F.Activations, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaInputCommandCurrentSkillTagQueryTest,
	"Wuwa.Combat.InputCommand.CurrentSkillTagQueryMatchesSkillClassification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaInputCommandCurrentSkillTagQueryTest::RunTest(const FString& Parameters)
{
	using namespace WuwaInputCommandTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	// Skill1 属于"普攻"类，Skill3 不属于；"当前是普攻 -> Skill2"这一行只在当前技能是普攻类时命中
	const auto First = F.Grant(F.Skill1, FGameplayTagContainer(F.NormalAttack));
	const auto Second = F.Grant(F.Skill2);
	const auto Third = F.Grant(F.Skill3);
	TestTrue(TEXT("Skill tags are recorded at grant time"), F.ASC->GetSkillTags(F.Skill1).HasTagExact(F.NormalAttack));
	FWuwaInputCommandRule AfterNormal = F.Rule(TEXT("AfterAnyNormalAttack"), F.Skill2);
	AfterNormal.CurrentSkillTagQuery = FGameplayTagQuery::MakeQuery_MatchTag(F.NormalAttack);
	F.Config->Rules = {AfterNormal, F.Rule(TEXT("Default"), F.Skill1)};
	TestTrue(TEXT("Without a current skill the class query does not match"), F.Press() == EWuwaCombatInputResult::ActivationRequested);
	UWuwaGameplayAbilityBase* Old = F.Active(First);
	if (!TestNotNull(TEXT("Default rule started the normal attack"), Old)) return false;
	F.Skills->SetSkillAcceptInput(Old->GetSkillHandle(), true);
	TestTrue(TEXT("During a normal attack the class rule matches"), F.Press() == EWuwaCombatInputResult::ActivationRequested);
	TestNotNull(TEXT("The class rule selected its target"), F.Active(Second));
	if (!F.EndCurrent(*this)) return false;
	UWuwaGameplayAbilityBase* Other = F.Start(*this, Third);
	if (!Other) return false;
	F.Skills->SetSkillAcceptInput(Other->GetSkillHandle(), true);
	TestTrue(TEXT("A current skill outside the class falls to the next rule"), F.Press() == EWuwaCombatInputResult::ActivationRequested);
	TestNotNull(TEXT("The default rule applies when the current skill is not a normal attack"), F.Active(First));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaInputCommandInvalidRuleTest,
	"Wuwa.Combat.InputCommand.InvalidRuleDoesNotFallThrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaInputCommandInvalidRuleTest::RunTest(const FString& Parameters)
{
	using namespace WuwaInputCommandTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	F.Grant(F.Skill1);
	F.Config->Rules = {F.Rule(TEXT("MissingTargetIdentity"), 0), F.Rule(TEXT("Fallback"), F.Skill1)};
	TestTrue(TEXT("An empty selected target identity is a configuration error"), F.Press() == EWuwaCombatInputResult::InvalidRule);
	TestEqual(TEXT("Empty selected target cannot run the later fallback"), F.Activations, 0);
	FWuwaInputCommandRule MissingCondition = F.Rule(TEXT("NullConditionObject"), F.Skill1);
	MissingCondition.Conditions.Add(nullptr);
	F.Config->Rules = {MissingCondition, F.Rule(TEXT("Fallback"), F.Skill1)};
	TestTrue(TEXT("A null condition object is not interpreted as a false or absent condition"),
		F.Press() == EWuwaCombatInputResult::InvalidRule);
	TestEqual(TEXT("Invalid condition cannot run the later fallback"), F.Activations, 0);
	TestEqual(TEXT("Invalid configuration does not enter the no-match buffer"), F.Runtime->GetBufferedInputCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaInputCommandConditionReentryTest,
	"Wuwa.Combat.InputCommand.ConditionCallbacksCannotSubmitStaleContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaInputCommandConditionReentryTest::RunTest(const FString& Parameters)
{
	using namespace WuwaInputCommandTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	const auto Target = F.Grant(F.Skill2);
	auto* Condition = NewObject<UWuwaInputConditionTest>(F.Config);
	FWuwaInputCommandRule Rule = F.Rule(TEXT("InstrumentedCondition"), F.Skill2);
	Rule.Conditions = {Condition};
	F.Config->Rules = {Rule};
	// These callbacks intentionally violate the production pure-query contract to
	// verify that an extension mistake cannot submit an already invalidated command.
	Condition->CheckContext = [&F](const FWuwaInputCommandContext&)
	{
		F.Runtime->ResetInput();
		return true;
	};
	TestTrue(TEXT("Resetting input during a condition invalidates the immediate selection"), F.Press() == EWuwaCombatInputResult::Ignored);
	TestEqual(TEXT("A reset callback cannot activate from the old input context"), F.Activations, 0);
	bool bReentryIgnored = false;
	Condition->CheckContext = [&F, &bReentryIgnored](const FWuwaInputCommandContext&)
	{
		bReentryIgnored = F.Press() == EWuwaCombatInputResult::Ignored;
		return true;
	};
	TestTrue(TEXT("Outer selection remains valid when recursive processing is refused"), F.Press() == EWuwaCombatInputResult::ActivationRequested);
	TestTrue(TEXT("The same runtime cannot recursively process input from a condition"), bReentryIgnored);
	TestEqual(TEXT("Recursive callbacks still produce exactly one activation"), F.Activations, 1);
	TestNotNull(TEXT("The outer selected target is active"), F.Active(Target));
	if (!F.EndCurrent(*this)) return false;
	UWuwaGameplayAbilityBase* Old = F.Start(*this, F.Grant());
	if (!Old) return false;
	Condition->CheckContext = nullptr;
	TestTrue(TEXT("A valid pure condition may buffer behind a closed skill window"), F.Press() == EWuwaCombatInputResult::Buffered);
	F.Skills->SetSkillAcceptInput(Old->GetSkillHandle(), true);
	Condition->CheckContext = [&F](const FWuwaInputCommandContext&)
	{
		F.Runtime->ClearBufferedInput(F.InputTag);
		return true;
	};
	TestTrue(TEXT("Clearing pending input inside a condition invalidates its pending snapshot"),
		F.Runtime->ProcessPendingInput(F.ASC) == EWuwaCombatInputResult::Ignored);
	TestEqual(TEXT("Cleared pending snapshot cannot cause a replacement"), F.Activations, 2);
	TestEqual(TEXT("The explicit clear remains effective after the callback"), F.Runtime->GetBufferedInputCount(), 0);
	const int32 OldHandle = Old->GetSkillHandle();
	Condition->CheckContext = [Old, OldHandle](const FWuwaInputCommandContext&)
	{
		Old->TryEndSkillExecution(OldHandle);
		return true;
	};
	TestTrue(TEXT("Ending the current skill inside a condition invalidates that context"), F.Press() == EWuwaCombatInputResult::Ignored);
	TestEqual(TEXT("The stale selection cannot activate after its source skill ended"), F.Activations, 2);
	TestEqual(TEXT("The source skill's own cleanup still completes"), F.Skills->GetCurrentSkillData().FightStateHandle, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaInputCommandManagedAttributeConditionTest,
	"Wuwa.Combat.InputCommand.ManagedAttributeConditionUsesCurrentASCValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaInputCommandManagedAttributeConditionTest::RunTest(const FString& Parameters)
{
	using namespace WuwaInputCommandTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	UClass* ConditionClass = LoadClass<UWuwaInputCondition>(nullptr,
		TEXT("/Script/UnrealSharp.InputCondition_AttributeMinimum_C"));
	if (!TestNotNull(TEXT("The real generated attribute condition class loads"), ConditionClass)) return false;
	TestEqual(TEXT("Attribute condition uses managed implementation"), ConditionClass->GetClass()->GetFName(), FName(TEXT("CSClass")));
	UWuwaInputCondition* Condition = NewObject<UWuwaInputCondition>(F.Config, ConditionClass);
	FStructProperty* AttributeProperty = FindFProperty<FStructProperty>(ConditionClass, TEXT("Attribute"));
	FFloatProperty* MinimumProperty = FindFProperty<FFloatProperty>(ConditionClass, TEXT("Minimum"));
	if (!TestNotNull(TEXT("Managed condition exposes the configured attribute"), AttributeProperty)
		|| !TestNotNull(TEXT("Managed condition exposes the configured minimum"), MinimumProperty)) return false;
	FGameplayAttribute* Attribute = AttributeProperty->ContainerPtrToValuePtr<FGameplayAttribute>(Condition);
	*Attribute = UWuwaAttributeSet::GetHealthAttribute();
	MinimumProperty->SetPropertyValue_InContainer(Condition, 50.f);
	FObjectPropertyBase* ASCProperty = FindFProperty<FObjectPropertyBase>(AWuwaCharactorBase::StaticClass(), TEXT("AbilitySystemComponent"));
	if (!TestNotNull(TEXT("Character exposes its native ASC owner property"), ASCProperty)) return false;
	ASCProperty->SetObjectPropertyValue_InContainer(F.Character, F.ASC);
	UWuwaAttributeSet* Attributes = NewObject<UWuwaAttributeSet>(F.Character);
	Attributes->InitHealth(40.f);
	Attributes->InitMaxHealth(100.f);
	F.ASC->AddAttributeSetSubobject(Attributes);
	const auto First = F.Grant(F.Skill1);
	const auto Second = F.Grant(F.Skill2);
	FWuwaInputCommandRule Conditional = F.Rule(TEXT("HealthAtLeast50"), F.Skill2);
	Conditional.Conditions = {Condition};
	F.Config->Rules = {Conditional, F.Rule(TEXT("Fallback"), F.Skill1)};
	TestTrue(TEXT("Below the attribute threshold, the fallback is selected"), F.Press() == EWuwaCombatInputResult::ActivationRequested);
	TestNotNull(TEXT("The fallback skill is active below the threshold"), F.Active(First));
	TestNull(TEXT("The conditional skill is inactive below the threshold"), F.Active(Second));
	if (!F.EndCurrent(*this)) return false;
	F.ASC->SetNumericAttributeBase(UWuwaAttributeSet::GetHealthAttribute(), 50.f);
	TestTrue(TEXT("At the exact threshold, the managed condition selects its target"), F.Press() == EWuwaCombatInputResult::ActivationRequested);
	TestNotNull(TEXT("The same shared condition reads the new current ASC value"), F.Active(Second));
	if (!F.EndCurrent(*this)) return false;
	F.Config->Rules = {Conditional};
	*Attribute = FGameplayAttribute();
	TestTrue(TEXT("An invalid attribute never passes the managed condition"), F.Press(0.f) == EWuwaCombatInputResult::NoMatchingRule);
	TestEqual(TEXT("Invalid attribute configuration does not start another skill"), F.Activations, 2);
	return true;
}

#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaInputCommandPhaseTest,
	"Wuwa.Combat.InputCommand.PhaseSelectsPressedHeldReleasedRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaInputCommandPhaseTest::RunTest(const FString& Parameters)
{
	using namespace WuwaInputCommandTests;
	FEditorScriptExecutionGuard Guard;
	FFixture F;
	if (!F.Initialize(*this)) return false;
	const auto Tap = F.Grant(F.Skill1);
	const auto Heavy = F.Grant(F.Skill2);
	const auto Release = F.Grant(F.Skill3);
	// 同一个键：按下 = 普攻、长按 = 重击；先只配这两行
	FWuwaInputCommandRule Pressed = F.Rule(TEXT("Tap"), F.Skill1);
	FWuwaInputCommandRule Held = F.Rule(TEXT("Hold"), F.Skill2);
	Held.Phase = EWuwaInputPhase::Held;
	F.Config->Rules = {Held, Pressed};
	TestTrue(TEXT("A press skips the Held row even though it comes first"), F.Press() == EWuwaCombatInputResult::ActivationRequested);
	TestNotNull(TEXT("The press starts the Pressed row's skill"), F.Active(Tap));
	TestNull(TEXT("The Held row's skill does not start on a press"), F.Active(Heavy));
	if (!F.EndCurrent(*this)) return false;
	TestTrue(TEXT("Reaching the hold threshold selects the Held row"),
		F.Runtime->ProcessInput(F.ASC, F.Input(F.InputTag, EWuwaInputPhase::Held), 1.f) == EWuwaCombatInputResult::ActivationRequested);
	TestNotNull(TEXT("The hold starts the Held row's skill"), F.Active(Heavy));
	if (!F.EndCurrent(*this)) return false;
	TestTrue(TEXT("A release with no Released row is ignored, as before"),
		F.Runtime->ProcessInput(F.ASC, F.Input(F.InputTag, EWuwaInputPhase::Released), 1.f) == EWuwaCombatInputResult::Ignored);
	FWuwaInputCommandRule Released = F.Rule(TEXT("Release"), F.Skill3);
	Released.Phase = EWuwaInputPhase::Released;
	F.Config->Rules = {Held, Pressed, Released};
	TestTrue(TEXT("A release selects the Released row"),
		F.Runtime->ProcessInput(F.ASC, F.Input(F.InputTag, EWuwaInputPhase::Released), 1.f) == EWuwaCombatInputResult::ActivationRequested);
	TestNotNull(TEXT("The release starts the Released row's skill"), F.Active(Release));
	TestEqual(TEXT("Nothing is left in the pre-input buffer"), F.Runtime->GetBufferedInputCount(), 0);
	return true;
}
