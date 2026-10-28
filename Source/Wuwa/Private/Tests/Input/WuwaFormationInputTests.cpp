#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "EnhancedActionKeyMapping.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Core/Tag/WuwaGameTags.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Formation/WuwaFormationComponent.h"
#include "Game/Formation/WuwaFormationInputHandler.h"
#include "Game/Input/DataAsset/WuwaInputDataAsset.h"
#include "Game/Input/WuwaInputTypes.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaFormationInputMappingTest, "Wuwa.Input.Formation.TagToIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWuwaFormationInputMappingTest::RunTest(const FString& Parameters)
{
	const FWuwaGameTags& Tags = FWuwaGameTags::Get();
	TestTrue(TEXT("Input.Route.Formation is registered"), Tags.Input_Route_Formation.IsValid());
	TestEqual(TEXT("Key 1 -> member 0"), UWuwaFormationInputHandler::GetMemberIndexForInput(Tags.Formation_SwitchtoOne), 0);
	TestEqual(TEXT("Key 2 -> member 1"), UWuwaFormationInputHandler::GetMemberIndexForInput(Tags.Formation_SwitchtoTwo), 1);
	TestEqual(TEXT("Key 3 -> member 2"), UWuwaFormationInputHandler::GetMemberIndexForInput(Tags.Formation_SwitchtoThree), 2);
	TestEqual(TEXT("Other inputs are not switch inputs"), UWuwaFormationInputHandler::GetMemberIndexForInput(Tags.Player_UI_ShowMouse), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaFormationInputAssetsTest, "Wuwa.Input.Formation.AssetConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWuwaFormationInputAssetsTest::RunTest(const FString& Parameters)
{
	const FWuwaGameTags& Tags = FWuwaGameTags::Get();
	const UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/CoreInput/Contexts/IMC_Character.IMC_Character"));
	const UWuwaInputDataAsset* Bindings = LoadObject<UWuwaInputDataAsset>(nullptr, TEXT("/Game/CoreInput/DataAsset/DA_IARouterMap.DA_IARouterMap"));
	if (!TestNotNull(TEXT("IMC_Character"), Context) || !TestNotNull(TEXT("DA_IARouterMap"), Bindings))
	{
		return false;
	}

	struct FExpected { const TCHAR* Path; FKey Key; FGameplayTag Tag; };
	const FExpected Expected[] = {
		{ TEXT("/Game/CoreInput/Actions/IA_SwitchChar_1.IA_SwitchChar_1"), EKeys::One, Tags.Formation_SwitchtoOne },
		{ TEXT("/Game/CoreInput/Actions/IA_SwitchChar_2.IA_SwitchChar_2"), EKeys::Two, Tags.Formation_SwitchtoTwo },
		{ TEXT("/Game/CoreInput/Actions/IA_SwitchChar_3.IA_SwitchChar_3"), EKeys::Three, Tags.Formation_SwitchtoThree },
	};
	for (const FExpected& E : Expected)
	{
		const UInputAction* Action = LoadObject<UInputAction>(nullptr, E.Path);
		if (!TestNotNull(E.Path, Action))
		{
			continue;
		}
		TArray<FKey> Keys;
		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			if (Mapping.Action == Action) Keys.Add(Mapping.Key);
			else TestFalse(FString::Printf(TEXT("%s is not also bound to %s"), *E.Key.ToString(), *GetNameSafe(Mapping.Action)), Mapping.Key == E.Key);
		}
		TestEqual(FString::Printf(TEXT("%s is bound to exactly one key"), *Action->GetName()), Keys.Num(), 1);
		TestTrue(FString::Printf(TEXT("%s is bound to %s"), *Action->GetName(), *E.Key.ToString()), Keys.Contains(E.Key));

		const FInputDataAsset* Row = Bindings->InputDataAssetMap.FindByPredicate([Action](const FInputDataAsset& R) { return R.InputAction == Action; });
		if (TestNotNull(FString::Printf(TEXT("%s has a DA row"), *Action->GetName()), Row))
		{
			TestEqual(FString::Printf(TEXT("%s input tag"), *Action->GetName()), Row->InputTag, E.Tag);
			TestEqual(FString::Printf(TEXT("%s route tag"), *Action->GetName()), Row->RouteTag, Tags.Input_Route_Formation);
		}
	}
	return true;
}

namespace WuwaFormationInputTests
{
	UWorld* FindPlayWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) return Context.World();
		}
		return nullptr;
	}

	AWuwaPlayerController* FindController()
	{
		UWorld* World = FindPlayWorld();
		return World ? Cast<AWuwaPlayerController>(World->GetFirstPlayerController()) : nullptr;
	}

	FWuwaInputEvent MakeEvent(const FGameplayTag& InputTag, const FGameplayTag& RouteTag, EWuwaInputPhase Phase)
	{
		FWuwaInputEvent Event;
		Event.InputTag = InputTag;
		Event.RouteTag = RouteTag;
		Event.Phase = Phase;
		return Event;
	}

	void CheckOnlyOnField(FAutomationTestBase* Test, const AWuwaPlayerController* PC, const UWuwaFormationComponent* Formation, int32 Expected, const TCHAR* When)
	{
		const TArray<TObjectPtr<AWuwaCharacter>>& Members = Formation->GetMembers();
		Test->TestEqual(*FString::Printf(TEXT("[%s] current index"), When), Formation->GetCurrentIndex(), Expected);
		Test->TestTrue(*FString::Printf(TEXT("[%s] the controller possesses member %d"), When, Expected), Members.IsValidIndex(Expected) && PC->GetPawn() == Members[Expected]);
		for (int32 i = 0; i < Members.Num(); ++i)
		{
			const AWuwaCharacter* Member = Members[i];
			if (Test->TestNotNull(*FString::Printf(TEXT("[%s] member %d"), When, i), Member))
			{
				Test->TestEqual(*FString::Printf(TEXT("[%s] member %d on-field"), When, i), Member->bOnField, i == Expected);
				Test->TestEqual(*FString::Printf(TEXT("[%s] member %d hidden"), When, i), Member->IsHidden(), i != Expected);
				Test->TestTrue(*FString::Printf(TEXT("[%s] member %d is owned by the player controller"), When, i), Member->GetOwner() == PC);
			}
		}
	}

	/** 找一个和角色骨骼匹配的攻击蒙太奇，用来模拟"攻击中切出去"。 */
	UAnimMontage* FindAttackMontageFor(const AWuwaCharacter* Role)
	{
		const USkeletalMeshComponent* Mesh = Role ? Role->GetMesh() : nullptr;
		const USkeleton* Skeleton = Mesh && Mesh->GetSkeletalMeshAsset() ? Mesh->GetSkeletalMeshAsset()->GetSkeleton() : nullptr;
		const TCHAR* Candidates[] = {
			TEXT("/Game/Characters/Role/changli/AnimMontage/AM_Attack01.AM_Attack01"),
			TEXT("/Game/Characters/Role/jiabei/AnimMontage/AM_Jiabei_Attack01.AM_Jiabei_Attack01"),
		};
		for (const TCHAR* Path : Candidates)
		{
			UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, Path);
			if (Montage && Skeleton && Montage->GetSkeleton() == Skeleton) return Montage;
		}
		return nullptr;
	}

	struct FState
	{
		FVector LocationBeforeSwitch = FVector::ZeroVector;
		int32 Broadcasts = 0;
		int32 LastBroadcastIndex = INDEX_NONE;
		FDelegateHandle Handle;
	};

	// 第一段：开局状态 → 按住 Alt 时切人被屏蔽 → 按 2 切到 1 号 → 冷却内按 1 不生效。
	class FCheckFirstSwitch final : public IAutomationLatentCommand
	{
	public:
		FCheckFirstSwitch(FAutomationTestBase* InTest, TSharedRef<FState> InState) : Test(InTest), State(InState) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			AWuwaPlayerController* PC = FindController();
			if (!PC || !PC->GetPawn())
			{
				if (Now - StartedAt > 20.0) { Test->AddError(TEXT("No AWuwaPlayerController with a pawn.")); return true; }
				return false;
			}
			UWuwaFormationComponent* Formation = PC->FormationComponent;
			if (!Test->TestNotNull(TEXT("Formation component"), Formation)) return true;
			Test->AddInfo(FString::Printf(TEXT("Formation members: %d"), Formation->GetMembers().Num()));
			if (!Test->TestTrue(TEXT("At least two members spawned"), Formation->GetMembers().Num() >= 2)) return true;
			CheckOnlyOnField(Test, PC, Formation, 0, TEXT("start"));

			TSharedRef<FState> Shared = State;
			State->Handle = Formation->OnRoleSwitched.AddLambda([Shared](AWuwaCharacter*, AWuwaCharacter*, int32 NewIndex)
			{
				++Shared->Broadcasts;
				Shared->LastBroadcastIndex = NewIndex;
			});

			const FWuwaGameTags& Tags = FWuwaGameTags::Get();
			const FWuwaInputEvent SwitchTwo = MakeEvent(Tags.Formation_SwitchtoTwo, Tags.Input_Route_Formation, EWuwaInputPhase::Pressed);
			const FWuwaInputEvent SwitchOne = MakeEvent(Tags.Formation_SwitchtoOne, Tags.Input_Route_Formation, EWuwaInputPhase::Pressed);

			// 按住 Alt：切人路由被屏蔽，按 2 不分发、不切人。
			PC->RouteInputEvent(MakeEvent(Tags.Player_UI_ShowMouse, Tags.Input_Route_UI, EWuwaInputPhase::Pressed));
			Test->TestTrue(TEXT("Alt blocks the formation route"), PC->IsRouteBlocked(Tags.Input_Route_Formation));
			Test->TestFalse(TEXT("Key 2 is not dispatched while Alt is held"), PC->RouteInputEvent(SwitchTwo));
			CheckOnlyOnField(Test, PC, Formation, 0, TEXT("key 2 during Alt"));
			PC->RouteInputEvent(MakeEvent(Tags.Player_UI_ShowMouse, Tags.Input_Route_UI, EWuwaInputPhase::Released));
			Test->TestFalse(TEXT("Releasing Alt unblocks the formation route"), PC->IsRouteBlocked(Tags.Input_Route_Formation));

			// 攻击中切出去：0 号正在播攻击蒙太奇。
			AWuwaCharacter* FirstRole = Formation->GetMembers()[0];
			UAnimInstance* FirstAnim = FirstRole->GetMesh()->GetAnimInstance();
			UAnimMontage* Attack = FindAttackMontageFor(FirstRole);
			if (Test->TestNotNull(TEXT("An attack montage matching member 0"), Attack) && Test->TestNotNull(TEXT("Member 0 anim instance"), FirstAnim))
			{
				FirstAnim->Montage_Play(Attack);
				Test->TestTrue(TEXT("Member 0 is attacking before the switch"), FirstAnim->IsAnyMontagePlaying());
			}

			// 按 2：切到 1 号，新角色站在旧角色的位置；镜头朝向（ControlRotation）不因切人改变。
			State->LocationBeforeSwitch = Formation->GetMembers()[0]->GetActorLocation();
			const FRotator ViewRotation(-20.f, Formation->GetMembers()[0]->GetActorRotation().Yaw + 37.f, 0.f);
			PC->SetControlRotation(ViewRotation);
			Test->TestTrue(TEXT("Key 2 is handled by the formation route"), PC->RouteInputEvent(SwitchTwo));
			Test->TestTrue(TEXT("Switching keeps the view rotation"), PC->GetControlRotation().Equals(ViewRotation, 0.01f));
			if (const ACharacter* NewRole = Cast<ACharacter>(PC->GetPawn()))
			{
				// 上场时直接站在地面上，不先进入下落（否则会掉几帧再落地，看起来抖一下）。
				Test->TestEqual(TEXT("The new role starts walking on the floor, not falling"),
					static_cast<int32>(NewRole->GetCharacterMovement()->MovementMode.GetValue()), static_cast<int32>(MOVE_Walking));
			}
			CheckOnlyOnField(Test, PC, Formation, 1, TEXT("after key 2"));
			Test->TestTrue(TEXT("The new role takes the place of the old role"),
				Formation->GetMembers()[1]->GetActorLocation().Equals(State->LocationBeforeSwitch, 1.f));
			Test->TestEqual(TEXT("OnRoleSwitched broadcast once"), State->Broadcasts, 1);
			Test->TestEqual(TEXT("OnRoleSwitched reports the new index"), State->LastBroadcastIndex, 1);

			// 冷却内按 1：不切。
			PC->RouteInputEvent(SwitchOne);
			CheckOnlyOnField(Test, PC, Formation, 1, TEXT("key 1 during cooldown"));
			Test->TestEqual(TEXT("No broadcast during cooldown"), State->Broadcasts, 1);
			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FState> State;
		double StartedAt = -1.0;
	};

	// 第二段（冷却过后）：按住 W 时按 1 切回 0 号；下场角色的移动意图清空，上场角色收到下一帧的 W 继续走。
	class FCheckSwitchBackWhileMoving final : public IAutomationLatentCommand
	{
	public:
		FCheckSwitchBackWhileMoving(FAutomationTestBase* InTest, TSharedRef<FState> InState) : Test(InTest), State(InState) {}

		virtual bool Update() override
		{
			AWuwaPlayerController* PC = FindController();
			UWuwaFormationComponent* Formation = PC ? PC->FormationComponent.Get() : nullptr;
			if (!Formation || Formation->GetMembers().Num() < 2) return true;

			const FWuwaGameTags& Tags = FWuwaGameTags::Get();
			const FInputDataAsset* MoveEntry = PC->InputTagMap ? PC->InputTagMap->InputDataAssetMap.FindByPredicate(
				[&Tags](const FInputDataAsset& Entry) { return Entry.InputTag == Tags.Player_Common_Movement_Move; }) : nullptr;
			if (!Test->TestNotNull(TEXT("Move input entry"), MoveEntry)) return true;
			FWuwaInputEvent Move = MakeEvent(MoveEntry->InputTag, MoveEntry->RouteTag, EWuwaInputPhase::Triggered);
			Move.SourceAction = MoveEntry->InputAction;
			Move.Value = FInputActionValue(FVector2D(0.0, 1.0));

			AWuwaCharacter* First = Formation->GetMembers()[0];
			AWuwaCharacter* Second = Formation->GetMembers()[1];
			PC->RouteInputEvent(Move);
			Test->TestEqual(TEXT("Holding W moves the on-field role"), Second->GetPlayerInputState().MoveAxis, FVector2D(0.0, 1.0));

			PC->RouteInputEvent(MakeEvent(Tags.Formation_SwitchtoOne, Tags.Input_Route_Formation, EWuwaInputPhase::Pressed));
			CheckOnlyOnField(Test, PC, Formation, 0, TEXT("after key 1"));
			Test->TestEqual(TEXT("OnRoleSwitched broadcast for the switch back"), State->Broadcasts, 2);
			Test->TestEqual(TEXT("The off-field role loses its move intent"), Second->GetPlayerInputState().MoveAxis, FVector2D::ZeroVector);
			// 攻击中切出去的角色再上场时，不能接着播离场那一刻冻结的攻击残留。
			const UAnimInstance* FirstAnim = First->GetMesh()->GetAnimInstance();
			Test->TestTrue(TEXT("The returning role has no leftover montage"), FirstAnim && !FirstAnim->IsAnyMontagePlaying());
			Test->TestTrue(TEXT("The returning role keeps the same anim instance (ASC ActorInfo stays valid)"),
				FirstAnim && First->GetAbilitySystemComponent()->AbilityActorInfo.IsValid()
				&& First->GetAbilitySystemComponent()->AbilityActorInfo->GetAnimInstance() == FirstAnim);

			PC->RouteInputEvent(Move);
			Test->TestEqual(TEXT("The next W frame moves the new on-field role"), First->GetPlayerInputState().MoveAxis, FVector2D(0.0, 1.0));
			Move.Phase = EWuwaInputPhase::Released;
			Move.Value.Reset();
			PC->RouteInputEvent(Move);

			Formation->OnRoleSwitched.Remove(State->Handle);
			return true;
		}

	private:
		FAutomationTestBase* Test;
		TSharedRef<FState> State;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaFormationInputRoutingTest, "Wuwa.Input.Formation.Routing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWuwaFormationInputRoutingTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Map/Wuwa")))
	{
		AddError(TEXT("Could not open /Game/Map/Wuwa."));
		return false;
	}
	TSharedRef<WuwaFormationInputTests::FState> State = MakeShared<WuwaFormationInputTests::FState>();
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(WuwaFormationInputTests::FCheckFirstSwitch(this, State));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.5f));
	ADD_LATENT_AUTOMATION_COMMAND(WuwaFormationInputTests::FCheckSwitchBackWhileMoving(this, State));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
