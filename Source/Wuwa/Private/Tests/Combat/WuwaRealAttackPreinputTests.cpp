#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/Input/WuwaTestMoveInput.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "InputAction.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_SkillReadyEndBridge.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_SpawnBullet.h"
#include "Game/NewWorld/Bullet/WuwaBulletSubsystem.h"
#include "Game/NewWorld/Combat/WuwaLockOnComponent.h"
#include "Game/Common/WuwaGameTags.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Input/WuwaInputRouterComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateBridgeComponent.h"
#include "Game/Animation/Notifies/WuwaAnimNotifyState_AddLooseTag.h"
#include "Game/NewWorld/Character/Common/Component/Combat/WuwaFightStateBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Input/UWuwaCombatInputRuntimeBridge.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaAbilityInputHandlerComponent.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputCommandConfig.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"
#include "Engine/DataTable.h"
#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "WuwaSkillNotifyTestTypes.h"

namespace WuwaRealAttackPreinputTests
{
	/** 角色资源：默认长离；嘉贝莉娜用 Jiabei()。AttackAbilities 按连段顺序，额外的 GA（如普攻中冲刺）放在最后。 */
	struct FRole
	{
		FString CharacterClass = TEXT("/Game/Characters/Role/changli/BP_Role_Changli.BP_Role_Changli_C");
		FString Mesh = TEXT("/Game/Characters/Role/changli/NewModel/changli2_0.changli2_0");
		FString AnimationClass = TEXT("/Game/Characters/Role/changli/ABP_Changli.ABP_Changli_C");
		FString InputCommands = TEXT("/Game/Characters/Role/changli/Input/DA_Changli_InputCommands.DA_Changli_InputCommands");
		TArray<TPair<FString, FString>> AttackAbilities;
		FString DashAbility = TEXT("/Game/Characters/Role/changli/GA/GA_Dash.GA_Dash_C");
		FString DashBackward = TEXT("/Game/Characters/Role/changli/AnimMontage/AM_Changli_Dash_B.AM_Changli_Dash_B");

		FRole()
		{
			for (int32 Index = 1; Index <= 5; ++Index)
			{
				AttackAbilities.Emplace(FString::Printf(TEXT("/Game/Characters/Role/changli/GA/GA_Attack%d.GA_Attack%d_C"), Index, Index),
					FString::Printf(TEXT("/Game/Characters/Role/changli/AnimMontage/AM_Attack%02d.AM_Attack%02d"), Index, Index));
			}
		}

		static FRole Jiabei()
		{
			FRole Role;
			Role.CharacterClass = TEXT("/Game/Characters/Role/jiabei/BP_Role_jiabei.BP_Role_jiabei_C");
			Role.Mesh = TEXT("/Game/Characters/Role/jiabei/Mesh/jiabei2_0.jiabei2_0");
			Role.AnimationClass = TEXT("/Game/Characters/Role/jiabei/ABP_Role_Jiabei.ABP_Role_Jiabei_C");
			Role.InputCommands = TEXT("/Game/Characters/Role/jiabei/Input/DA_Jiabei_InputCommands.DA_Jiabei_InputCommands");
			Role.AttackAbilities.Reset();
			for (int32 Index = 1; Index <= 4; ++Index)
			{
				Role.AttackAbilities.Emplace(FString::Printf(TEXT("/Game/Characters/Role/jiabei/GA/GA_Attack%d.GA_Attack%d_C"), Index, Index),
					FString::Printf(TEXT("/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_Attack%02d.AM_Jiabei_Attack%02d"), Index, Index));
			}
			Role.AttackAbilities.Emplace(TEXT("/Game/Characters/Role/jiabei/GA/GA_AttackDash.GA_AttackDash_C"),
				TEXT("/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_Charge.AM_Jiabei_Charge"));
			Role.DashAbility = TEXT("/Game/Characters/Role/jiabei/GA/GA_Dash.GA_Dash_C");
			Role.DashBackward = TEXT("/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_Dash_B.AM_Jiabei_Dash_B");
			return Role;
		}
	};

	struct FFixture
	{
		FAutomationTestBase& Test;
		FString Scenario;
		FRole Role;
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AWuwaCharacter* Character = nullptr;
		AWuwaPlayerController* Controller = nullptr;
		UWuwaAbilitySystemComponent* ASC = nullptr;
		UWuwaAbilityInputHandlerComponent* Handler = nullptr;
		UWuwaCombatInputRuntimeBridge* Runtime = nullptr;
		UAnimInstance* Anim = nullptr;
		UInputAction* AttackAction = nullptr;
		TArray<UClass*> AttackClasses;
		TArray<UAnimMontage*> AttackMontages;
		UClass* DashClass = nullptr;
		/** 角色蓝图上配置的真实技能表；技能按它授予（技能 ID = 行名）。 */
		UDataTable* SkillTable = nullptr;
		UAnimMontage* DashBackward = nullptr;
		UAnimMontage* SourceMontage = nullptr;
		int32 SourcePlaybackId = INDEX_NONE;
		int32 SourceSkillHandle = 0;
		int32 Activations = 0;
		int32 AbilityEnds = 0;
		float FrameStepSeconds = 1.f / 120.f;
		bool bObservedSourceReadyEnd = false;
		bool bUseRealAnimGraph = false;
		FDelegateHandle ActivationDelegate;
		FDelegateHandle EndDelegate;
		TArray<TSharedPtr<FJsonValue>> Frames;
		TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();

		FFixture(FAutomationTestBase& InTest, const TCHAR* InScenario, bool bInUseRealAnimGraph = false, FRole InRole = FRole())
			: Test(InTest), Scenario(InScenario), Role(MoveTemp(InRole)), bUseRealAnimGraph(bInUseRealAnimGraph)
		{
			Report->SetStringField(TEXT("scenario"), Scenario);
			Report->SetStringField(TEXT("auditUtc"), FDateTime::UtcNow().ToIso8601());
			Report->SetStringField(TEXT("scope"), TEXT("Real authored GA/command config/montage/notifies; transient native character and real input handler. No asset or CDO modification. Animation and input advance explicitly, without unrelated world systems."));
			Report->SetBoolField(TEXT("realAnimationGraph"), bUseRealAnimGraph);
		}

		~FFixture()
		{
			Capture(TEXT("FinalBeforeCleanup"));
			Report->SetBoolField(TEXT("observedSourceReadyEnd"), bObservedSourceReadyEnd);
			Report->SetNumberField(TEXT("activationCount"), Activations);
			Report->SetNumberField(TEXT("abilityEndCount"), AbilityEnds);
			Report->SetNumberField(TEXT("frameStepSeconds"), FrameStepSeconds);
			Report->SetArrayField(TEXT("frames"), Frames);
			const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Diagnostics/AttackPreinputRepro"));
			IFileManager::Get().MakeDirectory(*Directory, true);
			const FString Path = Directory / (Scenario + TEXT(".json"));
			FString Json;
			const auto Writer = TJsonWriterFactory<>::Create(&Json);
			Test.TestTrue(TEXT("Real-asset reproduction writes its report even after a failed assertion"),
				FJsonSerializer::Serialize(Report, Writer) && FFileHelper::SaveStringToFile(Json, *Path));
			Test.AddInfo(FString::Printf(TEXT("Real-asset report: %s; activations=%d; source ReadyEnd observed=%s"),
				*Path, Activations, bObservedSourceReadyEnd ? TEXT("true") : TEXT("false")));
			if (Controller) Controller->SetPawn(nullptr);
			if (ASC)
			{
				ASC->AbilityActivatedCallbacks.Remove(ActivationDelegate);
				ASC->AbilityEndedCallbacks.Remove(EndDelegate);
				ASC->CancelAllAbilities();
			}
			if (Anim) Anim->UninitializeAnimation();
			if (ASC) ASC->ClearActorInfo();
			if (World) World->DestroyWorld(false);
		}

