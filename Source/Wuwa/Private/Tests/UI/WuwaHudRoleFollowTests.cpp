#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Formation/WuwaFormationComponent.h"
#include "Game/Input/WuwaInputTypes.h"
#include "Game/LGUI/Component/WuwaUINiagara.h"
#include "Game/LGUI/Core/WuwaUISettings.h"
#include "Game/LGUI/Core/WuwaUiManager.h"
#include "Game/LGUI/View/WuwaUiState.h"
#include "Game/Module/Formation/WuwaFormationUiController.h"
#include "Game/Module/Hud/WuwaHudRoleStatePanel.h"
#include "Game/Module/Hud/EnergyBar/WuwaSpecialEnergyBarJiaBeiLiNa.h"
#include "Game/Module/Hud/EnergyBar/WuwaSpecialEnergyBarMorph.h"
#include "Game/Module/Hud/EnergyBar/WuwaSpecialEnergyBarSlot.h"
#include "Game/Module/Hud/WuwaHudSkillBarPanel.h"
#include "Game/Module/Hud/WuwaMainHudController.h"
#include "Game/Module/Hud/WuwaMainHudView.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaElement.h"
#include "Game/NewWorld/Character/Role/DataAsset/WuwaRoleDataAsset.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

// 主 HUD 里跟着当前角色走的部分（≈ 鸣潮按 RoleInfo / SkillButton / DT_SpecialEnergyBarInfo 刷新）：
// 技能栏的 E / R 图标和按键点击圈、角色状态的特殊能量条和血条。数据在 DA_Role_* 的 Role|UI 里（-run=WuwaRoleUiSetup 写入）。
namespace WuwaHudRoleFollowTests
{
	class FCheckFollow final : public IAutomationLatentCommand
	{
	public:
		explicit FCheckFollow(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			auto TimedOut = [&](const TCHAR* What)
			{
				if (Now - StartedAt < 30.0) return false;
				Test->AddError(What);
				return true;
			};

			AWuwaPlayerController* PC = nullptr;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) PC = Cast<AWuwaPlayerController>(Context.World()->GetFirstPlayerController());
			}
			UWuwaFormationComponent* Formation = PC ? PC->FormationComponent.Get() : nullptr;
			UWuwaUiManager* UiManager = PC ? UWuwaUiManager::Get(PC) : nullptr;
			UWuwaMainHudView* Hud = UiManager ? Cast<UWuwaMainHudView>(UiManager->GetView(UWuwaMainHudController::MainHudViewName)) : nullptr;
			if (!Formation || Formation->GetMembers().Num() < 2 || !Hud || Hud->GetState() != EWuwaUiState::Shown)
			{
				return TimedOut(TEXT("Formation / MainHUD never became ready."));
			}
			UWuwaHudSkillBarPanel* SkillBar = Hud->GetSkillBar();
			UWuwaHudRoleStatePanel* RoleState = Hud->GetRoleState();
			if (!Test->TestNotNull(TEXT("HUD_Main registry 3 = skill bar"), SkillBar) || !Test->TestNotNull(TEXT("HUD_Main registry 4 = role state"), RoleState))
			{
				return true;
			}

			const UWuwaRoleDataAsset* Current = Formation->GetMembers()[Formation->GetCurrentIndex()]->RoleData;
			if (!Test->TestNotNull(TEXT("Current role data"), Current)) return true;

			// 等能量条预制体异步加载出来
			if (RoleState->GetShownRole() != Current || !RoleState->GetEnergyBarActor())
			{
				return TimedOut(*FString::Printf(TEXT("[%s] energy bar %d never spawned."), *Current->GetName(), Current->SpecialEnergyBarId));
			}
			CheckRole(SkillBar, RoleState, Current);

