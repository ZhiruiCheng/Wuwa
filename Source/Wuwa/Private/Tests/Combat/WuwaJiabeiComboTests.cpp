#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Common/WuwaGameTags.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Formation/WuwaFormationComponent.h"
#include "Game/Input/WuwaInputRouterComponent.h"
#include "Game/Input/WuwaInputTypes.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

/**
 * 嘉贝地面普攻 / 重击连段（原作 BP_Input_CalbrenaV2 攻击按下 / 攻击长按），输入都走输入路由：
 *  Chain：普攻 1→2→3→4→2（招式进行中按动作标识选：普攻 4 之后回到普攻 2）；每段出手 0.05 秒就按（接招窗口还没开）→ 进预输入缓存，
 *         窗口打开时接下一段。
 *  ResetAfterWindow：普攻 1 的接招窗口关了（ReadyEnd 之后、站着不动技能还在）再按 → 回到普攻 1。
 *  HeavyLoop：每帧发长按（IA_Attack 连续触发）→ 重击 1→2→3→1 循环。
 *  HeavyToAttack：重击 1 的接招窗口里按普攻 → 普攻 2。
 *  ComboWindow：重击 1 结束后（技能表 SkillEndBuff：0.1 秒后挂 0.6 秒的 Role.Jiabei.Combo.Heavy1）按普攻 → 普攻 2；
 *               超过窗口再按 → 回到普攻 1。
 */
namespace WuwaJiabeiComboTest
{
	enum class EScenario : uint8 { Chain, ResetAfterWindow, HeavyLoop, HeavyToAttack, ComboWindow };

	FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(FName(Name), false); }

	UWorld* FindPlayWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) return Context.World();
		return nullptr;
	}

	class FScenario final : public IAutomationLatentCommand
	{
	public:
		FScenario(FAutomationTestBase* InTest, EScenario InScenario) : Test(InTest), Scenario(InScenario) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			if (Now - StartedAt > 300.0) { Test->AddError(FString::Printf(TEXT("Timed out at step %d."), Step)); return true; }
			World = FindPlayWorld();
			PC = World ? Cast<AWuwaPlayerController>(World->GetFirstPlayerController()) : nullptr;
			UWuwaFormationComponent* Formation = PC ? PC->FormationComponent.Get() : nullptr;
			if (!Formation || !Formation->IsFormationReady()) return false;
			int32 JiabeiIndex = INDEX_NONE;
			for (int32 Index = 0; Index < Formation->GetMembers().Num(); ++Index)
				if (Formation->GetMembers()[Index] && Formation->GetMembers()[Index]->GetClass()->GetName().Contains(TEXT("jiabei"))) JiabeiIndex = Index;
			if (JiabeiIndex == INDEX_NONE) return false;
			Jiabei = Formation->GetMembers()[JiabeiIndex];
			ASC = Jiabei->GetAbilitySystemComponent();
			const double GameNow = World->GetTimeSeconds();

			if (Step == 0)
			{
				if (Formation->GetCurrentIndex() != JiabeiIndex) Formation->RequestSwitch(JiabeiIndex);
				Next(GameNow);
				return false;
			}
			if (Step == 1)
			{
				if (GameNow - StepAt < 1.0 || PC->GetPawn() != Jiabei) return false;
				// 大厅地面、Boss 前方 4 米，面朝 Boss
				for (TActorIterator<AWuwaCharactorBase> It(World); It; ++It)
					if (It->GetClass()->GetName().Contains(TEXT("BP_Monster_Fuludelisi")))
					{
						const FVector Forward = It->GetActorForwardVector();
						Jiabei->SetActorLocation(It->GetActorLocation() + Forward * 400.0, false, nullptr, ETeleportType::TeleportPhysics);
						Jiabei->SetActorRotation(FRotator(0.0, (-Forward).Rotation().Yaw, 0.0));
					}
				Next(GameNow);
				return false;
			}
			if (Step == 2)
			{
				if (GameNow - StepAt < 1.0 || !Jiabei->GetWuwaMovementComponent()->IsMovingOnGround()) return false;
				Next(GameNow);
			}
			switch (Scenario)
			{
			case EScenario::Chain: return UpdateChain(GameNow);
			case EScenario::ResetAfterWindow: return UpdateResetAfterWindow(GameNow);
			case EScenario::HeavyLoop: return UpdateHeavyLoop(GameNow);
			case EScenario::HeavyToAttack: return UpdateHeavyToAttack(GameNow);
			default: return UpdateComboWindow(GameNow);
			}
		}

	private:
		int32 CurrentSkill() const
		{
			const FWuwaSkillData Skill = Jiabei->SkillComponent->GetCurrentSkillData();
			return IsValid(Skill.ActiveAbility) ? Skill.SkillId : 0;
		}
		bool Accepting() const { return Jiabei->SkillComponent->IsSkillAcceptingInput(); }
		int32 Count(const TCHAR* Name) const { return ASC ? ASC->GetTagCount(Tag(Name)) : 0; }
		void Next(double GameNow) { ++Step; StepAt = GameNow; }
		void Send(EWuwaInputPhase Phase)
		{
			FWuwaInputEvent Event;
			Event.InputTag = FWuwaGameTags::Get().Input_Combat_Attack;
			Event.RouteTag = FWuwaGameTags::Get().Input_Route_Ability;
			Event.Phase = Phase;
			Event.Timestamp = World->GetTimeSeconds();
			PC->GetInputRouter()->DispatchInput(Event);
		}

		bool UpdateChain(double GameNow)
		{
			if (Step == 3)
			{
				Send(EWuwaInputPhase::Pressed);
				Test->TestEqual(TEXT("first press: attack 1"), CurrentSkill(), 1208001);
				Expected = 1208001;
				Next(GameNow);
				return false;
			}
			if (Step == 4)
			{
				// 出手 0.05 秒（接招窗口还没开）就按：进缓存，当前招式不变
				if (GameNow - StepAt < 0.05) return false;
				Test->TestFalse(*FString::Printf(TEXT("%d's accept window not open yet"), Expected), Accepting());
				const FString Action = FString::Printf(TEXT("Role.Jiabei.Action.Attack%d"), Expected - 1208000);
				Test->TestEqual(*FString::Printf(TEXT("%d carries its action tag"), Expected), Count(*Action), 1);
				Send(EWuwaInputPhase::Pressed);
				Test->TestEqual(*FString::Printf(TEXT("early press during %d is buffered"), Expected), CurrentSkill(), Expected);
				Next(GameNow);
				return false;
			}
			// 接招窗口打开时缓存里的按下接下一段
			const int32 Want = Expected == 1208004 ? 1208002 : Expected + 1;
			if (CurrentSkill() != Want)
			{
				if (GameNow - StepAt > 4.0) { Test->AddError(FString::Printf(TEXT("buffered press never turned %d into %d (current %d)"), Expected, Want, CurrentSkill())); return true; }
				return false;
			}
			Test->AddInfo(FString::Printf(TEXT("%d -> %d after %.2f s"), Expected, Want, GameNow - StepAt + 0.05));
			const bool bDone = Expected == 1208004;
			Expected = Want;
			Step = 4;
			StepAt = GameNow;
			return bDone;
		}

		bool UpdateResetAfterWindow(double GameNow)
		{
			switch (Step)
			{
			case 3:
				Send(EWuwaInputPhase::Pressed);
				Test->TestEqual(TEXT("first press: attack 1"), CurrentSkill(), 1208001);
				Handle = Jiabei->SkillComponent->GetCurrentSkillData().FightStateHandle;
				Next(GameNow);
				return false;
			case 4:
				// 等接招窗口开过、再关掉
				bSawWindow |= Accepting();
				if (!bSawWindow || Accepting())
				{
					if (GameNow - StepAt > 3.0) { Test->AddError(TEXT("attack 1's accept window never opened and closed")); return true; }
					return false;
				}
				Test->TestEqual(TEXT("attack 1 still running after its window (standing still)"), CurrentSkill(), 1208001);
				Send(EWuwaInputPhase::Pressed);
				Test->TestEqual(TEXT("press after the window: attack 1 again"), CurrentSkill(), 1208001);
				Test->TestNotEqual(TEXT("a new attack 1 execution started"), Jiabei->SkillComponent->GetCurrentSkillData().FightStateHandle, Handle);
				return true;
			default:
				return true;
			}
		}

		bool UpdateHeavyLoop(double GameNow)
		{
			// 按住不放：每帧一次长按（控制器对连续触发的键每帧发 Held）
			Send(EWuwaInputPhase::Held);
			const int32 Skill = CurrentSkill();
			if (Skill != 0 && (Sequence.IsEmpty() || Sequence.Last() != Skill))
			{
				Sequence.Add(Skill);
				Test->AddInfo(FString::Printf(TEXT("%.2f s: %d"), GameNow - StepAt, Skill));
			}
			if (Sequence.Num() < 4)
			{
				if (GameNow - StepAt > 12.0) { Test->AddError(FString::Printf(TEXT("hold loop stalled after %d skills"), Sequence.Num())); return true; }
				return false;
			}
			Test->TestEqual(TEXT("hold loop: heavy 1 -> 2 -> 3 -> 1"), Sequence, TArray<int32>{1208016, 1208017, 1208018, 1208016});
			return true;
		}

		bool UpdateHeavyToAttack(double GameNow)
		{
			switch (Step)
			{
			case 3:
				Send(EWuwaInputPhase::Held);
				Test->TestEqual(TEXT("hold without a skill: heavy 1"), CurrentSkill(), 1208016);
				Next(GameNow);
				return false;
			case 4:
				if (CurrentSkill() != 1208016 || !Accepting())
				{
					if (GameNow - StepAt > 3.0) { Test->AddError(TEXT("heavy 1 never opened its accept window")); return true; }
					return false;
				}
				Test->TestEqual(TEXT("heavy 1 carries its action tag"), Count(TEXT("Role.Jiabei.Action.Heavy1")), 1);
				Send(EWuwaInputPhase::Pressed);
				Test->TestEqual(TEXT("press in heavy 1's window: attack 2"), CurrentSkill(), 1208002);
				return true;
			default:
				return true;
			}
		}

		bool UpdateComboWindow(double GameNow)
		{
			switch (Step)
			{
			case 3:
				Send(EWuwaInputPhase::Held);
				Test->TestEqual(TEXT("hold: heavy 1"), CurrentSkill(), 1208016);
				Next(GameNow);
				return false;
			case 4:
				// 等重击 1 自己结束（不按键）
				if (CurrentSkill() != 0)
				{
					if (GameNow - StepAt > 8.0) { Test->AddError(TEXT("heavy 1 never ended")); return true; }
					return false;
				}
				Test->AddInfo(FString::Printf(TEXT("heavy 1 ended after %.2f s"), GameNow - StepAt));
				Next(GameNow);
				return false;
			case 5:
				if (GameNow - StepAt < 0.25) return false;
				Test->TestEqual(TEXT("combo window tag 0.25 s after heavy 1 ended (SkillEndBuff 0.1 s -> 0.6 s)"), Count(TEXT("Role.Jiabei.Combo.Heavy1")), 1);
				Send(EWuwaInputPhase::Pressed);
				Test->TestEqual(TEXT("press in heavy 1's combo window: attack 2"), CurrentSkill(), 1208002);
				Next(GameNow);
				return false;
			case 6:
				if (CurrentSkill() != 0)
				{
					if (GameNow - StepAt > 15.0) { Test->AddError(TEXT("attack 2 never ended")); return true; }
					return false;
				}
				Next(GameNow);
				return false;
			case 7:
				if (GameNow - StepAt < 1.0) return false;
				Test->TestEqual(TEXT("no combo window 1 s after attack 2 ended"), Count(TEXT("Role.Jiabei.Combo.Attack2")), 0);
				Send(EWuwaInputPhase::Pressed);
				Test->TestEqual(TEXT("press after the window: back to attack 1"), CurrentSkill(), 1208001);
				return true;
			default:
				return true;
			}
		}

		FAutomationTestBase* Test;
		EScenario Scenario;
		UWorld* World = nullptr;
		AWuwaPlayerController* PC = nullptr;
		AWuwaCharacter* Jiabei = nullptr;
		UAbilitySystemComponent* ASC = nullptr;
		double StartedAt = -1.0;
		double StepAt = 0.0;
		int32 Step = 0;
		int32 Expected = 0;
		TArray<int32> Sequence;
		int32 Handle = 0;
		bool bSawWindow = false;
	};
}

