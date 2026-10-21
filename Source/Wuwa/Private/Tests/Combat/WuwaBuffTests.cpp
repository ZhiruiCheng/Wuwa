#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "AbilitySystemComponent.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Game/Common/WuwaGameTags.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Formation/WuwaFormationComponent.h"
#include "Game/Input/WuwaInputRouterComponent.h"
#include "Game/Input/WuwaInputTypes.h"
#include "Game/LGUI/Core/WuwaUiManager.h"
#include "Game/Module/Formation/WuwaFormationModel.h"
#include "Game/Module/Formation/WuwaFormationUiController.h"
#include "Game/Module/Hud/Buff/WuwaHudBuffBar.h"
#include "Game/Module/Hud/Buff/WuwaHudBuffItem.h"
#include "Game/Module/Hud/WuwaHudRoleStatePanel.h"
#include "Game/Module/Hud/WuwaMainHudController.h"
#include "Game/Module/Hud/WuwaMainHudView.h"
#include "Game/NewWorld/Buff/WuwaBuffLibrary.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

namespace WuwaBuffTest
{
	constexpr int64 TrueSight = 1205000001;
	constexpr int64 RemoveTrueSight = 1205000002;

	const FGameplayTag& TrueSightTag() { static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Role.Changli.Buff.TrueSight")); return Tag; }

	UWorld* FindPlayWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) return Context.World();
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaBuffLibraryTest, "Wuwa.Combat.Buff.Library",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaBuffLibraryTest::RunTest(const FString& Parameters)
{
	using namespace WuwaBuffTest;
	// 导入的数据：DT_Buff → GE、GE 上的 buff 信息、DT_BuffCue 的图标
	const TSubclassOf<UGameplayEffect> Effect = UWuwaBuffLibrary::FindBuffEffect(TrueSight);
	if (!TestNotNull(TEXT("DT_Buff has 1205000001 (心眼)"), Effect.Get())) return false;
	const UGameplayEffect* Defaults = Effect->GetDefaultObject<UGameplayEffect>();
	TestEqual(TEXT("心眼 lasts 12 s (db_buff durationmagnitude)"), Defaults->DurationPolicy, EGameplayEffectDurationType::HasDuration);
	TestTrue(TEXT("心眼 grants Role.Changli.Buff.TrueSight"), Defaults->GetGrantedTags().HasTagExact(TrueSightTag()));
	const UWuwaBuffInfoComponent* Info = UWuwaBuffLibrary::GetBuffInfo(Defaults);
	if (TestNotNull(TEXT("buff info component"), Info))
	{
		TestEqual(TEXT("buff id"), Info->BuffId, TrueSight);
		TestTrue(TEXT("UI cue 1205104053 listed"), Info->CueIds.Contains(1205104053));
	}
	const FWuwaBuffCueRow* Cue = UWuwaBuffLibrary::FindCue(1205104053);
	if (TestNotNull(TEXT("DT_BuffCue has the 心眼 icon cue"), Cue))
	{
		TestEqual(TEXT("cue type 24 (role-unique buff bar)"), Cue->CueType, 24);
		TestTrue(TEXT("icon T_IconChangliY"), Cue->Icon && Cue->Icon->GetName() == TEXT("T_IconChangliY"));
		TestTrue(TEXT("buff mark + fill colour params"), Cue->Parameters.Num() >= 2 && Cue->Parameters[0] == TEXT("1") && Cue->Parameters[1] == TEXT("F0744E7F"));
	}

	// 加 / 查 / 移除（测试世界里的单位）
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	World->InitializeActorsForPlay(FURL());
	AWuwaCharactorBase* Actor = World->SpawnActor<AWuwaCharactorBase>(AWuwaCharactorBase::StaticClass(), FTransform::Identity);
	Actor->DispatchBeginPlay();
	UAbilitySystemComponent* ASC = Actor->GetAbilitySystemComponent();
	const FActiveGameplayEffectHandle Handle = UWuwaBuffLibrary::AddBuff(ASC, TrueSight);
	TestTrue(TEXT("buff added"), Handle.IsValid());
	TestTrue(TEXT("tag granted"), ASC->HasMatchingGameplayTag(TrueSightTag()));
	TestTrue(TEXT("HasBuff"), UWuwaBuffLibrary::HasBuff(ASC, TrueSight));
	const TArray<FWuwaBuffUiData> Buffs = UWuwaFormationUiController::CollectBuffs(ASC);
	if (TestEqual(TEXT("one UI icon"), Buffs.Num(), 1))
	{
		TestEqual(TEXT("icon buff id"), Buffs[0].BuffId, TrueSight);
		TestEqual(TEXT("icon cue type"), Buffs[0].CueType, 24);
		TestNearlyEqual(TEXT("icon duration"), Buffs[0].Duration, 12.f, 1e-3f);
		TestNearlyEqual(TEXT("full at start"), Buffs[0].GetRemainPercent(World->GetTimeSeconds()), 1.f, 1e-3f);
	}
	// 1205000002（瞬时，移除心眼）= 心眼技开始时的 SkillStartBuff
	UWuwaBuffLibrary::AddBuff(ASC, RemoveTrueSight);
	TestFalse(TEXT("1205000002 removes 心眼"), UWuwaBuffLibrary::HasBuff(ASC, TrueSight) || ASC->HasMatchingGameplayTag(TrueSightTag()));
	TestEqual(TEXT("no icons after removal"), UWuwaFormationUiController::CollectBuffs(ASC).Num(), 0);
	UWuwaBuffLibrary::AddBuff(ASC, TrueSight);
	TestEqual(TEXT("RemoveBuff removes by id"), UWuwaBuffLibrary::RemoveBuff(ASC, TrueSight), 1);
	TestFalse(TEXT("gone"), UWuwaBuffLibrary::HasBuff(ASC, TrueSight));
	World->DestroyWorld(false);
	return true;
}

namespace WuwaBuffTest
{
	/**
	 * 大卡：切到长离 → 放普攻 4（1205004），0.38 秒的 Add Buff 通知加心眼 → HUD 专属 buff 栏出现图标、倒计时在走 →
	 * 按普攻：有心眼的规则选中心眼技 1205005 → 技能开始时 1205000002 移除心眼 → 图标收起。
	 */
	class FChangliCheck final : public IAutomationLatentCommand
	{
	public:
		explicit FChangliCheck(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			if (Now - StartedAt > 300.0) { Test->AddError(FString::Printf(TEXT("Timed out at step %d."), Step)); return true; }
			UWorld* World = FindPlayWorld();
			AWuwaPlayerController* PC = World ? Cast<AWuwaPlayerController>(World->GetFirstPlayerController()) : nullptr;
			UWuwaFormationComponent* Formation = PC ? PC->FormationComponent.Get() : nullptr;
			UWuwaUiManager* Manager = PC ? UWuwaUiManager::Get(PC) : nullptr;
			const UWuwaMainHudView* Hud = Manager ? Cast<UWuwaMainHudView>(Manager->GetView(UWuwaMainHudController::MainHudViewName)) : nullptr;
			UWuwaHudRoleStatePanel* RoleState = Hud ? Hud->GetRoleState() : nullptr;
			if (!Formation || !Formation->IsFormationReady() || !RoleState || !RoleState->GetUniqueBuffBar()) return false;
			const int32 ChangliIndex = Formation->GetMembers().IndexOfByPredicate([](const AWuwaCharacter* Member) { return Member && Member->GetClass()->GetName().Contains(TEXT("Changli")); });
			if (!Test->TestTrue(TEXT("Changli is in the team"), ChangliIndex != INDEX_NONE)) return true;
			AWuwaCharacter* Changli = Formation->GetMembers()[ChangliIndex];
			UAbilitySystemComponent* ASC = Changli->GetAbilitySystemComponent();
			const UWuwaFormationModel* Model = PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UWuwaFormationModel>() : nullptr;
			UWuwaHudBuffBar* Bar = RoleState->GetUniqueBuffBar();
			auto CurrentSkill = [Changli]() { return Changli->SkillComponent ? Changli->SkillComponent->GetCurrentSkillData().SkillId : 0; };
			switch (Step)
			{
			case 0:
				if (Formation->GetCurrentIndex() != ChangliIndex) Formation->RequestSwitch(ChangliIndex);
				Step = 1;
				StepAt = Now;
				return false;
			case 1:
			{
				if (Now - StepAt < 1.0 || PC->GetPawn() != Changli) return false;
				if (!bPlaced)
				{
					// 不依赖 PlayerStart（可能摆在高台上，普攻会把人带出边缘）：放到大厅地面上、Boss 前方 6 米。
					for (TActorIterator<AWuwaCharactorBase> It(Changli->GetWorld()); It; ++It)
						if (It->GetClass()->GetName().Contains(TEXT("BP_Monster_Fuludelisi")))
						{
							const FVector Forward = It->GetActorForwardVector();
							Changli->SetActorLocation(It->GetActorLocation() + Forward * 600.0, false, nullptr, ETeleportType::TeleportPhysics);
							Changli->SetActorRotation(FRotator(0.0, (-Forward).Rotation().Yaw, 0.0));
						}
					bPlaced = true;
					StepAt = Now;
					return false;
				}
				if (!Changli->GetCharacterMovement()->IsMovingOnGround()) return false;
				Test->TestEqual(TEXT("no buff icons at first"), Bar->GetShownItems().Num(), 0);
				// 普攻 4（技能表的 GA）
				const FWuwaSkillInfo* Info = Changli->SkillTable ? Changli->SkillTable->FindRow<FWuwaSkillInfo>(TEXT("1205004"), TEXT("Test")) : nullptr;
				if (!Test->TestTrue(TEXT("attack 4 activated"), Info && Info->Ability && ASC->TryActivateAbilityByClass(Info->Ability))) return true;
				Step = 2;
				StepAt = Now;
				return false;
			}
			case 2:
			{
				if (!ASC->HasMatchingGameplayTag(TrueSightTag()))
				{
					if (Now - StepAt > 3.0) { Test->AddError(TEXT("心眼 never granted by AM_Attack04's Add Buff notify.")); return true; }
					return false;
				}
				Test->AddInfo(FString::Printf(TEXT("心眼 granted %.2f s after attack 4 started (notify at 0.38 s)"), Now - StepAt));
				const TArray<FWuwaBuffUiData>* Buffs = Model ? Model->GetBuffs(ChangliIndex) : nullptr;
				Test->TestTrue(TEXT("model has the 心眼 icon"), Buffs && Buffs->Num() == 1 && (*Buffs)[0].BuffId == TrueSight);
				const TArray<UWuwaHudBuffItem*> Items = Bar->GetShownItems();
				if (Test->TestEqual(TEXT("unique buff bar shows one icon"), Items.Num(), 1))
				{
					Test->TestTrue(TEXT("icon is T_IconChangliY"), Items[0]->GetData().Icon && Items[0]->GetData().Icon->GetName() == TEXT("T_IconChangliY"));
					Test->TestTrue(TEXT("icon visible"), Items[0]->IsActive());
				}
				Test->TestEqual(TEXT("normal buff bar ignores cue type 24"), RoleState->GetBuffBar() ? RoleState->GetBuffBar()->GetShownItems().Num() : 0, 0);
				Step = 3;
				StepAt = Now;
				return false;
			}
			case 3:
			{
				if (Now - StepAt < 1.0) return false;
				const TArray<UWuwaHudBuffItem*> Items = Bar->GetShownItems();
				if (Items.Num() == 1)
				{
					const float Fill = Items[0]->GetDisplayedFill();
					Test->AddInfo(FString::Printf(TEXT("countdown fill after ~1 s: %.3f"), Fill));
					Test->TestTrue(TEXT("countdown is running (fill below 1, above 0.8)"), Fill < 1.f && Fill > 0.8f);
				}
				// 按普攻：心眼规则 → 心眼技 1205005
				FWuwaInputEvent Event;
				Event.InputTag = FWuwaGameTags::Get().Input_Combat_Attack;
				Event.RouteTag = FWuwaGameTags::Get().Input_Route_Ability;
				Event.Phase = EWuwaInputPhase::Pressed;
				Event.Timestamp = World->GetTimeSeconds();
				PC->GetInputRouter()->DispatchInput(Event);
				Step = 4;
				StepAt = Now;
				return false;
			}
			case 4:
				if (CurrentSkill() != 1205005)
				{
					if (Now - StepAt > 8.0) { Test->AddError(FString::Printf(TEXT("心眼技 1205005 never started (current %d)."), CurrentSkill())); return true; }
					return false;
				}
				Test->AddInfo(FString::Printf(TEXT("心眼技 started %.2f s after the press"), Now - StepAt));
				Test->TestFalse(TEXT("心眼 consumed at skill start (SkillStartBuffs 1205000002)"), ASC->HasMatchingGameplayTag(TrueSightTag()));
				Step = 5;
				StepAt = Now;
				return false;
			case 5:
				if (Now - StepAt < 0.5) return false;
				Test->TestEqual(TEXT("icon removed from the bar"), Bar->GetShownItems().Num(), 0);
				return true;
			default:
				return true;
			}
		}

	private:
		FAutomationTestBase* Test;
		double StartedAt = -1.0;
		double StepAt = 0.0;
		bool bPlaced = false;
		int32 Step = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaChangliTrueSightTest, "Wuwa.Combat.Buff.ChangliTrueSight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaChangliTrueSightTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Environment/DaKa/Maps/L_DaKa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(WuwaBuffTest::FChangliCheck(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