		void Capture(const TCHAR* Stage, const FString& InputResult = FString(), UGameplayAbility* Incoming = nullptr)
		{
			TSharedRef<FJsonObject> Frame = MakeShared<FJsonObject>();
			Frame->SetStringField(TEXT("stage"), Stage);
			Frame->SetNumberField(TEXT("worldSeconds"), World ? World->GetTimeSeconds() : -1.0);
			Frame->SetNumberField(TEXT("activations"), Activations);
			Frame->SetNumberField(TEXT("abilityEnds"), AbilityEnds);
			Frame->SetStringField(TEXT("inputResult"), InputResult);
			Frame->SetStringField(TEXT("incomingAbilityClass"), Incoming ? Incoming->GetClass()->GetPathName() : TEXT(""));
			Frame->SetNumberField(TEXT("bufferCount"), Runtime ? Runtime->GetBufferedInputCount() : -1);
			if (ASC)
			{
				Frame->SetStringField(TEXT("ascMontage"), GetPathNameSafe(ASC->GetCurrentMontage()));
				Frame->SetStringField(TEXT("ascAnimatingAbility"), GetPathNameSafe(ASC->GetAnimatingAbility()));
			}
			if (Character && Character->SkillComponent)
			{
				const FWuwaPlayerInputState InputState = Character->GetPlayerInputState();
				Frame->SetBoolField(TEXT("hasMoveInput"), InputState.bHasMoveInput);
				Frame->SetNumberField(TEXT("moveInputX"), InputState.MoveAxis.X);
				Frame->SetNumberField(TEXT("moveInputY"), InputState.MoveAxis.Y);
				const FWuwaSkillData Skill = Character->SkillComponent->GetCurrentSkillData();
				Frame->SetNumberField(TEXT("skillHandle"), Skill.FightStateHandle);
				Frame->SetNumberField(TEXT("interruptLevel"), Skill.InterruptLevel);
				Frame->SetNumberField(TEXT("opportunitySerial"), static_cast<double>(Skill.InputOpportunitySerial));
				Frame->SetNumberField(TEXT("skillStartSerial"), static_cast<double>(Skill.SkillStartSerial));
				Frame->SetBoolField(TEXT("acceptInput"), Skill.bSkillAcceptInput);
				Frame->SetBoolField(TEXT("readyEnd"), Skill.bMainSkillReadyEnd);
				Frame->SetStringField(TEXT("skillAbility"), GetPathNameSafe(Skill.ActiveAbility));
				Frame->SetStringField(TEXT("skillAbilityClass"), Skill.ActiveAbility ? Skill.ActiveAbility->GetClass()->GetPathName() : TEXT(""));
				Frame->SetBoolField(TEXT("canEndNow"), IsValid(Skill.ActiveAbility) && Skill.ActiveAbility->CanEndSkillExecutionNow());
				Frame->SetStringField(TEXT("abilityCurrentMontage"), IsValid(Skill.ActiveAbility) ? GetPathNameSafe(Skill.ActiveAbility->GetCurrentMontage()) : TEXT(""));
				if (Skill.FightStateHandle == SourceSkillHandle && SourceSkillHandle > 0 && Skill.bMainSkillReadyEnd)
					bObservedSourceReadyEnd = true;
				if (Character->FightStateComponent)
					Frame->SetNumberField(TEXT("fightHandle"), Character->FightStateComponent->GetStateData().Handle);
			}
			if (Anim && SourceMontage)
			{
				const FAnimMontageInstance* Source = Anim->GetMontageInstanceForID(SourcePlaybackId);
				const FAnimMontageInstance* Active = Anim->GetActiveInstanceForMontage(SourceMontage);
				Frame->SetNumberField(TEXT("sourcePlaybackId"), SourcePlaybackId);
				Frame->SetBoolField(TEXT("sourceInstanceExists"), Source != nullptr);
				Frame->SetBoolField(TEXT("sourceIsCurrentActiveInstance"), Source && Source == Active);
				Frame->SetNumberField(TEXT("sourcePosition"), Source ? Source->GetPosition() : -1.f);
				Frame->SetNumberField(TEXT("sourceWeight"), Source ? Source->GetWeight() : 0.f);
				Frame->SetNumberField(TEXT("sourcePlayRate"), Source ? Source->GetPlayRate() : 0.f);
				Frame->SetBoolField(TEXT("sourceStopped"), Source && Source->IsStopped());
			}
			Frames.Add(MakeShared<FJsonValueObject>(Frame));
		}