			if (Phase == 0 && ClickPressedAt > 0.0)
			{
				// 点击圈是 UINiagara：0.35 秒的特效，开始播后应该有粒子画进画布。
				// 编辑器里系统第一次用时要等脚本编译完才真正开始，所以等到有粒子（最多 30 秒）
				// Niagara 在不能渲染的进程里（-NullRHI）根本不运行（UNiagaraSystem::IsReadyToRunInternal 先判断 FApp::CanEverRender），
				// 这时只检查"请求了播放"，画面由 UIVisual.HudInGame 在真实渲染下看
				const bool bCanSimulate = FApp::CanEverRender();
				UWuwaUINiagara* Effect = Cast<UWuwaUINiagara>(SkillBar->GetClickEffect(EWuwaSkillButtonType::ResonanceSkill));
				if (Effect)
				{
					MaxSimulated = FMath::Max(MaxSimulated, Effect->GetSimulatedParticleCount());
					MaxRendered = FMath::Max(MaxRendered, Effect->GetRenderedParticleCount());
				}
				// 等到画出粒子，或者特效已经播完（不再等编译、也不在播）
				if (bCanSimulate && Effect && MaxRendered == 0 && (Effect->IsPlaying() || Now - ClickPressedAt < 1.0) && Now - ClickPressedAt < 30.0) return false;
				if (Effect)
				{
					Test->AddInfo(FString::Printf(TEXT("E click effect: waiting for compile %d, max simulated %d, drawcall %d, max rendered %d"),
						Effect->IsWaitingForCompile(), MaxSimulated,
						Effect->GetRenderers().Num() > 0 && Effect->GetRenderers()[0]->HasDrawcall(), MaxRendered));
					Test->TestTrue(TEXT("E click effect builds one LGUI renderer per Niagara renderer"), Effect->GetRenderers().Num() == 1);
					if (bCanSimulate)
					{
						Test->TestTrue(TEXT("E click effect simulates its particle"), MaxSimulated > 0);
						Test->TestTrue(TEXT("E click effect draws its particle into the canvas"), MaxRendered > 0);
					}
					Test->TestTrue(TEXT("E click effect renderer has an LGUI drawcall"), Effect->GetRenderers().Num() > 0 && Effect->GetRenderers()[0]->HasDrawcall());
				}
				else
				{
					Test->AddError(TEXT("E click effect is not a UWuwaUINiagara (rebuild HUD_Main)."));
				}
				ClickPressedAt = -1.0;
				Phase = 1;
				StartedAt = Now;
				const int32 Target = Formation->GetCurrentIndex() == 0 ? 1 : 0;
				const UWuwaRoleDataAsset* After = Formation->GetMembers()[Target]->RoleData;
				PC->GetLocalPlayer()->GetSubsystem<UWuwaFormationUiController>()->RequestSwitchTo(Target);
				Test->TestTrue(TEXT("The test team has two different roles"), After && After != Current);
				Test->TestTrue(TEXT("Skill bar switches with the role right away"), SkillBar->GetShownRole() == After);
				Test->TestTrue(TEXT("Role state switches with the role right away"), RoleState->GetShownRole() == After);
				return false;
			}
			if (Phase == 0)
			{
				// 按键反馈：E 按下（经 PC 分发）→ E 按钮的点击圈播放；松开不播，别的按钮不播
				const UWuwaUISettings* UiSettings = GetDefault<UWuwaUISettings>();
				const FGameplayTag SkillTag = FGameplayTag::RequestGameplayTag(TEXT("Input.Combat.Skill"), false);
				Test->TestTrue(TEXT("UISettings maps Input.Combat.Skill to the E button"),
					UiSettings->SkillButtonInputs.FindRef(SkillTag) == EWuwaSkillButtonType::ResonanceSkill);
				FWuwaInputEvent Input;
				Input.InputTag = SkillTag;
				Input.RouteTag = FGameplayTag::RequestGameplayTag(TEXT("Input.Route.Ability"), false);
				Input.Phase = EWuwaInputPhase::Pressed;
				PC->RouteInputEvent(Input);
				Input.Phase = EWuwaInputPhase::Released;
				PC->RouteInputEvent(Input);
				Test->TestEqual(TEXT("E press plays the E click ring once"), SkillBar->GetClickCount(EWuwaSkillButtonType::ResonanceSkill), 1);
				Test->TestEqual(TEXT("E press does not touch the R button"), SkillBar->GetClickCount(EWuwaSkillButtonType::Liberation), 0);
				ClickPressedAt = Now;
				return false;
			}

