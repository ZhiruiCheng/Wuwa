#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "AbilitySystemComponent.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Formation/WuwaFormationComponent.h"
#include "Game/LGUI/Core/WuwaUiManager.h"
#include "Game/LGUI/Mvc/WuwaEventSubsystem.h"
#include "Game/LGUI/View/WuwaUiState.h"
#include "Game/Module/Formation/WuwaFormationEvents.h"
#include "Game/Module/Formation/WuwaFormationModel.h"
#include "Game/Module/Formation/WuwaFormationUiController.h"
#include "Game/Module/Hud/WuwaHudTeamSlotPanel.h"
#include "Game/Module/Hud/WuwaMainHudView.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

namespace WuwaFormationUiTests
{
	AWuwaPlayerController* FindController()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				return Cast<AWuwaPlayerController>(Context.World()->GetFirstPlayerController());
			}
		}
		return nullptr;
	}

	/** 等到地图里的队伍生成好。 */
	bool WaitForFormation(FAutomationTestBase* Test, double& StartedAt, AWuwaPlayerController*& OutPC, UWuwaFormationComponent*& OutFormation)
	{
		const double Now = FPlatformTime::Seconds();
		if (StartedAt < 0.0) StartedAt = Now;
		OutPC = FindController();
		OutFormation = OutPC ? OutPC->FormationComponent.Get() : nullptr;
		if (OutFormation && OutFormation->GetMembers().Num() >= 2 && OutPC->GetLocalPlayer())
		{
			return true;
		}
		if (Now - StartedAt > 20.0) Test->AddError(TEXT("The formation did not spawn in /Game/Map/Wuwa."));
		return false;
	}

	// Controller + Model + 事件：不依赖任何预制体配置。
	class FCheckController final : public IAutomationLatentCommand
	{
	public:
		explicit FCheckController(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			AWuwaPlayerController* PC = nullptr;
			UWuwaFormationComponent* Formation = nullptr;
			if (!WaitForFormation(Test, StartedAt, PC, Formation)) return Test->HasAnyErrors();

			ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
			const UWuwaFormationModel* Model = LocalPlayer->GetSubsystem<UWuwaFormationModel>();
			UWuwaFormationUiController* Controller = LocalPlayer->GetSubsystem<UWuwaFormationUiController>();
			UWuwaEventSubsystem* Events = LocalPlayer->GetGameInstance()->GetSubsystem<UWuwaEventSubsystem>();
			if (!Test->TestNotNull(TEXT("Formation model"), Model) || !Test->TestNotNull(TEXT("Formation UI controller"), Controller)
				|| !Test->TestNotNull(TEXT("Event subsystem"), Events))
			{
				return true;
			}

			// 开局快照：人数、角色数据、当前是 0 号。
			const TArray<TObjectPtr<AWuwaCharacter>>& Members = Formation->GetMembers();
			Test->TestEqual(TEXT("Model has every member"), Model->GetMembers().Num(), Members.Num());
			Test->TestEqual(TEXT("Model current index"), Model->GetCurrentIndex(), Formation->GetCurrentIndex());
			for (int32 i = 0; i < Model->GetMembers().Num(); ++i)
			{
				const FWuwaTeamMemberUiData& Data = Model->GetMembers()[i];
				Test->TestEqual(*FString::Printf(TEXT("Member %d index"), i), Data.Index, i);
				Test->TestTrue(*FString::Printf(TEXT("Member %d role data comes from the character"), i), Data.RoleData && Data.RoleData == Members[i]->RoleData);
				Test->TestEqual(*FString::Printf(TEXT("Member %d on-field flag"), i), Data.bOnField, i == Formation->GetCurrentIndex());
			}

			// 监听事件：用一个临时对象当 Owner，测完一起移除。
			TStrongObjectPtr<UDataTable> Listener(NewObject<UDataTable>(GetTransientPackage()));
			int32 SwitchedCount = 0;
			FWuwaFormationSwitchedEvent LastSwitched;
			int32 HealthCount = 0;
			FWuwaFormationMemberHealthEvent LastHealth;
			Events->Add(WuwaFormationEvents::Switched, Listener.Get(), [&](const FInstancedStruct& Payload)
			{
				++SwitchedCount;
				if (const FWuwaFormationSwitchedEvent* Event = Payload.GetPtr<FWuwaFormationSwitchedEvent>()) LastSwitched = *Event;
			});
			Events->Add(WuwaFormationEvents::MemberHealth, Listener.Get(), [&](const FInstancedStruct& Payload)
			{
				++HealthCount;
				if (const FWuwaFormationMemberHealthEvent* Event = Payload.GetPtr<FWuwaFormationMemberHealthEvent>()) LastHealth = *Event;
			});

			// 切人：通过 Controller 的界面入口请求（等同点了 2 号头像）。
			const int32 Target = Formation->GetCurrentIndex() == 0 ? 1 : 0;
			const int32 Before = Formation->GetCurrentIndex();
			Controller->RequestSwitchTo(Target);
			Test->TestEqual(TEXT("Switched event once"), SwitchedCount, 1);
			Test->TestEqual(TEXT("Switched event old index"), LastSwitched.OldIndex, Before);
			Test->TestEqual(TEXT("Switched event new index"), LastSwitched.NewIndex, Target);
			Test->TestEqual(TEXT("Model follows the switch"), Model->GetCurrentIndex(), Target);
			Test->TestTrue(TEXT("Model on-field flags follow the switch"), Model->GetMember(Target) && Model->GetMember(Target)->bOnField
				&& Model->GetMember(Before) && !Model->GetMember(Before)->bOnField);

			// 不在场队员掉血：Model 更新，发 MemberHealth 事件。
			UAbilitySystemComponent* OffFieldASC = Members[Before]->GetAbilitySystemComponent();
			if (Test->TestNotNull(TEXT("Off-field member ASC"), OffFieldASC))
			{
				const float MaxHealth = FMath::Max(1.f, OffFieldASC->GetNumericAttribute(UWuwaAttributeSet::GetMaxHealthAttribute()));
				if (OffFieldASC->GetNumericAttribute(UWuwaAttributeSet::GetMaxHealthAttribute()) <= 0.f)
				{
					OffFieldASC->SetNumericAttributeBase(UWuwaAttributeSet::GetMaxHealthAttribute(), MaxHealth);
				}
				const int32 HealthEventsBefore = HealthCount;
				OffFieldASC->SetNumericAttributeBase(UWuwaAttributeSet::GetHealthAttribute(), MaxHealth * 0.25f);
				Test->TestTrue(TEXT("Off-field health change emits MemberHealth"), HealthCount > HealthEventsBefore);
				Test->TestEqual(TEXT("MemberHealth event index"), LastHealth.Index, Before);
				Test->TestEqual(TEXT("MemberHealth event value"), LastHealth.Health, MaxHealth * 0.25f);
				Test->TestTrue(TEXT("Model health follows"), Model->GetMember(Before) && FMath::IsNearlyEqual(Model->GetMember(Before)->Health, MaxHealth * 0.25f));
			}

			Events->RemoveAll(Listener.Get());
			return true;
		}

	private:
		FAutomationTestBase* Test;
		double StartedAt = -1.0;
	};

	// 主 HUD 的队伍栏：需要编辑器里配好 MainHUD 的 ViewClass 和两层注册表。
	class FCheckTeamSlots final : public IAutomationLatentCommand
	{
	public:
		explicit FCheckTeamSlots(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			AWuwaPlayerController* PC = nullptr;
			UWuwaFormationComponent* Formation = nullptr;
			if (!WaitForFormation(Test, StartedAt, PC, Formation)) return Test->HasAnyErrors();

			UWuwaUiManager* UiManager = UWuwaUiManager::Get(PC);
			if (!Test->TestNotNull(TEXT("UiManager"), UiManager)) return true;
			UWuwaMainHudView* Hud = Cast<UWuwaMainHudView>(UiManager->GetView(TEXT("MainHUD")));
			if (!Hud && !bRequestedOpen)
			{
				bRequestedOpen = true;
				Hud = Cast<UWuwaMainHudView>(UiManager->OpenView(TEXT("MainHUD")));
			}
			if (!Hud)
			{
				Test->AddError(TEXT("MainHUD is not a UWuwaMainHudView: set ViewClass = WuwaMainHudView on the MainHUD row of DT_UiViews."));
				return true;
			}
			if (Hud->GetState() != EWuwaUiState::Shown)
			{
				if (FPlatformTime::Seconds() - StartedAt > 20.0) { Test->AddError(TEXT("MainHUD never reached Shown.")); return true; }
				return false;
			}

			const TArray<TObjectPtr<UWuwaHudTeamSlotPanel>>& Slots = Hud->GetTeamSlots();
			if (!Test->TestEqual(TEXT("HUD_Main registry lists the three HUD_TeamSlot roots"), Slots.Num(), UWuwaMainHudView::TeamSlotCount))
			{
				return true;
			}
			const int32 MemberCount = Formation->GetMembers().Num();
			auto CheckSlots = [&](const TCHAR* When)
			{
				for (const UWuwaHudTeamSlotPanel* Slot : Slots)
				{
					const int32 i = Slot->GetSlotIndex();
					Test->TestEqual(*FString::Printf(TEXT("[%s] slot %d visible only when it has a member"), When, i), Slot->IsSlotVisible(), i < MemberCount);
					if (i < MemberCount)
					{
						Test->TestEqual(*FString::Printf(TEXT("[%s] slot %d highlighted only when on field"), When, i), Slot->IsHighlighted(), i == Formation->GetCurrentIndex());
					}
				}
			};
			CheckSlots(TEXT("start"));

			const int32 Target = Formation->GetCurrentIndex() == 0 ? 1 : 0;
			PC->GetLocalPlayer()->GetSubsystem<UWuwaFormationUiController>()->RequestSwitchTo(Target);
			CheckSlots(TEXT("after switch"));

			// 血量：改不在场队员的血，对应槽位的血条比例跟着变。
			const int32 OffField = Target == 0 ? 1 : 0;
			UAbilitySystemComponent* ASC = Formation->GetMembers()[OffField]->GetAbilitySystemComponent();
			const float MaxHealth = ASC ? ASC->GetNumericAttribute(UWuwaAttributeSet::GetMaxHealthAttribute()) : 0.f;
			if (ASC && MaxHealth > 0.f)
			{
				ASC->SetNumericAttributeBase(UWuwaAttributeSet::GetHealthAttribute(), MaxHealth * 0.5f);
				Test->TestEqual(TEXT("Slot health bar follows the member"), Slots[OffField]->GetDisplayedHealthPercent(), 0.5f, 0.001f);
			}
			return true;
		}

	private:
		FAutomationTestBase* Test;
		double StartedAt = -1.0;
		bool bRequestedOpen = false;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaFormationUiControllerTest, "Wuwa.UI.Formation.Controller",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWuwaFormationUiControllerTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Map/Wuwa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(WuwaFormationUiTests::FCheckController(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaFormationUiTeamSlotsTest, "Wuwa.UI.Formation.TeamSlots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWuwaFormationUiTeamSlotsTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Map/Wuwa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(WuwaFormationUiTests::FCheckTeamSlots(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