		bool Initialize()
		{
			if (!Test.TestNotNull(TEXT("Transient game world exists"), World)) return false;
			World->TimeSeconds = 1.0;
			FActorSpawnParameters Spawn;
			Spawn.ObjectFlags |= RF_Transient;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			UClass* CharacterClass = bUseRealAnimGraph
				? LoadClass<AWuwaCharacter>(nullptr, *Role.CharacterClass)
				: AWuwaCharacter::StaticClass();
			if (!Test.TestNotNull(TEXT("Character class loads"), CharacterClass)) return false;
			Character = World->SpawnActor<AWuwaCharacter>(CharacterClass, Spawn);
			if (!Test.TestNotNull(TEXT("Transient character exists"), Character)) return false;
			Character->GetWuwaMovementComponent()->MovementMode = MOVE_Walking;
			if (!Test.TestTrue(TEXT("Real movement state assembles"), Character->EnsureMovementStateSystem())
				|| !Test.TestTrue(TEXT("Real skill state assembles"), Character->EnsureSkillSystem())) return false;
			Character->RoleGaitComponent->RefreshPolicy();
			USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *Role.Mesh);
			if (!Test.TestNotNull(TEXT("Real role mesh loads"), Mesh)) return false;
			Character->GetMesh()->SetSkeletalMeshAsset(Mesh);
			UClass* AnimationClass = bUseRealAnimGraph
				? LoadClass<UAnimInstance>(nullptr, *Role.AnimationClass)
				: UWuwaSkillNotifyTestAnimInstance::StaticClass();
			if (!Test.TestNotNull(TEXT("Animation class loads"), AnimationClass)) return false;
			Character->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			Character->GetMesh()->SetAnimInstanceClass(AnimationClass);
			Character->GetMesh()->InitAnim(true);
			Anim = Character->GetMesh()->GetAnimInstance();
			if (!Test.TestNotNull(TEXT("Fixture animation instance exists"), Anim)) return false;
			Report->SetStringField(TEXT("characterClass"), CharacterClass->GetPathName());
			Report->SetStringField(TEXT("animationClass"), AnimationClass->GetPathName());
			Character->InputCommandConfig = LoadObject<UWuwaInputCommandConfig>(nullptr, *Role.InputCommands);
			if (!Test.TestNotNull(TEXT("Real authored input command table loads"), Character->InputCommandConfig.Get())) return false;
			ASC = NewObject<UWuwaAbilitySystemComponent>(Character);
			ASC->RegisterComponent();
			ASC->InitAbilityActorInfo(Character, Character);
			FObjectPropertyBase* ASCProperty = FindFProperty<FObjectPropertyBase>(AWuwaCharactorBase::StaticClass(), TEXT("AbilitySystemComponent"));
			if (!Test.TestNotNull(TEXT("Native character ASC property exists"), ASCProperty)) return false;
			ASCProperty->SetObjectPropertyValue_InContainer(Character, ASC);
			// Match the project's native player AttributeSet defaults, so an authored
			// GA cost/query is not rejected merely because the fixture lacks attributes.
			ASC->AddAttributeSetSubobject(NewObject<UWuwaAttributeSet>(Character));
			// 不论夹具用不用真实角色蓝图，技能 ID 都取自角色蓝图上配置的技能表
			if (UClass* RoleClass = LoadClass<AWuwaCharacter>(nullptr, *Role.CharacterClass))
				SkillTable = RoleClass->GetDefaultObject<AWuwaCharacter>()->SkillTable;
			if (!Test.TestNotNull(TEXT("Role skill table is authored on the character Blueprint"), SkillTable)) return false;
			for (const TPair<FString, FString>& Pair : Role.AttackAbilities)
			{
				const FString& AbilityPath = Pair.Key;
				const FString& MontagePath = Pair.Value;
				UClass* Class = LoadClass<UWuwaGameplayAbilityBase>(nullptr, *AbilityPath);
				UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
				if (!Test.TestNotNull(*AbilityPath, Class) || !Test.TestNotNull(*MontagePath, Montage)) return false;
				AttackClasses.Add(Class);
				AttackMontages.Add(Montage);
				if (!Grant(Class)) return false;
			}
			DashClass = LoadClass<UWuwaGameplayAbilityBase>(nullptr, *Role.DashAbility);
			DashBackward = LoadObject<UAnimMontage>(nullptr, *Role.DashBackward);
			if (!Test.TestNotNull(TEXT("Real Dash GA loads"), DashClass)
				|| !Test.TestNotNull(TEXT("Real backward Dash montage loads"), DashBackward)) return false;
			if (!Grant(DashClass)) return false;
			ActivationDelegate = ASC->AbilityActivatedCallbacks.AddLambda([this](UGameplayAbility* Ability)
			{
				++Activations;
				Capture(TEXT("AbilityActivatedBeforeSkillRegistration"), FString(), Ability);
			});
			EndDelegate = ASC->AbilityEndedCallbacks.AddLambda([this](UGameplayAbility* Ability)
			{
				++AbilityEnds;
				Capture(TEXT("AbilityEndedBeforeSkillCleanup"), FString(), Ability);
			});
			Controller = World->SpawnActor<AWuwaPlayerController>(Spawn);
			if (!Test.TestNotNull(TEXT("Real native input controller exists"), Controller)) return false;
			Controller->SetAsLocalPlayerController();
			ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
			LocalPlayer->PlayerController = Controller;
			Controller->Player = LocalPlayer;
			Controller->SetPawn(Character);
			Character->SetController(Controller);
			ASC->InitAbilityActorInfo(Character, Character);
			Controller->AscComponent = ASC;
			Controller->RegisterInputRouteHandlers();
			Handler = Controller->AbilityInputHandler;
			AttackAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/CoreInput/Actions/IA_Attack.IA_Attack"));
			if (!Test.TestNotNull(TEXT("Production ability handler exists"), Handler)
				|| !Test.TestNotNull(TEXT("Real attack input action loads"), AttackAction)) return false;
			Report->SetNumberField(TEXT("defaultBufferLifetimeSeconds"), Handler->DefaultBufferLifetimeSeconds);
			// Preserve the actual controller Blueprint's configured buffer lifetime in
			// this native controller fixture without running unrelated camera systems.
			if (UClass* ControllerClass = LoadClass<AWuwaPlayerController>(nullptr, TEXT("/Game/Core/BP_WuwaPlayerController.BP_WuwaPlayerController_C")))
			{
				if (const UWuwaAbilityInputHandlerComponent* AuthoredHandler = ControllerClass->GetDefaultObject<AWuwaPlayerController>()->AbilityInputHandler)
				{
					Handler->DefaultBufferLifetimeSeconds = AuthoredHandler->DefaultBufferLifetimeSeconds;
					Handler->BufferLifetimeOverrides = AuthoredHandler->BufferLifetimeOverrides;
					Report->SetNumberField(TEXT("authoredControllerBufferLifetimeSeconds"), Handler->DefaultBufferLifetimeSeconds);
				}
			}
			Capture(TEXT("Initialized"));
			return true;
		}

		/** 在技能表里找到这个 GA 的行，按该行的技能 ID 授予。 */
		bool Grant(UClass* Class) const
		{
			bool bGranted = false;
			SkillTable->ForeachRow<FWuwaSkillInfo>(TEXT("RealAttackPreinputGrant"), [&](const FName& RowName, const FWuwaSkillInfo& Info)
			{
				if (!bGranted && Info.Ability.Get() == Class)
					bGranted = ASC->GiveSkill(WuwaSkill::ParseSkillId(RowName), Info, nullptr).IsValid();
			});
			return Test.TestTrue(*FString::Printf(TEXT("%s is granted from the role skill table"), *GetNameSafe(Class)), bGranted);
		}

		FWuwaInputEvent Input(FGameplayTag Tag) const
		{
			FWuwaInputEvent Event;
			Event.InputTag = Tag;
			Event.RouteTag = FWuwaGameTags::Get().Input_Route_Ability;
			Event.Phase = EWuwaInputPhase::Pressed;
			Event.Timestamp = World->GetTimeSeconds();
			Event.SourceAction = AttackAction;
			return Event;
		}