			UAbilitySystemComponent* ASC = Formation->GetMembers()[Formation->GetCurrentIndex()]->GetAbilitySystemComponent();
			if (Phase >= 2)
			{
				// 能量条平滑：涨的时候 0.1 秒追到目标，等一会儿再看
				if (Now - PhaseAt < 0.3) return false;
				return CheckEnergyBar(RoleState, ASC, Now);
			}
			// 能量条界面按表里的类创建
			UWuwaSpecialEnergyBarBase* Bar = RoleState->GetEnergyBar();
			if (Test->TestNotNull(TEXT("Energy bar panel created"), Bar))
			{
				Test->TestTrue(TEXT("Energy bar uses the class from DT_SpecialEnergyBarInfo"),
					Bar->GetConfig().BarClass && Bar->IsA(Bar->GetConfig().BarClass));
			}
			// 等级、元素来自 ASC 属性
			if (ASC)
			{
				Test->TestEqual(TEXT("Role state level = Level attribute"), RoleState->GetDisplayedLevel(),
					FMath::RoundToInt(ASC->GetNumericAttribute(UWuwaAttributeSet::GetLevelAttribute())));
				Test->TestTrue(TEXT("Role state element = ElementType attribute"), RoleState->GetDisplayedElement()
					== WuwaElement::FromAttributeValue(ASC->GetNumericAttribute(UWuwaAttributeSet::GetElementTypeAttribute())));
				// 协奏能量：协奏圈的填充比例跟着属性变
				const float MaxConcerto = ASC->GetNumericAttribute(UWuwaAttributeSet::GetMaxConcertoEnergyAttribute());
				if (Test->TestTrue(TEXT("MaxConcertoEnergy initialized"), MaxConcerto > 0.f))
				{
					ASC->SetNumericAttributeBase(UWuwaAttributeSet::GetConcertoEnergyAttribute(), MaxConcerto * 0.5f);
					Test->TestEqual(TEXT("Concerto ring follows ConcertoEnergy"), RoleState->GetDisplayedConcertoPercent(), 0.5f, 0.001f);
				}
			}
			// 当前角色掉血：角色状态的血条跟着变
			const float MaxHealth = ASC ? ASC->GetNumericAttribute(UWuwaAttributeSet::GetMaxHealthAttribute()) : 0.f;
			if (ASC && MaxHealth > 0.f)
			{
				ASC->SetNumericAttributeBase(UWuwaAttributeSet::GetHealthAttribute(), MaxHealth * 0.5f);
				Test->TestEqual(TEXT("Role state health bar follows the current role"), RoleState->GetDisplayedHealthPercent(), 0.5f, 0.001f);
			}
			// 特殊能量：改属性，能量条跟着变
			if (!ASC || !Bar) return true;
			Phase = 2;
			PhaseAt = Now;
			SetHalfEnergy(Bar, ASC);
			return false;
		}

		void SetHalfEnergy(UWuwaSpecialEnergyBarBase* Bar, UAbilitySystemComponent* ASC)
		{
			using S = UWuwaAttributeSet;
			if (Bar->IsA<UWuwaSpecialEnergyBarJiaBeiLiNa>())
			{
				// 嘉贝莉娜：红格读特殊能量 2（上限 40）
				ASC->SetNumericAttributeBase(S::GetSpecialEnergy2Attribute(), ASC->GetNumericAttribute(S::GetMaxSpecialEnergy2Attribute()) * 0.5f);
			}
			else
			{
				ASC->SetNumericAttributeBase(S::GetSpecialEnergy1Attribute(), ASC->GetNumericAttribute(S::GetMaxSpecialEnergy1Attribute()) * 0.5f);
			}
		}

		bool CheckEnergyBar(UWuwaHudRoleStatePanel* RoleState, UAbilitySystemComponent* ASC, double Now)
		{
			using S = UWuwaAttributeSet;
			UWuwaSpecialEnergyBarBase* Bar = RoleState->GetEnergyBar();
			if (!Bar || !ASC) return true;
			if (UWuwaSpecialEnergyBarMorph* Morph = Cast<UWuwaSpecialEnergyBarMorph>(Bar))
			{
				const UWuwaSpecialEnergyBarSlot* Slots = Morph->GetBarItem();
				if (!Test->TestNotNull(TEXT("Morph has its inner slot bar"), Slots)) return true;
				const TArray<TObjectPtr<UWuwaSpecialEnergyBarSlotItem>>& Items = Slots->GetSlotItems();
				if (!Test->TestEqual(TEXT("Slot count = SlotNum"), Items.Num(), Bar->GetConfig().SlotNum)) return true;
				if (Phase == 2)
				{
					// 一半（4 格的 2 格）：前两格填满但按键还不可用（部分状态、填充 1），后两格空
					Test->TestEqual(TEXT("Half energy: bar percent"), Bar->GetCurrentPercent(), 0.5f, 0.001f);
					Test->TestEqual(TEXT("Half energy: slot 0 filled"), Items[0]->GetShownFill(), 1.f, 0.001f);
					Test->TestEqual(TEXT("Half energy: slot 1 filled"), Items[1]->GetShownFill(), 1.f, 0.001f);
					Test->TestEqual(TEXT("Half energy: slot 2 empty"), Items[2]->GetVisualState(), -1);
					Test->TestFalse(TEXT("Half energy: frame keeps the normal texture"), Morph->IsShowingEnableTexture());
					ASC->SetNumericAttributeBase(S::GetSpecialEnergy1Attribute(), ASC->GetNumericAttribute(S::GetMaxSpecialEnergy1Attribute()));
					Phase = 3;
					PhaseAt = Now;
					return false;
				}
				// 满：按键可用，每格都是满状态，外框换成"可用"贴图
				for (int32 Index = 0; Index < Items.Num(); ++Index)
				{
					Test->TestEqual(*FString::Printf(TEXT("Full energy: slot %d full state"), Index), Items[Index]->GetVisualState(), 1);
				}
				Test->TestTrue(TEXT("Full energy: frame switches to EnableTexture"), Morph->IsShowingEnableTexture());
				return true;
			}
			if (UWuwaSpecialEnergyBarJiaBeiLiNa* Jiabei = Cast<UWuwaSpecialEnergyBarJiaBeiLiNa>(Bar))
			{
				const UWuwaSpecialEnergyBarJiaBeiLiNaSlot* Slots = Jiabei->GetBarItem();
				if (!Test->TestNotNull(TEXT("Jiabei has her two-slot bar"), Slots)) return true;
				Test->TestEqual(TEXT("Jiabei red slot follows SpecialEnergy2"), Slots->GetRedPercent(), 0.5f, 0.001f);
				if (Slots->GetSlotItems().Num() > 0)
				{
					Test->TestEqual(TEXT("Jiabei red slot fill"), Slots->GetSlotItems()[0]->GetShownFill(), 0.5f, 0.001f);
				}
			}
			return true;
		}

	private:
		void CheckRole(const UWuwaHudSkillBarPanel* SkillBar, const UWuwaHudRoleStatePanel* RoleState, const UWuwaRoleDataAsset* Role) const
		{
			const FString Name = Role->GetName();
			const EWuwaSkillButtonType Buttons[] = {EWuwaSkillButtonType::ResonanceSkill, EWuwaSkillButtonType::Liberation};
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Buttons); ++Index)
			{
				const FWuwaRoleSkillButton* Button = Role->FindSkillButton(Buttons[Index]);
				if (Test->TestNotNull(*FString::Printf(TEXT("[%s] has skill button %d (run -run=WuwaRoleUiSetup)"), *Name, static_cast<int32>(Buttons[Index])), Button))
				{
					Test->TestTrue(*FString::Printf(TEXT("[%s] skill icon %d is the role's"), *Name, Index), SkillBar->GetShownIcon(Index) == Button->Icon.Get());
				}
			}
			Test->TestEqual(*FString::Printf(TEXT("[%s] energy bar id"), *Name), RoleState->GetShownEnergyBarId(), Role->SpecialEnergyBarId);
		}

		FAutomationTestBase* Test;
		double StartedAt = -1.0;
		int32 Phase = 0;
		double PhaseAt = 0.0;
		double ClickPressedAt = -1.0;
		int32 MaxSimulated = 0;
		int32 MaxRendered = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaHudRoleFollowTest, "Wuwa.UI.Hud.FollowsCurrentRole",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWuwaHudRoleFollowTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Map/Wuwa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(WuwaHudRoleFollowTests::FCheckFollow(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
