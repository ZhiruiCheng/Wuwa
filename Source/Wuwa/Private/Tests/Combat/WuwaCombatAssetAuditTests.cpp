#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Animation/AnimMontage.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "K2Node_BaseAsyncTask.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_SkillReadyEndBridge.h"
#include "Game/Animation/Notifies/WuwaAnimNotifyState_SkillAcceptInputBridge.h"
#include "Core/Tag/WuwaGameTags.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Input/DataAsset/WuwaInputDataAsset.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputCommandConfig.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"
#include "Engine/DataTable.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

namespace WuwaCombatAssetAuditTests
{
	TSharedRef<FJsonObject> AuditMontage(const TCHAR* Path)
	{
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("path"), Path);
		const UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, Path);
		Result->SetBoolField(TEXT("loaded"), IsValid(Montage));
		bool bHasAcceptInput = false;
		bool bHasReadyEnd = false;
		TArray<TSharedPtr<FJsonValue>> Events;
		if (Montage)
		{
			Result->SetNumberField(TEXT("lengthSeconds"), Montage->GetPlayLength());
			Result->SetNumberField(TEXT("rateScale"), Montage->RateScale);
			Result->SetNumberField(TEXT("blendOutTriggerTime"), Montage->BlendOutTriggerTime);
			Result->SetNumberField(TEXT("blendInSeconds"), Montage->GetDefaultBlendInTime());
			Result->SetNumberField(TEXT("blendOutSeconds"), Montage->GetDefaultBlendOutTime());
			Result->SetBoolField(TEXT("enableAutoBlendOut"), Montage->bEnableAutoBlendOut);
			TArray<TSharedPtr<FJsonValue>> Sections;
			for (const FCompositeSection& Section : Montage->CompositeSections)
			{
				TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
				Row->SetStringField(TEXT("name"), Section.SectionName.ToString());
				Row->SetStringField(TEXT("nextSection"), Section.NextSectionName.ToString());
				Row->SetNumberField(TEXT("timeSeconds"), Section.GetTime());
				Sections.Add(MakeShared<FJsonValueObject>(Row));
			}
			Result->SetArrayField(TEXT("sections"), Sections);
			TArray<TSharedPtr<FJsonValue>> Segments;
			for (const FSlotAnimationTrack& Slot : Montage->SlotAnimTracks)
			{
				for (const FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
				{
					TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
					Row->SetStringField(TEXT("slotName"), Slot.SlotName.ToString());
					Row->SetStringField(TEXT("animation"), GetPathNameSafe(Segment.GetAnimReference().Get()));
					Row->SetNumberField(TEXT("montageStartSeconds"), Segment.StartPos);
					Row->SetNumberField(TEXT("animationStartSeconds"), Segment.AnimStartTime);
					Row->SetNumberField(TEXT("animationEndSeconds"), Segment.AnimEndTime);
					Row->SetNumberField(TEXT("animationPlayRate"), Segment.AnimPlayRate);
					Row->SetNumberField(TEXT("loopCount"), Segment.LoopingCount);
					Segments.Add(MakeShared<FJsonValueObject>(Row));
				}
			}
			Result->SetArrayField(TEXT("segments"), Segments);
			for (const FAnimNotifyEvent& Event : Montage->Notifies)
			{
				const UObject* Notify = Event.NotifyStateClass ? static_cast<const UObject*>(Event.NotifyStateClass.Get())
					: static_cast<const UObject*>(Event.Notify.Get());
				TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
				Row->SetStringField(TEXT("name"), Event.NotifyName.ToString());
				Row->SetStringField(TEXT("class"), Notify ? Notify->GetClass()->GetPathName() : TEXT(""));
				Row->SetNumberField(TEXT("timeSeconds"), Event.GetTime());
				Row->SetNumberField(TEXT("durationSeconds"), Event.GetDuration());
				Row->SetNumberField(TEXT("triggerTimeSeconds"), Event.GetTriggerTime());
				Row->SetNumberField(TEXT("endTriggerTimeSeconds"), Event.GetEndTriggerTime());
				Row->SetStringField(TEXT("montageTickType"), Event.MontageTickType == EMontageNotifyTickType::BranchingPoint
					? TEXT("BranchingPoint") : TEXT("Queued"));
				Row->SetNumberField(TEXT("triggerWeightThreshold"), Event.TriggerWeightThreshold);
				Row->SetNumberField(TEXT("notifyTriggerChance"), Event.NotifyTriggerChance);
				Row->SetNumberField(TEXT("notifyFilterType"), static_cast<int32>(Event.NotifyFilterType.GetValue()));
				Row->SetBoolField(TEXT("isNotifyState"), Event.NotifyStateClass != nullptr);
				Events.Add(MakeShared<FJsonValueObject>(Row));
				bHasAcceptInput |= Notify && Notify->IsA(UWuwaAnimNotifyState_SkillAcceptInputBridge::StaticClass());
				bHasReadyEnd |= Notify && Notify->IsA(UWuwaAnimNotify_SkillReadyEndBridge::StaticClass());
			}
		}
		Result->SetArrayField(TEXT("notifies"), Events);
		Result->SetBoolField(TEXT("hasSkillAcceptInputWindow"), bHasAcceptInput);
		Result->SetBoolField(TEXT("hasSkillReadyEndNotify"), bHasReadyEnd);
		return Result;
	}

	TSharedRef<FJsonObject> AuditAbilityGraphs(UClass* AbilityClass)
	{
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("class"), GetPathNameSafe(AbilityClass));
		const UWuwaGameplayAbilityBase* Ability = AbilityClass ? Cast<UWuwaGameplayAbilityBase>(AbilityClass->GetDefaultObject()) : nullptr;
		Result->SetBoolField(TEXT("isMainSkill"), Ability && Ability->bIsMainSkill);
		Result->SetNumberField(TEXT("interruptLevel"), Ability ? Ability->InterruptLevel : -1);
		Result->SetNumberField(TEXT("instancingPolicy"), Ability ? static_cast<int32>(Ability->GetInstancingPolicy()) : -1);
		TSharedRef<FJsonObject> DefaultProperties = MakeShared<FJsonObject>();
		if (Ability)
		{
			const TSet<FName> RequestedProperties = {
				TEXT("AbilityTags"), TEXT("ActivationRequiredTags"), TEXT("ActivationBlockedTags"),
				TEXT("ActivationOwnedTags"), TEXT("BlockAbilitiesWithTag"), TEXT("CancelAbilitiesWithTag"),
				TEXT("SourceRequiredTags"), TEXT("SourceBlockedTags"), TEXT("TargetRequiredTags"), TEXT("TargetBlockedTags"),
				TEXT("CostGameplayEffectClass"), TEXT("CooldownGameplayEffectClass"), TEXT("bRetriggerInstancedAbility")
			};
			for (TFieldIterator<FProperty> It(AbilityClass, EFieldIteratorFlags::IncludeSuper); It; ++It)
			{
				const FString Name = It->GetName();
				if (!RequestedProperties.Contains(It->GetFName()) && !Name.Contains(TEXT("Montage"))
					&& !Name.Contains(TEXT("Rate")) && !Name.Contains(TEXT("Section"))) continue;
				FString Value;
				It->ExportText_InContainer(0, Value, Ability, nullptr, const_cast<UWuwaGameplayAbilityBase*>(Ability), PPF_None);
				DefaultProperties->SetStringField(Name, Value);
			}
		}
		Result->SetObjectField(TEXT("defaultProperties"), DefaultProperties);
		TArray<TSharedPtr<FJsonValue>> GraphRows;
		int32 EndAbilityCalls = 0;
		int32 OnEndAbilityCalls = 0;
		int32 OnEndAbilityEvents = 0;
		int32 MontageTasks = 0;
		// Include authored parent graphs (e.g. Dash's Blueprint base), without compiling,
		// reconstructing nodes, changing pins, or instantiating the gameplay abilities.
		for (UClass* Class = AbilityClass; Class; Class = Class->GetSuperClass())
		{
			UBlueprint* Blueprint = Cast<UBlueprint>(Class->ClassGeneratedBy);
			if (!Blueprint) continue;
			TArray<UEdGraph*> Graphs;
			Blueprint->GetAllGraphs(Graphs);
			for (const UEdGraph* Graph : Graphs)
			{
				if (!Graph) continue;
				TSharedRef<FJsonObject> GraphRow = MakeShared<FJsonObject>();
				GraphRow->SetStringField(TEXT("blueprint"), Blueprint->GetPathName());
				GraphRow->SetStringField(TEXT("graph"), Graph->GetPathName());
				TArray<TSharedPtr<FJsonValue>> NodeRows;
				for (const UEdGraphNode* Node : Graph->Nodes)
				{
					if (!Node) continue;
					TSharedRef<FJsonObject> NodeRow = MakeShared<FJsonObject>();
					NodeRow->SetStringField(TEXT("name"), Node->GetName());
					NodeRow->SetStringField(TEXT("class"), Node->GetClass()->GetPathName());
					NodeRow->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
					NodeRow->SetStringField(TEXT("guid"), Node->NodeGuid.ToString());
					if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
					{
						const FName Function = Call->FunctionReference.GetMemberName();
						NodeRow->SetStringField(TEXT("functionName"), Function.ToString());
						NodeRow->SetStringField(TEXT("functionParent"), GetPathNameSafe(Call->FunctionReference.GetMemberParentClass()));
						EndAbilityCalls += Function == TEXT("K2_EndAbility");
						OnEndAbilityCalls += Function == TEXT("K2_OnEndAbility");
					}
					if (const UK2Node_Event* Event = Cast<UK2Node_Event>(Node))
					{
						const FName Function = Event->EventReference.GetMemberName();
						NodeRow->SetStringField(TEXT("eventName"), Function.ToString());
						NodeRow->SetStringField(TEXT("eventParent"), GetPathNameSafe(Event->EventReference.GetMemberParentClass()));
						OnEndAbilityEvents += Function == TEXT("K2_OnEndAbility");
					}
					if (const UK2Node_BaseAsyncTask* Async = Cast<UK2Node_BaseAsyncTask>(Node))
					{
						const UFunction* Factory = Async->GetFactoryFunction();
						NodeRow->SetStringField(TEXT("asyncFactoryFunction"), GetPathNameSafe(Factory));
						MontageTasks += Factory && Factory->GetFName() == TEXT("CreatePlayMontageAndWaitProxy");
					}
					TArray<TSharedPtr<FJsonValue>> Pins;
					for (const UEdGraphPin* Pin : Node->Pins)
					{
						if (!Pin) continue;
						TSharedRef<FJsonObject> PinRow = MakeShared<FJsonObject>();
						PinRow->SetStringField(TEXT("name"), Pin->PinName.ToString());
						PinRow->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
						PinRow->SetStringField(TEXT("category"), Pin->PinType.PinCategory.ToString());
						PinRow->SetStringField(TEXT("defaultValue"), Pin->DefaultValue);
						PinRow->SetStringField(TEXT("autogeneratedDefaultValue"), Pin->AutogeneratedDefaultValue);
						PinRow->SetStringField(TEXT("defaultObject"), GetPathNameSafe(Pin->DefaultObject.Get()));
						PinRow->SetStringField(TEXT("defaultText"), Pin->DefaultTextValue.ToString());
						TArray<TSharedPtr<FJsonValue>> Links;
						for (const UEdGraphPin* Linked : Pin->LinkedTo)
						{
							if (!Linked) continue;
							TSharedRef<FJsonObject> Link = MakeShared<FJsonObject>();
							Link->SetStringField(TEXT("node"), Linked->GetOwningNode()->GetPathName());
							Link->SetStringField(TEXT("pin"), Linked->PinName.ToString());
							Links.Add(MakeShared<FJsonValueObject>(Link));
						}
						PinRow->SetArrayField(TEXT("links"), Links);
						Pins.Add(MakeShared<FJsonValueObject>(PinRow));
					}
					NodeRow->SetArrayField(TEXT("pins"), Pins);
					NodeRows.Add(MakeShared<FJsonValueObject>(NodeRow));
				}
				GraphRow->SetArrayField(TEXT("nodes"), NodeRows);
				GraphRows.Add(MakeShared<FJsonValueObject>(GraphRow));
			}
		}
		Result->SetArrayField(TEXT("graphs"), GraphRows);
		Result->SetNumberField(TEXT("endAbilityCallNodes"), EndAbilityCalls);
		Result->SetNumberField(TEXT("onEndAbilityCallNodes"), OnEndAbilityCalls);
		Result->SetNumberField(TEXT("onEndAbilityEventNodes"), OnEndAbilityEvents);
		Result->SetNumberField(TEXT("playMontageAndWaitNodes"), MontageTasks);
		Result->SetStringField(TEXT("scope"), TEXT("Authored graph structure and pin defaults/links, including Blueprint parents. Counts do not prove reachable runtime execution."));
		return Result;
	}

	TSharedRef<FJsonObject> AuditAbility(UClass* AbilityClass, const AWuwaCharacter* Character,
		const UWuwaInputDataAsset* Bindings, const UInputMappingContext* MappingContext,
		const UInputAction* Action, FGameplayTag ExpectedTag, const UWuwaAbilitySystemComponent* GrantedASC)
	{
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		const UGameplayAbility* Ability = AbilityClass ? AbilityClass->GetDefaultObject<UGameplayAbility>() : nullptr;
		const UWuwaGameplayAbilityBase* WuwaAbility = Cast<UWuwaGameplayAbilityBase>(Ability);
		Result->SetBoolField(TEXT("classLoaded"), IsValid(AbilityClass));
		Result->SetStringField(TEXT("class"), GetPathNameSafe(AbilityClass));
		Result->SetStringField(TEXT("parentClass"), AbilityClass ? GetPathNameSafe(AbilityClass->GetSuperClass()) : TEXT(""));
		Result->SetBoolField(TEXT("inheritsWuwaGameplayAbilityBase"), WuwaAbility != nullptr);
		// 技能身份：角色技能表里 Ability 为该类的行（行名 = 技能 ID）
		int32 SkillId = 0;
		int32 SkillRows = 0;
		if (Character && Character->SkillTable && AbilityClass)
			Character->SkillTable->ForeachRow<FWuwaSkillInfo>(TEXT("CombatAssetAudit"), [&](const FName& RowName, const FWuwaSkillInfo& Info)
			{
				if (Info.Ability.Get() != AbilityClass) return;
				++SkillRows;
				SkillId = WuwaSkill::ParseSkillId(RowName);
			});
		Result->SetNumberField(TEXT("skillId"), SkillId);
		Result->SetNumberField(TEXT("skillTableRows"), SkillRows);
		Result->SetStringField(TEXT("expectedInputTag"), ExpectedTag.ToString());
		Result->SetBoolField(TEXT("isMainSkill"), WuwaAbility && WuwaAbility->bIsMainSkill);
		Result->SetNumberField(TEXT("interruptLevel"), WuwaAbility ? WuwaAbility->InterruptLevel : -1);
		Result->SetNumberField(TEXT("instancingPolicy"), Ability ? static_cast<int32>(Ability->GetInstancingPolicy()) : -1);
		Result->SetBoolField(TEXT("supportsSameSpecReplacement"), WuwaAbility
			&& WuwaAbility->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerExecution);
		Result->SetBoolField(TEXT("listedInSkillTable"), SkillRows == 1 && SkillId > 0);
		// 按键规则表里有没有一行"这个输入 -> 这个技能 ID"
		int32 CommandRules = 0;
		if (Character && Character->InputCommandConfig && SkillId > 0)
			for (const FWuwaInputCommandRule& Rule : Character->InputCommandConfig->Rules)
				CommandRules += Rule.InputTag == ExpectedTag && Rule.TargetSkillId == SkillId;
		Result->SetNumberField(TEXT("commandRulesTargetingSkill"), CommandRules);
		int32 GrantedCount = 0;
		if (GrantedASC && AbilityClass) for (const FGameplayAbilitySpec& Spec : GrantedASC->GetActivatableAbilities())
			GrantedCount += Spec.Ability && Spec.Ability->GetClass() == AbilityClass;
		Result->SetNumberField(TEXT("grantedByInitInitialAbilities"), GrantedCount);

		TArray<TSharedPtr<FJsonValue>> Rows;
		int32 MatchingBindingCount = 0;
		if (Bindings && Action) for (const FInputDataAsset& Binding : Bindings->InputDataAssetMap)
		{
			if (Binding.InputAction != Action) continue;
			TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("inputTag"), Binding.InputTag.ToString());
			Row->SetStringField(TEXT("routeTag"), Binding.RouteTag.ToString());
			Rows.Add(MakeShared<FJsonValueObject>(Row));
			MatchingBindingCount += Binding.IsConfigured() && Binding.InputTag == ExpectedTag
				&& Binding.RouteTag == FWuwaGameTags::Get().Input_Route_Ability;
		}
		Result->SetArrayField(TEXT("semanticBindings"), Rows);
		Result->SetNumberField(TEXT("matchingSemanticBindingCount"), MatchingBindingCount);
		TArray<TSharedPtr<FJsonValue>> Keys;
		if (MappingContext && Action) for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
			if (Mapping.Action == Action) Keys.Add(MakeShared<FJsonValueString>(Mapping.Key.ToString()));
		Result->SetArrayField(TEXT("mappedKeys"), Keys);
		Result->SetBoolField(TEXT("inputActionLoaded"), IsValid(Action));
		Result->SetBoolField(TEXT("inputAndGrantReady"), WuwaAbility && ExpectedTag.IsValid() && SkillRows == 1 && SkillId > 0
			&& CommandRules > 0 && GrantedCount == 1 && MatchingBindingCount == 1 && Keys.Num() > 0);
		Result->SetStringField(TEXT("inputReadinessScope"), TEXT("Key -> semantic input -> at least one InputCommandConfig rule targeting the skill ID -> granted once from the skill table. Rule conditions are not evaluated."));
		// This audit never activates a skill or advances its animation. Avoid reporting
		// static configuration as proof that graph wiring, montage references or combat work.
		Result->SetBoolField(TEXT("abilityExecutionVerified"), false);
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCombatAssetAuditTest, "Wuwa.Combat.Assets.ReadOnlyReadinessAudit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCombatAssetAuditTest::RunTest(const FString& Parameters)
{
	using namespace WuwaCombatAssetAuditTests;
	FEditorScriptExecutionGuard Guard;
	UClass* DashClass = LoadClass<UGameplayAbility>(nullptr,
		TEXT("/Game/Characters/Role/changli/GA/GA_Dash.GA_Dash_C"));
	UClass* AttackClass = LoadClass<UGameplayAbility>(nullptr,
		TEXT("/Game/Characters/Role/changli/GA/GA_Attack1.GA_Attack1_C"));
	UClass* CharacterClass = LoadClass<AWuwaCharacter>(nullptr,
		TEXT("/Game/Characters/Role/changli/BP_Role_Changli.BP_Role_Changli_C"));
	UClass* ControllerClass = LoadClass<AWuwaPlayerController>(nullptr,
		TEXT("/Game/Core/BP_WuwaPlayerController.BP_WuwaPlayerController_C"));
	const AWuwaCharacter* CharacterDefaults = CharacterClass ? CharacterClass->GetDefaultObject<AWuwaCharacter>() : nullptr;
	const AWuwaPlayerController* ControllerDefaults = ControllerClass ? ControllerClass->GetDefaultObject<AWuwaPlayerController>() : nullptr;
	const UWuwaInputDataAsset* Bindings = LoadObject<UWuwaInputDataAsset>(nullptr,
		TEXT("/Game/CoreInput/DataAsset/DA_IARouterMap.DA_IARouterMap"));
	const UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(nullptr,
		TEXT("/Game/CoreInput/Contexts/IMC_Character.IMC_Character"));
	const UInputAction* DodgeAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/CoreInput/Actions/IA_Dodge.IA_Dodge"));
	const UInputAction* AttackAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/CoreInput/Actions/IA_Attack.IA_Attack"));

	TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("auditUtc"), FDateTime::UtcNow().ToIso8601());
	Report->SetStringField(TEXT("scope"), TEXT("Read-only asset/configuration audit; no asset saves or ability activation."));
	Report->SetBoolField(TEXT("characterLoaded"), IsValid(CharacterDefaults));
	Report->SetBoolField(TEXT("controllerUsesInspectedBindings"), ControllerDefaults && ControllerDefaults->InputTagMap == Bindings && Bindings);
	Report->SetBoolField(TEXT("mappingContextLoaded"), IsValid(MappingContext));
	TArray<TSharedPtr<FJsonValue>> ConfiguredClasses;
	bool bNullAbilityEntry = false;
	if (CharacterDefaults) for (const auto& AbilityClass : CharacterDefaults->CharacterAbilities)
	{
		ConfiguredClasses.Add(MakeShared<FJsonValueString>(GetPathNameSafe(AbilityClass.Get())));
		bNullAbilityEntry |= !AbilityClass;
	}
	Report->SetArrayField(TEXT("characterAbilityClasses"), ConfiguredClasses);
	Report->SetStringField(TEXT("skillTable"), CharacterDefaults ? GetPathNameSafe(CharacterDefaults->SkillTable.Get()) : TEXT(""));
	Report->SetBoolField(TEXT("hasNullAbilityEntry"), bNullAbilityEntry);

	// Reproduce the production grant function in a transient native actor using the
	// actual authored class array. Never instantiate the asset's construction script.
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	UWuwaAbilitySystemComponent* ASC = nullptr;
	bool bCalledProductionGrant = false;
	if (World && CharacterDefaults && !bNullAbilityEntry)
	{
		FActorSpawnParameters Spawn;
		Spawn.ObjectFlags |= RF_Transient;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AWuwaCharacter* Character = World->SpawnActor<AWuwaCharacter>(Spawn);
		if (Character)
		{
			ASC = NewObject<UWuwaAbilitySystemComponent>(Character);
			ASC->RegisterComponent();
			ASC->InitAbilityActorInfo(Character, Character);
			FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(AWuwaCharactorBase::StaticClass(), TEXT("AbilitySystemComponent"));
			if (Property)
			{
				Property->SetObjectPropertyValue_InContainer(Character, ASC);
				Character->SkillTable = CharacterDefaults->SkillTable;
				Character->CharacterAbilities = CharacterDefaults->CharacterAbilities;
				Character->InitInitialAbilities();
				bCalledProductionGrant = true;
			}
		}
	}
	Report->SetBoolField(TEXT("productionGrantFixtureAvailable"), bCalledProductionGrant);
	const FGameplayTag DashTag = FWuwaGameTags::Get().Input_Combat_Dash;
	const FGameplayTag AttackTag = FWuwaGameTags::Get().Input_Combat_Attack;
	TSharedRef<FJsonObject> Dash = AuditAbility(DashClass, CharacterDefaults, Bindings, MappingContext, DodgeAction, DashTag, ASC);
	TSharedRef<FJsonObject> Attack = AuditAbility(AttackClass, CharacterDefaults, Bindings, MappingContext, AttackAction, AttackTag, ASC);
	Report->SetObjectField(TEXT("dash"), Dash);
	Report->SetObjectField(TEXT("attack"), Attack);
	Report->SetObjectField(TEXT("dashForwardMontage"), AuditMontage(
		TEXT("/Game/Characters/Role/changli/AnimMontage/AM_Changli_Dash_F.AM_Changli_Dash_F")));
	Report->SetObjectField(TEXT("dashBackwardMontage"), AuditMontage(
		TEXT("/Game/Characters/Role/changli/AnimMontage/AM_Changli_Dash_B.AM_Changli_Dash_B")));
	Report->SetObjectField(TEXT("availableAttackMontage"), AuditMontage(
		TEXT("/Game/Characters/Role/changli/AnimMontage/AM_Attack01.AM_Attack01")));
	Report->SetBoolField(TEXT("attackMontageReferenceInAbilityVerified"), false);
	Report->SetStringField(TEXT("attackScopeNote"), TEXT("Attack readiness is static configuration only, not gameplay. Montage availability does not prove the GA references or plays it."));

	TestTrue(TEXT("Existing Dash uses the production gameplay ability base"), Dash->GetBoolField(TEXT("inheritsWuwaGameplayAbilityBase")));
	TestTrue(TEXT("Existing Dash supports per-execution replacement"), Dash->GetBoolField(TEXT("supportsSameSpecReplacement")));
	TestTrue(TEXT("Existing Dash has exactly one skill ID in the role skill table"), Dash->GetBoolField(TEXT("listedInSkillTable")));
	TestTrue(TEXT("The role command table routes the dodge input to the Dash skill ID"), Dash->GetNumberField(TEXT("commandRulesTargetingSkill")) > 0);
	TestEqual(TEXT("Production initial grant grants Dash exactly once"), static_cast<int32>(Dash->GetNumberField(TEXT("grantedByInitInitialAbilities"))), 1);
	TestTrue(TEXT("Existing Dash keeps a valid mapped and routed semantic input"), Dash->GetBoolField(TEXT("inputAndGrantReady")));
	TestTrue(TEXT("The real controller uses the inspected semantic binding asset"), Report->GetBoolField(TEXT("controllerUsesInspectedBindings")));

	const FString ReportDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Diagnostics/CombatStage4"));
	IFileManager::Get().MakeDirectory(*ReportDirectory, true);
	const FString ReportPath = ReportDirectory / TEXT("AssetAudit.json");
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	const bool bWritten = FJsonSerializer::Serialize(Report, Writer) && FFileHelper::SaveStringToFile(Json, *ReportPath);
	TestTrue(TEXT("Read-only asset audit writes its factual JSON report"), bWritten);
	AddInfo(FString::Printf(TEXT("Asset audit: %s; Attack input/rule/grant ready=%s (rule conditions and gameplay not exercised)."),
		*ReportPath, Attack->GetBoolField(TEXT("inputAndGrantReady")) ? TEXT("true") : TEXT("false")));

	TSharedRef<FJsonObject> GraphReport = MakeShared<FJsonObject>();
	GraphReport->SetStringField(TEXT("auditUtc"), FDateTime::UtcNow().ToIso8601());
	GraphReport->SetStringField(TEXT("scope"), TEXT("Read-only current attack/Dash graph and montage audit; no assets saved and no abilities activated."));
	TArray<TSharedPtr<FJsonValue>> AttackGraphs;
	for (int32 Index = 1; Index <= 5; ++Index)
	{
		const FString ClassPath = FString::Printf(TEXT("/Game/Characters/Role/changli/GA/GA_Attack%d.GA_Attack%d_C"), Index, Index);
		const FString MontagePath = FString::Printf(TEXT("/Game/Characters/Role/changli/AnimMontage/AM_Attack%02d.AM_Attack%02d"), Index, Index);
		TSharedRef<FJsonObject> Entry = AuditAbilityGraphs(LoadClass<UGameplayAbility>(nullptr, *ClassPath));
		Entry->SetStringField(TEXT("requestedClass"), ClassPath);
		Entry->SetObjectField(TEXT("montage"), AuditMontage(*MontagePath));
		AttackGraphs.Add(MakeShared<FJsonValueObject>(Entry));
	}
	GraphReport->SetArrayField(TEXT("attacks"), AttackGraphs);
	GraphReport->SetObjectField(TEXT("dash"), AuditAbilityGraphs(DashClass));
	GraphReport->SetObjectField(TEXT("dashForwardMontage"), AuditMontage(
		TEXT("/Game/Characters/Role/changli/AnimMontage/AM_Changli_Dash_F.AM_Changli_Dash_F")));
	GraphReport->SetObjectField(TEXT("dashBackwardMontage"), AuditMontage(
		TEXT("/Game/Characters/Role/changli/AnimMontage/AM_Changli_Dash_B.AM_Changli_Dash_B")));
	const FString GraphDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Diagnostics/AttackBuffer"));
	const FString GraphPath = GraphDirectory / TEXT("GraphAudit.json");
	FString GraphJson;
	const bool bGraphWritten = IFileManager::Get().MakeDirectory(*GraphDirectory, true)
		&& FJsonSerializer::Serialize(GraphReport, TJsonWriterFactory<>::Create(&GraphJson))
		&& FFileHelper::SaveStringToFile(GraphJson, *GraphPath);
	TestTrue(TEXT("Read-only graph audit writes all five current attack graphs and montages"), bGraphWritten);
	AddInfo(FString::Printf(TEXT("Attack/Dash graph and montage audit: %s"), *GraphPath));
	if (ASC) ASC->ClearActorInfo();
	if (World) World->DestroyWorld(false);
	return true;
}

#endif