		bool Start(FGameplayTag Tag, UClass* ExpectedClass, UAnimMontage* ExpectedMontage, bool bDirectASCStart = false)
		{
			FWuwaInputEvent InitialInput = Input(Tag);
			// A release establishes the real handler's runtime/subscriptions without
			// selecting Attack01, allowing a middle combo stage to be isolated below.
			if (bDirectASCStart) InitialInput.Phase = EWuwaInputPhase::Released;
			bool bRouted = Controller->GetInputRouter()->DispatchInput(InitialInput);
			if (bDirectASCStart)
			{
				FGameplayAbilitySpecHandle Handle;
				for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
					if (Spec.Ability && Spec.Ability->GetClass() == ExpectedClass) Handle = Spec.Handle;
				bRouted = bRouted && Handle.IsValid() && ASC->TryActivateAbility(Handle, false);
			}
			FObjectPropertyBase* RuntimeProperty = FindFProperty<FObjectPropertyBase>(UWuwaAbilityInputHandlerComponent::StaticClass(), TEXT("Runtime"));
			Runtime = RuntimeProperty ? Cast<UWuwaCombatInputRuntimeBridge>(RuntimeProperty->GetObjectPropertyValue_InContainer(Handler)) : nullptr;
			Capture(bDirectASCStart ? TEXT("StartMiddleStageThroughASC") : TEXT("StartThroughProductionRoute"),
				bRouted ? TEXT("RequestAccepted") : TEXT("RequestRejected"));
			const FWuwaSkillData Current = Character->SkillComponent->GetCurrentSkillData();
			if (!Test.TestTrue(TEXT("Initial input reaches production handler"), bRouted)
				|| !Test.TestNotNull(TEXT("Production handler created its managed runtime"), Runtime)
				|| !Test.TestNotNull(TEXT("Initial real GA retained a skill execution"), Current.ActiveAbility.Get())) return false;
			SourceMontage = ExpectedMontage;
			SourceSkillHandle = Current.FightStateHandle;
			FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(SourceMontage);
			if (Instance) SourcePlaybackId = Instance->GetInstanceID();
			Capture(TEXT("StartedSourcePlayback"));
			return Test.TestEqual(TEXT("Authored input rules select the expected real GA"), Current.ActiveAbility->GetClass(), ExpectedClass)
				&& Test.TestEqual(TEXT("Real GA task registers its montage with ASC"), ASC->GetCurrentMontage(), ExpectedMontage)
				&& Test.TestEqual(TEXT("Real GA task owns ASC animation"), ASC->GetAnimatingAbility(), static_cast<UGameplayAbility*>(Current.ActiveAbility.Get()))
				&& Test.TestNotNull(TEXT("Real GA starts an actual playback instance"), Instance);
		}

		void HoldMovement()
		{
			// This is the public input entry that populates PlayerInputState; the
			// animation notification observes exactly the same semantic move intent.
			WuwaTestInput::SetMoveAxis(Character, FVector2D(0.f, 1.f));
			Character->GetWuwaMovementComponent()->ConsumeInputVector();
			Test.TestTrue(TEXT("Public movement input is visible through PlayerInputState"), Character->GetPlayerInputState().bHasMoveInput);
			Capture(TEXT("MovementKeyHeld"));
		}

		void Advance(float DeltaSeconds)
		{
			World->TimeSeconds += DeltaSeconds;
			if (bUseRealAnimGraph)
			{
				Character->GetMesh()->TickAnimation(DeltaSeconds, true);
				Character->GetMesh()->RefreshBoneTransforms(nullptr);
			}
			else
			{
				CastChecked<UWuwaSkillNotifyTestAnimInstance>(Anim)->AdvanceNotifyTimeline(DeltaSeconds);
			}
			Capture(TEXT("AfterAnimation"));
			Handler->TickComponent(DeltaSeconds, LEVELTICK_All, &Handler->PrimaryComponentTick);
			Capture(TEXT("AfterInputTick"));
		}

		/** 按真实播放速率推进源蒙太奇，直到它的播放位置到达 MontagePosition（蒙太奇时间）。 */
		void AdvanceSourceTo(float MontagePosition)
		{
			for (int32 Frame = 0; Frame < 4800; ++Frame)
			{
				const FAnimMontageInstance* Instance = Anim->GetMontageInstanceForID(SourcePlaybackId);
				if (!Instance || Instance->IsStopped() || Instance->GetPosition() >= MontagePosition) break;
				Advance(FrameStepSeconds);
			}
		}

		/** 源蒙太奇上连段窗口（Add Loose Tag Window）的结束时间；没有返回 -1。 */
		float ComboWindowEnd() const
		{
			for (const FAnimNotifyEvent& Event : SourceMontage->Notifies)
			{
				if (Cast<UWuwaAnimNotifyState_AddLooseTag>(Event.NotifyStateClass)) return Event.GetTime() + Event.GetDuration();
			}
			return -1.f;
		}

		float ReadyEndTime() const
		{
			for (const FAnimNotifyEvent& Event : SourceMontage->Notifies)
			{
				if (Event.Notify && Event.Notify->IsA(UWuwaAnimNotify_SkillReadyEndBridge::StaticClass())) return Event.GetTime();
			}
			return -1.f;
		}

		/** 直接把一次按下交给生产环境的输入运行时，返回选招结果。 */
		EWuwaCombatInputResult Press(FGameplayTag Tag)
		{
			const EWuwaCombatInputResult Result = Runtime->ProcessInput(ASC, Input(Tag), Handler->DefaultBufferLifetimeSeconds);
			Capture(*FString::Printf(TEXT("Press %s"), *Tag.ToString()), UEnum::GetValueAsString(Result));
			for (int32 Frame = 0; Frame < 3; ++Frame) Advance(FrameStepSeconds);
			return Result;
		}

