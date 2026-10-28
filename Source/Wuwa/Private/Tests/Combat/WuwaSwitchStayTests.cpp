#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameplayTagContainer.h"
#include "Core/Tag/WuwaGameTags.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Formation/WuwaFormationComponent.h"
#include "Game/Input/WuwaInputRouterComponent.h"
#include "Game/Input/WuwaInputTypes.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

namespace WuwaSwitchStayTest
{
	UWorld* FindPlayWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) return Context.World();
		}
		return nullptr;
	}

	bool HasSwitchNoHide(const AWuwaCharacter* Role)
	{
		const UAbilitySystemComponent* Abilities = Role ? Role->GetAbilitySystemComponent() : nullptr;
		return Abilities && Abilities->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Formation.SwitchNoHide")));
	}

	/**
	 * 大卡：第 0 个角色站到 Boss 面前按普攻（有技能目标），技能还没到可结束点时切到第 1 个 →
	 * 旧角色暂留（不隐藏、蒙太奇继续、新角色站到旁边），技能到可结束点后 0.3 秒离场；
	 * 再切回去时第 1 个角色没在放技能 → 立刻离场。
	 */
	class FCheck final : public IAutomationLatentCommand
	{
	public:
		explicit FCheck(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			if (Now - StartedAt > 300.0) { Test->AddError(FString::Printf(TEXT("Timed out at step %d."), Step)); return true; }
			UWorld* World = FindPlayWorld();
			AWuwaPlayerController* PC = World ? Cast<AWuwaPlayerController>(World->GetFirstPlayerController()) : nullptr;
			UWuwaFormationComponent* Formation = PC ? PC->FormationComponent.Get() : nullptr;
			if (!Formation || !Formation->IsFormationReady() || Formation->GetMembers().Num() < 2) return false;
			AWuwaCharacter* First = Formation->GetMembers()[0];
			AWuwaCharacter* Second = Formation->GetMembers()[1];
			if (Step == 0)
			{
				AActor* Boss = nullptr;
				for (TActorIterator<AWuwaCharactorBase> It(World); It; ++It)
				{
					if (It->GetClass()->GetName().Contains(TEXT("BP_Monster_Fuludelisi"))) Boss = *It;
				}
				if (!Boss || PC->GetPawn() != First) return false;
				const FVector Forward = Boss->GetActorForwardVector();
				First->SetActorLocation(Boss->GetActorLocation() + Forward * 200.0, false, nullptr, ETeleportType::TeleportPhysics);
				First->SetActorRotation(FRotator(0.0, (-Forward).Rotation().Yaw, 0.0));
				Step = 1;
				StepAt = Now;
				return false;
			}
			if (Step == 1)
			{
				if (Now - StepAt < 1.0) return false;   // 落地、冷却
				FWuwaInputEvent Event;
				Event.InputTag = FWuwaGameTags::Get().Input_Combat_Attack;
				Event.RouteTag = FWuwaGameTags::Get().Input_Route_Ability;
				Event.Phase = EWuwaInputPhase::Pressed;
				Event.Timestamp = World->GetTimeSeconds();
				PC->GetInputRouter()->DispatchInput(Event);
				Step = 2;
				StepAt = Now;
				return false;
			}
			if (Step == 2)
			{
				// 普攻开始、有技能目标，等 0.15 秒（还没到可结束点）再切人
				const FWuwaSkillData Skill = First->SkillComponent ? First->SkillComponent->GetCurrentSkillData() : FWuwaSkillData();
				if (!IsValid(Skill.ActiveAbility) || Now - StepAt < 0.15)
				{
					if (Now - StepAt > 3.0) { Test->AddError(TEXT("Attack did not start.")); return true; }
					return false;
				}
				Test->TestNotNull(TEXT("attack has a skill target (the boss)"), First->SkillComponent->GetSkillTarget());
				Test->TestFalse(TEXT("attack has not reached ReadyEnd yet"), Skill.bMainSkillReadyEnd);
				UAnimInstance* Anim = First->GetMesh()->GetAnimInstance();
				Montage = Anim ? Anim->GetCurrentActiveMontage() : nullptr;
				Test->TestNotNull(TEXT("attack montage playing"), Montage);
				Formation->RequestSwitch(1);
				Test->TestTrue(TEXT("player now controls the second role"), PC->GetPawn() == Second);
				Test->TestTrue(TEXT("old role stays on field"), First->bOnField && First->IsLingering());
				Test->TestTrue(TEXT("old role got SwitchNoHide (GoDown)"), HasSwitchNoHide(First));
				Test->TestTrue(TEXT("old montage keeps playing"), Anim && Montage && Anim->Montage_IsPlaying(Montage));
				Test->TestTrue(TEXT("old role keeps collision"), First->GetActorEnableCollision());
				const double Distance = FVector::Dist2D(First->GetActorLocation(), Second->GetActorLocation());
				Test->AddInfo(FString::Printf(TEXT("new role placed %.0f cm from the staying role"), Distance));
				Test->TestTrue(TEXT("new role placed beside the staying role (SetQtePosition)"), Distance > 150.0);
				Step = 3;
				StepAt = Now;
				return false;
			}
			if (Step == 3)
			{
				// 暂留期间：技能到了可结束点 → GoDown 去掉 → 0.3 秒后离场
				const FWuwaSkillData Skill = First->SkillComponent->GetCurrentSkillData();
				if (!ReadyEndAt && (!IsValid(Skill.ActiveAbility) || Skill.bMainSkillReadyEnd)) ReadyEndAt = Now;
				if (First->bOnField)
				{
					if (Now - StepAt > 8.0) { Test->AddError(TEXT("Old role never left.")); return true; }
					return false;
				}
				Test->AddInfo(FString::Printf(TEXT("old role stayed %.2f s (ReadyEnd/skill end after %.2f s)"), Now - StepAt, ReadyEndAt ? ReadyEndAt - StepAt : -1.0));
				Test->TestTrue(TEXT("left only after ReadyEnd / skill end"), ReadyEndAt > 0.0);
				Test->TestFalse(TEXT("no longer staying"), First->IsLingering());
				Test->TestFalse(TEXT("SwitchNoHide removed"), HasSwitchNoHide(First));
				Test->TestTrue(TEXT("old role hidden"), First->IsHidden());
				Step = 4;
				StepAt = Now;
				return false;
			}
			if (Step == 4)
			{
				if (Now - StepAt < 1.2) return false;   // 切人冷却
				// 第二个角色站着没放技能：切走时立刻离场
				Formation->RequestSwitch(0);
				Test->TestTrue(TEXT("switched back"), PC->GetPawn() == First);
				Test->TestFalse(TEXT("idle role leaves immediately"), Second->bOnField || Second->IsLingering());
				return true;
			}
			return true;
		}

	private:
		FAutomationTestBase* Test;
		UAnimMontage* Montage = nullptr;
		double StartedAt = -1.0;
		double StepAt = 0.0;
		double ReadyEndAt = 0.0;
		int32 Step = 0;
	};

	/**
	 * 大卡：第 0 个角色普攻中切走（暂留），暂留还没结束就切回来 → 直接接管它：原地不动、蒙太奇继续、不再暂留；
	 * 被切走的第 1 个角色没在放技能，立刻离场。切人冷却临时调小，否则普攻 1 的暂留（约 0.4 秒）比冷却短。
	 */
	class FSwitchBack final : public IAutomationLatentCommand
	{
	public:
		explicit FSwitchBack(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			if (Now - StartedAt > 300.0) { Test->AddError(FString::Printf(TEXT("Timed out at step %d."), Step)); return true; }
			UWorld* World = FindPlayWorld();
			AWuwaPlayerController* PC = World ? Cast<AWuwaPlayerController>(World->GetFirstPlayerController()) : nullptr;
			UWuwaFormationComponent* Formation = PC ? PC->FormationComponent.Get() : nullptr;
			if (!Formation || !Formation->IsFormationReady() || Formation->GetMembers().Num() < 2) return false;
			AWuwaCharacter* First = Formation->GetMembers()[0];
			AWuwaCharacter* Second = Formation->GetMembers()[1];
			if (Step == 0)
			{
				AActor* Boss = nullptr;
				for (TActorIterator<AWuwaCharactorBase> It(World); It; ++It)
					if (It->GetClass()->GetName().Contains(TEXT("BP_Monster_Fuludelisi"))) Boss = *It;
				if (!Boss || PC->GetPawn() != First) return false;
				const FVector Forward = Boss->GetActorForwardVector();
				First->SetActorLocation(Boss->GetActorLocation() + Forward * 200.0, false, nullptr, ETeleportType::TeleportPhysics);
				First->SetActorRotation(FRotator(0.0, (-Forward).Rotation().Yaw, 0.0));
				SavedCooldown = Cooldown(Formation);
				Cooldown(Formation) = 0.05f;
				Step = 1;
				StepAt = Now;
				return false;
			}
			if (Step == 1)
			{
				if (Now - StepAt < 1.0) return false;
				FWuwaInputEvent Event;
				Event.InputTag = FWuwaGameTags::Get().Input_Combat_Attack;
				Event.RouteTag = FWuwaGameTags::Get().Input_Route_Ability;
				Event.Phase = EWuwaInputPhase::Pressed;
				Event.Timestamp = World->GetTimeSeconds();
				PC->GetInputRouter()->DispatchInput(Event);
				Step = 2;
				StepAt = Now;
				return false;
			}
			if (Step == 2)
			{
				const FWuwaSkillData Skill = First->SkillComponent ? First->SkillComponent->GetCurrentSkillData() : FWuwaSkillData();
				if (!IsValid(Skill.ActiveAbility) || Now - StepAt < 0.1)
				{
					if (Now - StepAt > 3.0) { Test->AddError(TEXT("Attack did not start.")); return Finish(Formation); }
					return false;
				}
				Formation->RequestSwitch(1);
				Test->TestTrue(TEXT("first role lingers after switching out"), First->bOnField && First->IsLingering());
				Step = 3;
				StepAt = Now;
				return false;
			}
			if (Step == 3)
			{
				if (Now - StepAt < 0.1) return false;   // 过冷却（已调成 0.05 秒）
				if (!First->IsLingering()) { Test->AddError(TEXT("First role left before it could be switched back.")); return Finish(Formation); }
				const FVector Before = First->GetActorLocation();
				UAnimInstance* Anim = First->GetMesh()->GetAnimInstance();
				UAnimMontage* Montage = Anim ? Anim->GetCurrentActiveMontage() : nullptr;
				Formation->RequestSwitch(0);
				Test->TestTrue(TEXT("control goes straight to the lingering role"), PC->GetPawn() == First);
				Test->TestTrue(TEXT("taken over in place (no teleport)"), FVector::Dist(Before, First->GetActorLocation()) < 1.0);
				Test->TestFalse(TEXT("no longer lingering"), First->IsLingering());
				Test->TestTrue(TEXT("still on field"), First->bOnField && !First->IsHidden());
				Test->TestTrue(TEXT("its montage keeps playing"), Anim && Montage && Anim->Montage_IsPlaying(Montage));
				Test->TestFalse(TEXT("idle second role leaves immediately"), Second->bOnField || Second->IsLingering());
				Step = 4;
				StepAt = Now;
				return false;
			}
			if (Step == 4)
			{
				// 接管后不会再被暂留计时器隐藏掉。
				if (Now - StepAt < 1.0) return false;
				Test->TestTrue(TEXT("taken-over role stays on field"), First->bOnField && !First->IsHidden() && PC->GetPawn() == First);
				return Finish(Formation);
			}
			return Finish(Formation);
		}

	private:
		/** SwitchCooldown 是受保护的配置项，测试里按反射改。 */
		static float& Cooldown(UWuwaFormationComponent* Formation)
		{
			const FFloatProperty* Property = FindFProperty<FFloatProperty>(UWuwaFormationComponent::StaticClass(), TEXT("SwitchCooldown"));
			check(Property);
			return *Property->ContainerPtrToValuePtr<float>(Formation);
		}
		bool Finish(UWuwaFormationComponent* Formation)
		{
			if (Formation && SavedCooldown >= 0.f) Cooldown(Formation) = SavedCooldown;
			return true;
		}
		FAutomationTestBase* Test;
		double StartedAt = -1.0;
		double StepAt = 0.0;
		float SavedCooldown = -1.f;
		int32 Step = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSwitchBackTest, "Wuwa.Formation.SwitchBackToLingering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSwitchBackTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Environment/DaKa/Maps/L_DaKa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(WuwaSwitchStayTest::FSwitchBack(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSwitchStayTest, "Wuwa.Formation.SwitchStay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSwitchStayTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Environment/DaKa/Maps/L_DaKa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(WuwaSwitchStayTest::FCheck(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