#define WUWA_JIABEI_COMBO_TEST(Class, Name, Scenario) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class, Name, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
	bool Class::RunTest(const FString& Parameters) \
	{ \
		if (!AutomationOpenMap(TEXT("/Game/Environment/DaKa/Maps/L_DaKa"))) return false; \
		ADD_LATENT_AUTOMATION_COMMAND(WuwaJiabeiComboTest::FScenario(this, WuwaJiabeiComboTest::EScenario::Scenario)); \
		ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); \
		return true; \
	}

WUWA_JIABEI_COMBO_TEST(FWuwaJiabeiComboChainTest, "Wuwa.Combat.RoleSkills.JiabeiComboChain", Chain)
WUWA_JIABEI_COMBO_TEST(FWuwaJiabeiResetAfterWindowTest, "Wuwa.Combat.RoleSkills.JiabeiResetAfterWindow", ResetAfterWindow)
WUWA_JIABEI_COMBO_TEST(FWuwaJiabeiHeavyLoopTest, "Wuwa.Combat.RoleSkills.JiabeiHeavyLoop", HeavyLoop)
WUWA_JIABEI_COMBO_TEST(FWuwaJiabeiHeavyToAttackTest, "Wuwa.Combat.RoleSkills.JiabeiHeavyToAttack", HeavyToAttack)
WUWA_JIABEI_COMBO_TEST(FWuwaJiabeiComboWindowTest, "Wuwa.Combat.RoleSkills.JiabeiComboWindow", ComboWindow)

#endif