		bool ReplayAttackAtReadyEnd(UClass* ExpectedNext)
		{
			float ReadyTime = -1.f;
			TArray<TSharedPtr<FJsonValue>> NotifyRows;
			for (const FAnimNotifyEvent& Event : SourceMontage->Notifies)
			{
				TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
				const UObject* Notify = Event.Notify ? static_cast<const UObject*>(Event.Notify.Get()) : Event.NotifyStateClass.Get();
				Row->SetStringField(TEXT("class"), Notify ? Notify->GetClass()->GetPathName() : TEXT(""));
				Row->SetNumberField(TEXT("time"), Event.GetTime());
				Row->SetNumberField(TEXT("duration"), Event.GetDuration());
				Row->SetNumberField(TEXT("tickType"), Event.MontageTickType.GetValue());
				NotifyRows.Add(MakeShared<FJsonValueObject>(Row));
				if (Event.Notify && Event.Notify->IsA(UWuwaAnimNotify_SkillReadyEndBridge::StaticClass())
					&& (ReadyTime < 0.f || Event.GetTime() < ReadyTime)) ReadyTime = Event.GetTime();
			}
			Report->SetArrayField(TEXT("sourceNotifies"), NotifyRows);
			Report->SetStringField(TEXT("sourceMontage"), SourceMontage->GetPathName());
			Report->SetNumberField(TEXT("sourceLength"), SourceMontage->GetPlayLength());
			Report->SetNumberField(TEXT("sourceRateScale"), SourceMontage->RateScale);
			Report->SetNumberField(TEXT("sourceBlendOutTime"), SourceMontage->BlendOut.GetBlendTime());
			Report->SetNumberField(TEXT("sourceBlendOutTriggerTime"), SourceMontage->BlendOutTriggerTime);
			Report->SetNumberField(TEXT("readyEndAssetTime"), ReadyTime);
			if (!Test.TestTrue(TEXT("Original montage contains a real managed ReadyEnd notify"), ReadyTime >= 0.f)) return false;
			FAnimMontageInstance* Instance = Anim->GetMontageInstanceForID(SourcePlaybackId);
			if (!Test.TestNotNull(TEXT("Source playback is still present before advancing"), Instance)) return false;
			const float EffectiveRate = FMath::Abs(Instance->GetPlayRate() * SourceMontage->RateScale);
			if (!Test.TestTrue(TEXT("Original source playback has positive effective speed"), EffectiveRate > 0.f)) return false;
			const float BufferPosition = FMath::Max(0.f, ReadyTime - 0.1f * EffectiveRate);
			Report->SetNumberField(TEXT("bufferInjectionAssetPosition"), BufferPosition);
			for (int32 Frame = 0; Frame < 2400; ++Frame)
			{
				Instance = Anim->GetMontageInstanceForID(SourcePlaybackId);
				if (!Instance || Instance->IsStopped() || Instance->GetPosition() >= BufferPosition - KINDA_SMALL_NUMBER) break;
				const float Delta = FMath::Min(FrameStepSeconds, (BufferPosition - Instance->GetPosition()) / EffectiveRate);
				Advance(Delta);
			}
			Capture(TEXT("ImmediatelyBeforeBufferedAttack"));
			const FGameplayTag AttackTag = FWuwaGameTags::Get().Input_Combat_Attack;
			const float* Override = Handler->BufferLifetimeOverrides.Find(AttackTag);
			const float Lifetime = Override ? *Override : Handler->DefaultBufferLifetimeSeconds;
			// This is the same runtime created and subscribed by the production handler.
			// Call its reflected entry directly here only so the exact result is recorded.
			const EWuwaCombatInputResult Result = Runtime->ProcessInput(ASC, Input(AttackTag), Lifetime);
			Capture(TEXT("BufferedAttackInput"), UEnum::GetValueAsString(Result));
			Report->SetStringField(TEXT("bufferInputResult"), UEnum::GetValueAsString(Result));
			Test.TestTrue(TEXT("Attack issued 0.1 seconds before ReadyEnd is actually buffered"), Result == EWuwaCombatInputResult::Buffered);
			for (int32 Frame = 0; Frame < FMath::CeilToInt(0.3f / FrameStepSeconds); ++Frame) Advance(FrameStepSeconds);
			const FWuwaSkillData After = Character->SkillComponent->GetCurrentSkillData();
			const bool bReplaced = After.ActiveAbility && After.ActiveAbility->GetClass() == ExpectedNext;
			Report->SetBoolField(TEXT("expectedReplacementActive"), bReplaced);
			Report->SetStringField(TEXT("expectedReplacementClass"), ExpectedNext->GetPathName());
			Capture(TEXT("AfterReadyEndObservation"));
			Test.TestTrue(TEXT("Original ReadyEnd becomes visible before the synchronous skill handoff"), bObservedSourceReadyEnd);
			Test.TestTrue(TEXT("Original montage ReadyEnd consumes the attack into the expected next real GA"), bReplaced);
			Test.TestEqual(TEXT("One buffered attack produces exactly one additional activation"), Activations, 2);
			return true;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRealAttackPreinputTest,
	"Wuwa.Combat.RealAssets.Attack01PreinputAtReadyEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRealAttackPreinputTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Guard;
	WuwaRealAttackPreinputTests::FFixture Fixture(*this, TEXT("Attack01To02"));
	if (!Fixture.Initialize()) return false;
	if (!Fixture.Start(FWuwaGameTags::Get().Input_Combat_Attack, Fixture.AttackClasses[0], Fixture.AttackMontages[0])) return false;
	return Fixture.ReplayAttackAtReadyEnd(Fixture.AttackClasses[1]);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRealDashAttackPreinputTest,
	"Wuwa.Combat.RealAssets.BackwardDashPreinputAtReadyEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRealDashAttackPreinputTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Guard;
	WuwaRealAttackPreinputTests::FFixture Fixture(*this, TEXT("DashBackwardToAttack01"));
	if (!Fixture.Initialize()) return false;
	if (!Fixture.Start(FWuwaGameTags::Get().Input_Combat_Dash, Fixture.DashClass, Fixture.DashBackward)) return false;
	return Fixture.ReplayAttackAtReadyEnd(Fixture.AttackClasses[0]);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRealMovingAttackPreinputTest,
	"Wuwa.Combat.RealAssets.Attack01PreinputWhileMovementHeld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRealMovingAttackPreinputTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Guard;
	WuwaRealAttackPreinputTests::FFixture Fixture(*this, TEXT("Attack01To02MovementHeld"));
	if (!Fixture.Initialize()) return false;
	Fixture.HoldMovement();
	if (!Fixture.Start(FWuwaGameTags::Get().Input_Combat_Attack, Fixture.AttackClasses[0], Fixture.AttackMontages[0])) return false;
	return Fixture.ReplayAttackAtReadyEnd(Fixture.AttackClasses[1]);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRealMovingDashAttackPreinputTest,
	"Wuwa.Combat.RealAssets.BackwardDashPreinputWhileMovementHeld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRealMovingDashAttackPreinputTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Guard;
	WuwaRealAttackPreinputTests::FFixture Fixture(*this, TEXT("DashBackwardToAttack01MovementHeld"));
	if (!Fixture.Initialize()) return false;
	// Select the backward Dash with no input, then keep movement pressed throughout
	// the remaining source montage, including its movement-cancel window.
	if (!Fixture.Start(FWuwaGameTags::Get().Input_Combat_Dash, Fixture.DashClass, Fixture.DashBackward)) return false;
	Fixture.HoldMovement();
	return Fixture.ReplayAttackAtReadyEnd(Fixture.AttackClasses[0]);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRealMovingAttack02PreinputTest,
	"Wuwa.Combat.RealAssets.Attack02PreinputMovementCancelBeforeReadyEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRealMovingAttack02PreinputTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Guard;
	for (const int32 FramesPerSecond : {120, 240})
	{
		const FString Scenario = FString::Printf(TEXT("Attack02To03MovementHeld%dHz"), FramesPerSecond);
		WuwaRealAttackPreinputTests::FFixture Fixture(*this, *Scenario);
		Fixture.FrameStepSeconds = 1.f / static_cast<float>(FramesPerSecond);
		if (!Fixture.Initialize()) return false;
		Fixture.HoldMovement();
		if (!Fixture.Start(FWuwaGameTags::Get().Input_Combat_Attack, Fixture.AttackClasses[1], Fixture.AttackMontages[1], true)) return false;
		// In the original Attack02 asset, movement cancellation starts roughly 9 ms
		// before ReadyEnd. Fine-grained real timeline steps expose the ordering.
		Fixture.ReplayAttackAtReadyEnd(Fixture.AttackClasses[2]);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRealMovingAttack02WithoutPreinputTest,
	"Wuwa.Combat.RealAssets.Attack02MovementExitWithoutPreinput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRealMovingAttack02WithoutPreinputTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Guard;
	WuwaRealAttackPreinputTests::FFixture Fixture(*this, TEXT("Attack02MovementHeldWithoutPreinput"));
	Fixture.FrameStepSeconds = 1.f / 240.f;
	if (!Fixture.Initialize()) return false;
	Fixture.HoldMovement();
	if (!Fixture.Start(FWuwaGameTags::Get().Input_Combat_Attack, Fixture.AttackClasses[1], Fixture.AttackMontages[1], true)) return false;
	const FAnimMontageInstance* Instance = Fixture.Anim->GetMontageInstanceForID(Fixture.SourcePlaybackId);
	const float EffectiveRate = FMath::Abs(Instance->GetPlayRate() * Fixture.SourceMontage->RateScale);
	float ReadyTime = 0.f;
	for (const FAnimNotifyEvent& Event : Fixture.SourceMontage->Notifies)
	{
		if (Event.Notify && Event.Notify->IsA(UWuwaAnimNotify_SkillReadyEndBridge::StaticClass())) ReadyTime = Event.GetTime();
	}
	if (!TestTrue(TEXT("Attack02 has a ReadyEnd notify and positive playback rate"), ReadyTime > 0.f && EffectiveRate > 0.f)) return false;
	for (int32 Frame = 0; Frame < FMath::CeilToInt((ReadyTime / EffectiveRate + 0.15f) / Fixture.FrameStepSeconds); ++Frame)
		Fixture.Advance(Fixture.FrameStepSeconds);
	Fixture.Capture(TEXT("MovementExitWithoutAnyAttackInput"));
	TestNull(TEXT("Held movement still exits Attack02 after ReadyEnd when no attack is buffered"),
		Fixture.Character->SkillComponent->GetCurrentSkillData().ActiveAbility.Get());
	TestEqual(TEXT("Movement exit does not create a new attack"), Fixture.Activations, 1);
	TestEqual(TEXT("The real source GA ends exactly once"), Fixture.AbilityEnds, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRealAttackNaturalEndTest,
	"Wuwa.Combat.RealAssets.AllAttackStagesReleaseSkillAfterMontageCompletes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRealAttackNaturalEndTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Guard;
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const FString Scenario = FString::Printf(TEXT("Attack%02dNaturalCompletion"), Index + 1);
		WuwaRealAttackPreinputTests::FFixture Fixture(*this, *Scenario);
		if (!Fixture.Initialize()) return false;
		if (!Fixture.Start(FWuwaGameTags::Get().Input_Combat_Attack, Fixture.AttackClasses[Index], Fixture.AttackMontages[Index], true)) return false;
		const FAnimMontageInstance* Instance = Fixture.Anim->GetMontageInstanceForID(Fixture.SourcePlaybackId);
		const float EffectiveRate = FMath::Abs(Instance->GetPlayRate() * Fixture.SourceMontage->RateScale);
		if (!TestTrue(TEXT("Real attack playback rate is positive"), EffectiveRate > 0.f)) return false;
		const float Duration = Fixture.SourceMontage->GetPlayLength() / EffectiveRate + Fixture.SourceMontage->BlendOut.GetBlendTime() + 0.2f;
		for (int32 Frame = 0; Frame < FMath::CeilToInt(Duration / Fixture.FrameStepSeconds); ++Frame)
			Fixture.Advance(Fixture.FrameStepSeconds);
		Fixture.Capture(TEXT("AfterNaturalMontageCompletion"));
		TestNull(FString::Printf(TEXT("Attack%02d releases skill ownership after the original montage completes"), Index + 1),
			Fixture.Character->SkillComponent->GetCurrentSkillData().ActiveAbility.Get());
		TestEqual(FString::Printf(TEXT("Attack%02d ends its original GA exactly once"), Index + 1), Fixture.AbilityEnds, 1);
		TestEqual(TEXT("No input cannot create a new skill"), Fixture.Activations, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRealAttackGraphPreinputTest,
	"Wuwa.Combat.RealAssets.Attack01PreinputWithActualAnimationBlueprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRealAttackGraphPreinputTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Guard;
	for (const bool bHoldMovement : {false, true})
	{
		WuwaRealAttackPreinputTests::FFixture Fixture(*this,
			bHoldMovement ? TEXT("Attack01ActualABPMovementHeld") : TEXT("Attack01ActualABPStanding"), true);
		if (!Fixture.Initialize()) return false;
		if (bHoldMovement) Fixture.HoldMovement();
		if (!Fixture.Start(FWuwaGameTags::Get().Input_Combat_Attack, Fixture.AttackClasses[0], Fixture.AttackMontages[0])) return false;
		Fixture.ReplayAttackAtReadyEnd(Fixture.AttackClasses[1]);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRealJiabeiAttackDashTest,
	"Wuwa.Combat.RealAssets.Jiabei.DashDuringAttackUsesAttackDash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRealJiabeiAttackDashTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Guard;
	UClass* AttackDashClass = nullptr;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FString Scenario = FString::Printf(TEXT("JiabeiAttack%02dDash"), Index + 1);
		WuwaRealAttackPreinputTests::FFixture Fixture(*this, *Scenario, false, WuwaRealAttackPreinputTests::FRole::Jiabei());
		if (!Fixture.Initialize()) return false;
		AttackDashClass = Fixture.AttackClasses[4];
		if (!Fixture.Start(FWuwaGameTags::Get().Input_Combat_Attack, Fixture.AttackClasses[Index], Fixture.AttackMontages[Index], Index != 0)) return false;
		// 普攻刚出手（还没到接受输入窗口）就按冲刺。
		for (int32 Frame = 0; Frame < FMath::CeilToInt(0.1f / Fixture.FrameStepSeconds); ++Frame) Fixture.Advance(Fixture.FrameStepSeconds);
		const EWuwaCombatInputResult Result = Fixture.Runtime->ProcessInput(Fixture.ASC,
			Fixture.Input(FWuwaGameTags::Get().Input_Combat_Dash), Fixture.Handler->DefaultBufferLifetimeSeconds);
		Fixture.Capture(TEXT("DashDuringAttack"), UEnum::GetValueAsString(Result));
		// 激活当下读：测试里 CMC 不推进、角色不动，RoleGait 之后会按“停稳”把 Dodge 转回 Stand（游戏里根运动在动，不会）
		const UWuwaUnifiedStateBridgeComponent* State = Fixture.Character->UnifiedStateComponent;
		const int32 MoveStateAtStart = IsValid(State) ? static_cast<int32>(State->GetStateData().MoveState) : -1;
		for (int32 Frame = 0; Frame < 3; ++Frame) Fixture.Advance(Fixture.FrameStepSeconds);
		const FWuwaSkillData After = Fixture.Character->SkillComponent->GetCurrentSkillData();
		TestEqual(FString::Printf(TEXT("Dash during Attack%02d is activated immediately"), Index + 1),
			UEnum::GetValueAsString(Result), UEnum::GetValueAsString(EWuwaCombatInputResult::ActivationRequested));
		TestEqual(FString::Printf(TEXT("Dash during Attack%02d selects GA_AttackDash"), Index + 1),
			After.ActiveAbility ? After.ActiveAbility->GetClass() : nullptr, AttackDashClass);
		TestEqual(TEXT("GA_AttackDash plays AM_Jiabei_Charge"), Fixture.ASC->GetCurrentMontage(), Fixture.AttackMontages[4]);
		TestEqual(TEXT("Dash input produces exactly one additional activation"), Fixture.Activations, 2);
		// 与 GA_Dash 一样写入 Dodge：滑铲是闪避，步态系统要知道角色在冲刺里
		TestEqual(TEXT("Slide during attack writes the Dodge move state like GA_Dash"), MoveStateAtStart, static_cast<int32>(EWuwaMoveState::Dodge));
	}

	// 不在普攻里时，冲刺兜底规则仍然走普通 GA_Dash（对照：同样在激活当下写入 Dodge）。
	WuwaRealAttackPreinputTests::FFixture Idle(*this, TEXT("JiabeiIdleDash"), false, WuwaRealAttackPreinputTests::FRole::Jiabei());
	if (!Idle.Initialize()) return false;
	if (!Idle.Start(FWuwaGameTags::Get().Input_Combat_Dash, Idle.DashClass, Idle.DashBackward)) return false;
	const UWuwaUnifiedStateBridgeComponent* IdleState = Idle.Character->UnifiedStateComponent;
	TestEqual(TEXT("Control: plain GA_Dash writes Dodge at activation"),
		IsValid(IdleState) ? static_cast<int32>(IdleState->GetStateData().MoveState) : -1, static_cast<int32>(EWuwaMoveState::Dodge));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRealChangliComboWindowTest,
	"Wuwa.Combat.RealAssets.ComboWindowContinuesThenResets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRealChangliComboWindowTest::RunTest(const FString& Parameters)
{
	// 长离：01→02→03→04，窗口内按攻击接下一段；窗口过后（蒙太奇还在播）按攻击回到第一段。04 之后有心眼，窗口内外都接心眼技 05
	FEditorScriptExecutionGuard Guard;
	const FGameplayTag Attack = FWuwaGameTags::Get().Input_Combat_Attack;
	const FGameplayTag ComboTag = FGameplayTag::RequestGameplayTag(TEXT("State.Combat.ComboWindow"));
	for (int32 Index = 0; Index < 4; ++Index)
	{
		for (const bool bAfterWindow : {false, true})
		{
			const FString Scenario = FString::Printf(TEXT("ChangliAttack%02d%s"), Index + 1, bAfterWindow ? TEXT("AfterComboWindow") : TEXT("InsideComboWindow"));
			WuwaRealAttackPreinputTests::FFixture Fixture(*this, *Scenario);
			if (!Fixture.Initialize()) return false;
			if (!Fixture.Start(Attack, Fixture.AttackClasses[Index], Fixture.AttackMontages[Index], Index != 0)) return false;
			const float Ready = Fixture.ReadyEndTime(), WindowEnd = Fixture.ComboWindowEnd();
			if (!TestTrue(FString::Printf(TEXT("%s has ReadyEnd before its combo window end"), *Scenario), Ready > 0.f && WindowEnd > Ready)) return false;
			Fixture.Advance(Fixture.FrameStepSeconds);
			TestTrue(FString::Printf(TEXT("%s: combo tag present while the window is open"), *Scenario), Fixture.ASC->HasMatchingGameplayTag(ComboTag));
			Fixture.AdvanceSourceTo(bAfterWindow ? WindowEnd + 0.05f : (Ready + WindowEnd) * 0.5f);
			TestEqual(FString::Printf(TEXT("%s: combo tag state"), *Scenario), Fixture.ASC->HasMatchingGameplayTag(ComboTag), !bAfterWindow);
			TestEqual(FString::Printf(TEXT("%s: source attack still owns the skill"), *Scenario),
				Fixture.Character->SkillComponent->GetCurrentSkillData().ActiveAbility ? Fixture.Character->SkillComponent->GetCurrentSkillData().ActiveAbility->GetClass() : nullptr,
				Fixture.AttackClasses[Index]);
			const int32 SourceHandle = Fixture.Character->SkillComponent->GetCurrentSkillData().FightStateHandle;
			const EWuwaCombatInputResult Result = Fixture.Press(Attack);
			const FWuwaSkillData After = Fixture.Character->SkillComponent->GetCurrentSkillData();
			// 普攻 4 在 0.38 秒的 Add Buff 通知加心眼（buff 1205000001）；原作 BP_Input_ChangLi 攻击按下先判断心眼，
			// 有心眼就出心眼技 1205005（AttackClasses[4]），不看连段窗口，所以窗口过后也不回第一段
			UClass* Expected = Fixture.AttackClasses[Index == 3 ? 4 : bAfterWindow ? 0 : Index + 1];
			TestEqual(FString::Printf(TEXT("%s: attack is activated"), *Scenario),
				UEnum::GetValueAsString(Result), UEnum::GetValueAsString(EWuwaCombatInputResult::ActivationRequested));
			TestEqual(FString::Printf(TEXT("%s: selects %s"), *Scenario, *GetNameSafe(Expected)), After.ActiveAbility ? After.ActiveAbility->GetClass() : nullptr, Expected);
			TestNotEqual(FString::Printf(TEXT("%s: a new skill execution started"), *Scenario), After.FightStateHandle, SourceHandle);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRealJiabeiNaturalEndTest,
	"Wuwa.Combat.RealAssets.Jiabei.NewStagesReleaseSkillAfterMontageCompletes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRealJiabeiNaturalEndTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Guard;
	// 新增的 GA_Attack4 / GA_AttackDash 用占位通知窗口，确认它们能正常结束并释放技能。
	for (const int32 Index : {3, 4})
	{
		const FString Scenario = FString::Printf(TEXT("JiabeiStage%dNaturalCompletion"), Index + 1);
		WuwaRealAttackPreinputTests::FFixture Fixture(*this, *Scenario, false, WuwaRealAttackPreinputTests::FRole::Jiabei());
		if (!Fixture.Initialize()) return false;
		const FGameplayTag Tag = Index == 4 ? FWuwaGameTags::Get().Input_Combat_Dash : FWuwaGameTags::Get().Input_Combat_Attack;
		if (!Fixture.Start(Tag, Fixture.AttackClasses[Index], Fixture.AttackMontages[Index], true)) return false;
		const FAnimMontageInstance* Instance = Fixture.Anim->GetMontageInstanceForID(Fixture.SourcePlaybackId);
		const float EffectiveRate = FMath::Abs(Instance->GetPlayRate() * Fixture.SourceMontage->RateScale);
		if (!TestTrue(TEXT("Real stage playback rate is positive"), EffectiveRate > 0.f)) return false;
		const float Duration = Fixture.SourceMontage->GetPlayLength() / EffectiveRate + Fixture.SourceMontage->BlendOut.GetBlendTime() + 0.2f;
		for (int32 Frame = 0; Frame < FMath::CeilToInt(Duration / Fixture.FrameStepSeconds); ++Frame)
			Fixture.Advance(Fixture.FrameStepSeconds);
		Fixture.Capture(TEXT("AfterNaturalMontageCompletion"));
		TestNull(FString::Printf(TEXT("%s releases skill ownership after completion"), *Scenario),
			Fixture.Character->SkillComponent->GetCurrentSkillData().ActiveAbility.Get());
		TestEqual(FString::Printf(TEXT("%s ends its GA exactly once"), *Scenario), Fixture.AbilityEnds, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRealBulletNotifyTest,
	"Wuwa.Combat.RealAssets.BulletNotifiesSpawnBullets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRealBulletNotifyTest::RunTest(const FString& Parameters)
{
	// 真实角色蓝图（带 BulletTable）+ 真实动画蓝图：每段普攻从头播到尾，蒙太奇上的 Spawn Bullet 通知
	// 都应按时生成子弹，子弹 ID 对得上，技能 ID 是正在播这段蒙太奇的 GA 的技能 ID。
	FEditorScriptExecutionGuard Guard;
	using namespace WuwaRealAttackPreinputTests;
	for (const bool bJiabei : {false, true})
	{
		const FRole Role = bJiabei ? FRole::Jiabei() : FRole();
		for (int32 Index = 0; Index < Role.AttackAbilities.Num(); ++Index)
		{
			const FString Scenario = FString::Printf(TEXT("%sBullets%02d"), bJiabei ? TEXT("Jiabei") : TEXT("Changli"), Index + 1);
			FFixture Fixture(*this, *Scenario, true, Role);
			if (!Fixture.Initialize()) return false;
			UAnimMontage* Montage = Fixture.AttackMontages[Index];
			// 期望：这段蒙太奇上 Spawn Bullet 通知的 (时间, 子弹 ID)
			TArray<TPair<float, FName>> Expected;
			for (const FAnimNotifyEvent& Event : Montage->Notifies)
			{
				if (const UWuwaAnimNotify_SpawnBullet* Spawn = Cast<UWuwaAnimNotify_SpawnBullet>(Event.Notify))
				{
					for (const FName& Id : Spawn->BulletIds) Expected.Emplace(Event.GetTime(), Id);
				}
			}
			if (Expected.IsEmpty())
			{
				AddInfo(FString::Printf(TEXT("%s has no Spawn Bullet notifies"), *Montage->GetName()));
				continue;
			}
			UWuwaBulletSubsystem* Bullets = UWuwaBulletSubsystem::Get(Fixture.World);
			if (!TestNotNull(TEXT("bullet subsystem"), Bullets)) return false;
			struct FSpawned { FName Id; float Position; int32 SkillId; int32 AbilitySkillId; };
			TArray<FSpawned> Spawned;
			Bullets->OnBulletSpawned.AddLambda([&](const FWuwaBullet& Bullet)
			{
				const FAnimMontageInstance* Instance = Fixture.Anim->GetMontageInstanceForID(Fixture.SourcePlaybackId);
				const UWuwaGameplayAbilityBase* Ability = Fixture.ASC->GetAnimatingWuwaAbility();
				Spawned.Add({Bullet.BulletId, Instance ? Instance->GetPosition() : -1.f, Bullet.SkillId, Ability ? Ability->GetSkillId() : 0});
			});

			if (!Fixture.Start(FWuwaGameTags::Get().Input_Combat_Attack, Fixture.AttackClasses[Index], Montage, Index != 0)) return false;
			const FAnimMontageInstance* Instance = Fixture.Anim->GetMontageInstanceForID(Fixture.SourcePlaybackId);
			const float Rate = Instance ? FMath::Abs(Instance->GetPlayRate() * Montage->RateScale) : 1.f;
			Fixture.AdvanceSourceTo(Expected.Last().Key + 0.05f);

			if (!TestEqual(*FString::Printf(TEXT("%s spawns one bullet per authored id"), *Montage->GetName()), Spawned.Num(), Expected.Num())) continue;
			for (int32 I = 0; I < Expected.Num(); ++I)
			{
				const FString What = FString::Printf(TEXT("%s bullet %s"), *Montage->GetName(), *Expected[I].Value.ToString());
				TestEqual(*(What + TEXT(" id in order")), Spawned[I].Id, Expected[I].Value);
				// 通知在这一帧推进之后派发：播放位置在通知时间之后、不超过一帧
				TestTrue(*FString::Printf(TEXT("%s spawns at %.3f (notify %.3f)"), *What, Spawned[I].Position, Expected[I].Key),
					Spawned[I].Position >= Expected[I].Key - KINDA_SMALL_NUMBER && Spawned[I].Position <= Expected[I].Key + Fixture.FrameStepSeconds * Rate + KINDA_SMALL_NUMBER);
				TestTrue(*(What + TEXT(" carries the playing skill id")), Spawned[I].SkillId != 0 && Spawned[I].SkillId == Spawned[I].AbilitySkillId);
			}
			AddInfo(FString::Printf(TEXT("%s: %d bullets, skill %d"), *Montage->GetName(), Spawned.Num(), Spawned[0].SkillId));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRealLockOnSkillTargetTest,
	"Wuwa.Combat.RealAssets.Jiabei.AttackLocksTargetAndShootsIt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRealLockOnSkillTargetTest::RunTest(const FString& Parameters)
{
	// 嘉贝普攻1（技能表：锁定、114 号参数 14 米、技能目标方向）：技能开始时挑到怪物、瞬间转向它，枪的判定框（技能目标出生基准）出现在怪物身上
	FEditorScriptExecutionGuard Guard;
	using namespace WuwaRealAttackPreinputTests;
	FFixture Fixture(*this, TEXT("JiabeiAttack01LocksTarget"), true, FRole::Jiabei());
	if (!Fixture.Initialize()) return false;
	const FVector From = Fixture.Character->GetActorLocation();
	AWuwaCharactorBase* Monster = Fixture.World->SpawnActor<AWuwaCharactorBase>(From + FVector(600.f, 600.f, 0.f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("monster spawned"), Monster)) return false;
	Monster->Camp = TEXT("Monster");
	Monster->DispatchBeginPlay();
	TestTrue(TEXT("character faces +X before the attack"), FMath::Abs(Fixture.Character->GetActorRotation().Yaw) < 1.0);

	UWuwaBulletSubsystem* Bullets = UWuwaBulletSubsystem::Get(Fixture.World);
	TArray<FVector> Spawned;
	Bullets->OnBulletSpawned.AddLambda([&Spawned](const FWuwaBullet& Bullet) { Spawned.Add(Bullet.Transform.GetLocation()); });
	TArray<FWuwaBulletHit> Hits;
	Bullets->OnBulletHit.AddLambda([&Hits](const FWuwaBulletHit& Hit) { Hits.Add(Hit); });
	if (!Fixture.Start(FWuwaGameTags::Get().Input_Combat_Attack, Fixture.AttackClasses[0], Fixture.AttackMontages[0])) return false;

	TestTrue(TEXT("skill target is the monster"), Fixture.Character->SkillComponent->GetSkillTarget() == Monster);
	TestTrue(*FString::Printf(TEXT("character turned to the monster (yaw %.1f)"), Fixture.Character->GetActorRotation().Yaw),
		FMath::Abs(FMath::FindDeltaAngleDegrees(Fixture.Character->GetActorRotation().Yaw, 45.0)) < 1.0);
	Fixture.AdvanceSourceTo(0.25f);
	if (TestTrue(TEXT("the shot spawned a bullet"), Spawned.Num() > 0))
	{
		TestTrue(*FString::Printf(TEXT("bullet born on the monster (%.0f cm away)"), FVector::Dist(Spawned[0], Monster->GetActorLocation())),
			FVector::Dist(Spawned[0], Monster->GetActorLocation()) < 1.0);
		// 子弹系统跑一帧：判定窗口内、和怪物重叠 → 命中一次，伤害 ID 是表里的
		Bullets->Tick(1.f / 60.f);
		const FWuwaBulletHit* OnMonster = Hits.FindByPredicate([Monster](const FWuwaBulletHit& Hit) { return Hit.Victim.Get() == Monster; });
		if (TestNotNull(TEXT("the shot hits the monster"), OnMonster))
		{
			TestEqual(TEXT("hit carries the bullet's damage id"), OnMonster->DamageId, static_cast<int64>(1208001001));
			TestEqual(TEXT("first hit on the monster"), OnMonster->VictimHitIndex, 1);
		}
		TestFalse(TEXT("the shot does not hit Jiabei herself"), Hits.ContainsByPredicate([&Fixture](const FWuwaBulletHit& Hit) { return Hit.Victim.Get() == Fixture.Character; }));
	}
	return true;
}

#endif
