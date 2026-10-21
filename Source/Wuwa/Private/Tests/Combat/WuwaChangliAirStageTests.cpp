#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameplayEffect.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Game/Animation/Notifies/WuwaAnimNotifyState_AddLooseTag.h"
#include "Game/Animation/Notifies/WuwaAnimNotifyState_KeepAwayFromGround.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_AddBuff.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_SpawnBullet.h"
#include "Game/Common/WuwaGameTags.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
#include "Game/NewWorld/Combat/WuwaTimeStopSubsystem.h"
#include "Game/Camera/WuwaSequenceCameraSubsystem.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Formation/WuwaFormationComponent.h"
#include "Game/Input/WuwaInputRouterComponent.h"
#include "Game/Input/WuwaInputTypes.h"
#include "Game/NewWorld/Buff/WuwaBuffLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAirAttackAbility.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAirDodgeAbility.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputCommandConfig.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

namespace WuwaChangliAirStageTest
{
	const TCHAR* ChangliTable = TEXT("/Game/Characters/Role/changli/Data/DT_SkillInfo_Changli.DT_SkillInfo_Changli");
	const TCHAR* JiabeiTable = TEXT("/Game/Characters/Role/jiabei/Data/DT_SkillInfo_Jiabei.DT_SkillInfo_Jiabei");

	FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(FName(Name), false); }

	UWorld* FindPlayWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) return Context.World();
		return nullptr;
	}

	const FWuwaSkillInfo* Row(const UDataTable* Table, int32 Id) { return Table ? Table->FindRow<FWuwaSkillInfo>(*FString::FromInt(Id), TEXT("Test"), false) : nullptr; }

	template <typename T>
	bool HasNotify(const UAnimMontage* Montage)
	{
		if (!Montage) return false;
		for (const FAnimNotifyEvent& Event : Montage->Notifies)
			if (Cast<T>(Event.Notify) || Cast<T>(Event.NotifyStateClass)) return true;
		return false;
	}

	bool HasNotifyClassNamed(const UAnimMontage* Montage, const TCHAR* Fragment)
	{
		if (!Montage) return false;
		for (const FAnimNotifyEvent& Event : Montage->Notifies)
		{
			const UObject* Object = Event.Notify ? static_cast<const UObject*>(Event.Notify) : Event.NotifyStateClass;
			if (Object && Object->GetClass()->GetName().Contains(Fragment)) return true;
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaChangliAirStageAssetsTest, "Wuwa.Combat.RoleSkills.ChangliAirStageAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaChangliAirStageAssetsTest::RunTest(const FString& Parameters)
{
	using namespace WuwaChangliAirStageTest;
	const UDataTable* Changli = LoadObject<UDataTable>(nullptr, ChangliTable);
	const UDataTable* Jiabei = LoadObject<UDataTable>(nullptr, JiabeiTable);
	if (!TestNotNull(TEXT("tables"), Changli) || !TestNotNull(TEXT("Jiabei table"), Jiabei)) return false;

	// 空中普攻 1~4：动作标识（空中普攻次数 buff 的移除条件要看）
	for (int32 Index = 1; Index <= 4; ++Index)
	{
		const FWuwaSkillInfo* Info = Row(Changli, 1205100 + Index);
		TestTrue(*FString::Printf(TEXT("1205%d action tag AirAttack%d"), 100 + Index, Index),
			Info && Info->ActionTags.HasTagExact(Tag(*FString::Printf(TEXT("Role.Changli.Action.AirAttack%d"), Index))));
	}

	// 1205105 空中下落攻击：GA_ChangLi_AirAttack → 地面激活直接结束，三段蒙太奇
	if (const FWuwaSkillInfo* Plunge = Row(Changli, 1205105); TestNotNull(TEXT("1205105 row"), Plunge))
	{
		const UWuwaAirAttackAbility* GA = Plunge->Ability ? Cast<UWuwaAirAttackAbility>(Plunge->Ability->GetDefaultObject()) : nullptr;
		if (TestNotNull(TEXT("1205105 GA is WuwaAirAttackAbility"), GA))
		{
			TestTrue(TEXT("1205105 ends when activated on the ground (GA_ChangLi_AirAttack)"), GA->bEndIfActivatedOnGround);
			TestEqual(TEXT("1205105 interrupt level (original 6)"), GA->InterruptLevel, 6);
			TestEqual(TEXT("1205105 montages"), GA->SkillMontages.Num(), 3);
			if (GA->SkillMontages.Num() == 3)
			{
				TestEqual(TEXT("start"), GetNameSafe(GA->SkillMontages[0]), FString(TEXT("AM_AirAttack_Start")));
				TestEqual(TEXT("loop"), GetNameSafe(GA->SkillMontages[1]), FString(TEXT("AM_AirAttack_Loop")));
				TestEqual(TEXT("end"), GetNameSafe(GA->SkillMontages[2]), FString(TEXT("AM_AirAttack_End")));
				TestTrue(TEXT("start adds the air-attack count buff"), HasNotify<UWuwaAnimNotify_AddBuff>(GA->SkillMontages[0]));
				TestTrue(TEXT("start keeps the role on field when switched out (切人不隐藏 window)"), HasNotify<UWuwaAnimNotifyState_AddLooseTag>(GA->SkillMontages[0]));
				TestTrue(TEXT("loop bullet 1205201009"), HasNotify<UWuwaAnimNotify_SpawnBullet>(GA->SkillMontages[1]));
				TestTrue(TEXT("end bullet 1205006100"), HasNotify<UWuwaAnimNotify_SpawnBullet>(GA->SkillMontages[2]));
				TestTrue(TEXT("loop effects"), HasNotifyClassNamed(GA->SkillMontages[1], TEXT("PlayEffect")));
				TestTrue(TEXT("end effects"), HasNotifyClassNamed(GA->SkillMontages[2], TEXT("PlayEffect")));
				TestTrue(TEXT("end audio"), HasNotifyClassNamed(GA->SkillMontages[2], TEXT("AudioEvent")));
			}
		}
		TestTrue(TEXT("1205105 action tag AirPlunge"), Plunge->ActionTags.HasTagExact(Tag(TEXT("Role.Changli.Action.AirPlunge"))));
	}

	// 1205106 心眼·跃：GA_Common_AirAttack（只有一段），技能开始移除心眼、0.2 秒禁止跳跃
	if (const FWuwaSkillInfo* Leap = Row(Changli, 1205106); TestNotNull(TEXT("1205106 row"), Leap))
	{
		const UWuwaAirAttackAbility* GA = Leap->Ability ? Cast<UWuwaAirAttackAbility>(Leap->Ability->GetDefaultObject()) : nullptr;
		if (TestNotNull(TEXT("1205106 GA is WuwaAirAttackAbility"), GA))
		{
			TestFalse(TEXT("1205106 can start on the ground"), GA->bEndIfActivatedOnGround);
			const UAnimMontage* Montage = GA->SkillMontages.Num() ? GA->SkillMontages[0].Get() : nullptr;
			TestEqual(TEXT("1205106 montage"), GetNameSafe(Montage), FString(TEXT("AM_AirAttack05")));
			TestTrue(TEXT("AM_AirAttack05 keeps away from the ground (lift-off)"), HasNotify<UWuwaAnimNotifyState_KeepAwayFromGround>(Montage));
			TestTrue(TEXT("AM_AirAttack05 bullets"), HasNotify<UWuwaAnimNotify_SpawnBullet>(Montage));
		}
		TestTrue(TEXT("1205106 action tags AirAttack5 + GroundJumpDerive"), Leap->ActionTags.HasTagExact(Tag(TEXT("Role.Changli.Action.AirAttack5")))
			&& Leap->ActionTags.HasTagExact(Tag(TEXT("Role.Changli.Action.GroundJumpDerive"))));
		TestTrue(TEXT("1205106 start buffs remove TrueSight and forbid jumping"), Leap->SkillStartBuffs.Contains(1205000002) && Leap->SkillStartBuffs.Contains(1205601001));
	}

	// 空中闪避（两个角色）：GA_Role_LandEnd_Net，等级 7，边缘保护，转向输入方向
	for (const UDataTable* Table : { Changli, Jiabei })
	{
		for (const int32 Id : { 100003, 100004 })
		{
			const FWuwaSkillInfo* Info = Row(Table, Id);
			if (!TestNotNull(*FString::Printf(TEXT("%s %d row"), *Table->GetName(), Id), Info)) continue;
			const UWuwaAirDodgeAbility* GA = Info->Ability ? Cast<UWuwaAirDodgeAbility>(Info->Ability->GetDefaultObject()) : nullptr;
			if (!TestNotNull(*FString::Printf(TEXT("%s %d GA is WuwaAirDodgeAbility"), *Table->GetName(), Id), GA)) continue;
			TestEqual(*FString::Printf(TEXT("%s %d level 7"), *Table->GetName(), Id), GA->InterruptLevel, 7);
			TestTrue(*FString::Printf(TEXT("%s %d ledge protection"), *Table->GetName(), Id), Info->bWalkOffLedge);
			TestEqual(*FString::Printf(TEXT("%s %d input direction"), *Table->GetName(), Id), Info->SkillDirection, EWuwaSkillDirection::InputDirection);
			const UAnimMontage* Montage = GA->SkillMontages.Num() ? GA->SkillMontages[0].Get() : nullptr;
			TestTrue(*FString::Printf(TEXT("%s %d montage Jump_Second_%s"), *Table->GetName(), Id, Id == 100003 ? TEXT("F") : TEXT("B")),
				Montage && Montage->GetName().EndsWith(Id == 100003 ? TEXT("Jump_Second_F") : TEXT("Jump_Second_B")));
			const UAnimSequence* Sequence = Montage && Montage->SlotAnimTracks.Num() ? Cast<UAnimSequence>(Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference()) : nullptr;
			TestTrue(*FString::Printf(TEXT("%s %d root motion"), *Table->GetName(), Id), Sequence && Sequence->bEnableRootMotion);
		}
	}

	// buff：空中普攻次数 1205000003、闪避禁止 1101004005（移除条件）
	const TSubclassOf<UGameplayEffect> CountClass = UWuwaBuffLibrary::FindBuffEffect(1205000003);
	const UGameplayEffect* Count = CountClass ? CountClass->GetDefaultObject<UGameplayEffect>() : nullptr;
	if (TestNotNull(TEXT("GE 1205000003"), Count))
	{
		TestEqual(TEXT("count buff infinite"), Count->DurationPolicy, EGameplayEffectDurationType::Infinite);
		const UTargetTagRequirementsGameplayEffectComponent* Req = Count->FindComponent<UTargetTagRequirementsGameplayEffectComponent>();
		TestTrue(TEXT("count buff removed when neither in the air nor in an air attack"), Req && Req->RemovalTagRequirements.IgnoreTags.HasTagExact(FWuwaGameTags::Get().State_Position_Air)
			&& Req->RemovalTagRequirements.IgnoreTags.HasTagExact(Tag(TEXT("Role.Changli.Action.AirAttack5"))));
	}
	const TSubclassOf<UGameplayEffect> ForbidClass = UWuwaBuffLibrary::FindBuffEffect(1101004005);
	const UGameplayEffect* Forbid = ForbidClass ? ForbidClass->GetDefaultObject<UGameplayEffect>() : nullptr;
	if (TestNotNull(TEXT("GE 1101004005"), Forbid))
	{
		TestEqual(TEXT("dodge forbid infinite (original duration -1)"), Forbid->DurationPolicy, EGameplayEffectDurationType::Infinite);
		const UTargetTagRequirementsGameplayEffectComponent* Req = Forbid->FindComponent<UTargetTagRequirementsGameplayEffectComponent>();
		TestTrue(TEXT("dodge forbid removed on ground + on field"), Req && Req->RemovalTagRequirements.RequireTags.HasTagExact(FWuwaGameTags::Get().State_Position_Ground)
			&& Req->RemovalTagRequirements.RequireTags.HasTagExact(FWuwaGameTags::Get().State_Formation_OnField));
	}

	// 输入规则：空中链、跳跃派生、空中闪避的条件对象都在
	const UWuwaInputCommandConfig* Commands = LoadObject<UWuwaInputCommandConfig>(nullptr,
		TEXT("/Game/Characters/Role/changli/Input/DA_Changli_InputCommands.DA_Changli_InputCommands"));
	if (TestNotNull(TEXT("Changli input commands"), Commands))
	{
		for (const TCHAR* Name : { TEXT("AirLeapToPlunge"), TEXT("AirTrueSight"), TEXT("AirAttack01"), TEXT("AirAttack04"), TEXT("AirPlungeAfter04"),
			TEXT("JumpTrueSight"), TEXT("JumpTrueSightNotReady"), TEXT("AirDodgeForbid"), TEXT("AirDodgeForward"), TEXT("AirDodgeBack"),
			TEXT("HeavyXinHuo"), TEXT("HeavyDefault"), TEXT("AirHoldPlunge"), TEXT("PlungeTo03") })
		{
			const FWuwaInputCommandRule* Rule = Commands->Rules.FindByPredicate([Name](const FWuwaInputCommandRule& R) { return R.RuleName == Name; });
			if (!TestNotNull(*FString::Printf(TEXT("rule %s"), Name), Rule)) continue;
			for (const TObjectPtr<UWuwaInputCondition>& Condition : Rule->Conditions)
				TestNotNull(*FString::Printf(TEXT("rule %s condition object"), Name), Condition.Get());
		}
	}
	return true;
}

namespace WuwaChangliAirStageTest
{
	enum class EScenario : uint8 { Chain, Leap, Dodge, SkillAir, SkillGround, Burst, Heavy, HeavyXinHuo, PlungeOnBoss };

	/**
	 * 大卡，长离（输入都走输入路由，和玩家按键一样）：
	 *  Chain：起跳 → 空中按普攻 1→2→3→4（空中普攻次数 buff 每段 +1，接招窗口里按）；空中普攻 4 给心眼（原作 0.105 秒加 1205000001），
	 *         所以第 5 下 = 心眼·跃 1205106（原作 空中分支先看心眼），在它的接招窗口再按 = 下落攻击 1205105：
	 *         起手 → 下落循环（强制向下 5000）→ 落地播落地段；落地后次数 buff 被移除；落地段接招窗口里再按 = 地面普攻 3。
	 *  Leap：地面上有心眼按跳跃 → 心眼·跃 1205106（心眼被移除、0.2 秒禁止跳跃），离地 ≥ 80 cm；接招窗口里按普攻 → 下落攻击。
	 *  Dodge：起跳后按闪避（没有方向）→ 空中后闪 100004，加闪避禁止；再按闪避不出第二次；落地后闪避禁止被移除、技能结束。
	 */
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
			int32 ChangliIndex = INDEX_NONE;
			for (int32 Index = 0; Index < Formation->GetMembers().Num(); ++Index)
				if (Formation->GetMembers()[Index] && Formation->GetMembers()[Index]->GetClass()->GetName().Contains(TEXT("Changli"))) ChangliIndex = Index;
			if (ChangliIndex == INDEX_NONE) return false;
			Changli = Formation->GetMembers()[ChangliIndex];
			Movement = Changli->GetWuwaMovementComponent();
			ASC = Changli->GetAbilitySystemComponent();
			const double GameNow = World->GetTimeSeconds();

			if (Step == 0)
			{
				if (Formation->GetCurrentIndex() != ChangliIndex) Formation->RequestSwitch(ChangliIndex);
				Next(GameNow);
				return false;
			}
			if (Step == 1)
			{
				if (GameNow - StepAt < 1.0 || PC->GetPawn() != Changli) return false;
				// 放到大厅地面上、Boss 前方 12 米（不依赖 PlayerStart）
				for (TActorIterator<AWuwaCharactorBase> It(World); It; ++It)
					if (It->GetClass()->GetName().Contains(TEXT("BP_Monster_Fuludelisi")))
					{
						const FVector Forward = It->GetActorForwardVector();
						Changli->SetActorLocation(It->GetActorLocation() + Forward * 1200.0, false, nullptr, ETeleportType::TeleportPhysics);
						Changli->SetActorRotation(FRotator(0.0, (-Forward).Rotation().Yaw, 0.0));
					}
				Next(GameNow);
				return false;
			}
			if (Step == 2)
			{
				if (GameNow - StepAt < 1.0 || !Movement->IsMovingOnGround()) return false;
				GroundZ = Changli->GetActorLocation().Z;
				Next(GameNow);
			}
			switch (Scenario)
			{
			case EScenario::Chain: return UpdateChain(GameNow);
			case EScenario::Leap: return UpdateLeap(GameNow);
			case EScenario::Dodge: return UpdateDodge(GameNow);
			case EScenario::SkillAir: return UpdateSkill(GameNow, true);
			case EScenario::SkillGround: return UpdateSkill(GameNow, false);
			case EScenario::Heavy: return UpdateHeavy(GameNow);
			case EScenario::HeavyXinHuo: return UpdateHeavyXinHuo(GameNow);
			case EScenario::PlungeOnBoss: return UpdatePlungeOnBoss(GameNow);
			default: return UpdateBurst(GameNow);
			}
		}

	private:
		int32 CurrentSkill() const
		{
			const FWuwaSkillData Skill = Changli->SkillComponent->GetCurrentSkillData();
			return IsValid(Skill.ActiveAbility) ? Skill.SkillId : 0;
		}
		FString Montage() const { return GetNameSafe(Changli->GetMesh()->GetAnimInstance()->GetCurrentActiveMontage()); }
		int32 Count(const TCHAR* Name) const { return ASC ? ASC->GetTagCount(Tag(Name)) : 0; }
		void Next(double GameNow) { ++Step; StepAt = GameNow; }

		void Press(const FGameplayTag& InputTag, const FGameplayTag& Route, EWuwaInputPhase Phase = EWuwaInputPhase::Pressed)
		{
			FWuwaInputEvent Event;
			Event.InputTag = InputTag;
			Event.RouteTag = Route;
			Event.Phase = Phase;
			Event.Timestamp = World->GetTimeSeconds();
			PC->GetInputRouter()->DispatchInput(Event);
		}
		void PressJump() { Press(FWuwaGameTags::Get().Player_Common_Movement_Jump, FWuwaGameTags::Get().Input_Route_Movement); }
		void PressAttack() { Press(FWuwaGameTags::Get().Input_Combat_Attack, FWuwaGameTags::Get().Input_Route_Ability); }
		void PressDash() { Press(FWuwaGameTags::Get().Input_Combat_Dash, FWuwaGameTags::Get().Input_Route_Ability); }
		/** 长按普攻：IA_Attack 的 Hold 触发器到点（0.3 秒）后控制器发的 Held 阶段。 */
		void HoldAttack() { Press(FWuwaGameTags::Get().Input_Combat_Attack, FWuwaGameTags::Get().Input_Route_Ability, EWuwaInputPhase::Held); }

		/** 起跳（失败重试到 3 秒）；返回 true = 已经起跳。 */
		bool Jump(double GameNow)
		{
			PressJump();
			if (Movement->IsJumpRootMotionActive()) return true;
			if (GameNow - StepAt > 3.0) { Test->AddError(TEXT("Changli did not jump.")); Step = 99; }
			return false;
		}

		bool UpdateChain(double GameNow)
		{
			switch (Step)
			{
			case 3:
				if (!Jump(GameNow)) return Step == 99;
				Next(GameNow);
				return false;
			case 4:
				if (GameNow - StepAt < 0.3) return false;
				PressAttack();
				Test->TestEqual(TEXT("first press in the air: air attack 1 (not the ground chain)"), CurrentSkill(), 1205101);
				Test->TestEqual(TEXT("AM_AirAttack01 playing"), Montage(), FString(TEXT("AM_AirAttack01")));
				Test->TestEqual(TEXT("air attack 1 action tag on the owner"), Count(TEXT("Role.Changli.Action.AirAttack1")), 1);
				Expected = 1205101;
				Next(GameNow);
				return false;
			case 5:
			{
				// 每段：接招窗口里按普攻 → 1205101 + 次数（次数 buff 在每段前面的通知里 +1）
				if (Movement->IsMovingOnGround()) { Test->AddError(FString::Printf(TEXT("landed during %d"), Expected)); return true; }
				if (CurrentSkill() != Expected || !Changli->SkillComponent->IsSkillAcceptingInput())
				{
					if (GameNow - StepAt > 3.0) { Test->AddError(FString::Printf(TEXT("%d never opened its accept-input window (current %d)"), Expected, CurrentSkill())); return true; }
					return false;
				}
				if (Expected <= 1205104)
				{
					const int32 Layers = Count(TEXT("Role.Changli.Action.AirAttackCount"));
					Test->TestEqual(*FString::Printf(TEXT("air-attack count during %d"), Expected), Layers, Expected - 1205100);
				}
				if (Expected == 1205104) Test->TestEqual(TEXT("air attack 4 grants TrueSight"), Count(TEXT("Role.Changli.Buff.TrueSight")), 1);
				PressAttack();
				const int32 Want = Expected == 1205104 ? 1205106 : Expected == 1205106 ? 1205105 : Expected + 1;
				Test->TestEqual(*FString::Printf(TEXT("press in %d's window starts %d"), Expected, Want), CurrentSkill(), Want);
				Expected = Want;
				StepAt = GameNow;
				if (Want == 1205105) { Test->TestEqual(TEXT("plunge start montage"), Montage(), FString(TEXT("AM_AirAttack_Start"))); Next(GameNow); }
				return false;
			}
			case 6:
			{
				// 下落：先看到循环段、向下速度很大；落地后播落地段
				MinVz = FMath::Min(MinVz, float(Movement->Velocity.Z));
				bSawLoop |= Montage() == TEXT("AM_AirAttack_Loop");
				if (!Movement->IsMovingOnGround() || Montage() != TEXT("AM_AirAttack_End"))
				{
					if (GameNow - StepAt > 5.0) { Test->AddError(FString::Printf(TEXT("plunge never landed (montage %s, skill %d)"), *Montage(), CurrentSkill())); return true; }
					return false;
				}
				Test->AddInfo(FString::Printf(TEXT("plunge: fastest fall %.0f cm/s, landed after %.2f s"), MinVz, GameNow - StepAt));
				Test->TestTrue(TEXT("plunge played the fall loop"), bSawLoop);
				Test->TestTrue(TEXT("plunge falls fast (SetForceSpeed down 5000, faster than a plain fall)"), MinVz < -2500.f);
				Test->TestEqual(TEXT("landing part of the same skill"), CurrentSkill(), 1205105);
				Next(GameNow);
				return false;
			}
			case 7:
				if (GameNow - StepAt < 0.2) return false;
				Test->TestEqual(TEXT("count buff removed after landing"), Count(TEXT("Role.Changli.Action.AirAttackCount")), 0);
				Next(GameNow);
				return false;
			case 8:
				// 原作 攻击按下 地面分支：接招窗口里身上有 动作标识.空中下落攻击 → 地面普攻 3（规则 PlungeTo03）
				if (CurrentSkill() == 1205105 && !Changli->SkillComponent->IsSkillAcceptingInput())
				{
					if (GameNow - StepAt > 6.0) { Test->AddError(TEXT("plunge landing never opened its accept-input window.")); return true; }
					return false;
				}
				Test->TestEqual(TEXT("still the plunge landing when its window opens"), CurrentSkill(), 1205105);
				PressAttack();
				Test->TestEqual(TEXT("attack in the plunge landing window: ground attack 3 (1205003)"), CurrentSkill(), 1205003);
				return true;
			default:
				return true;
			}
		}

		bool UpdateLeap(double GameNow)
		{
			switch (Step)
			{
			case 3:
				UWuwaBuffLibrary::AddBuff(ASC, 1205000001);
				Test->TestEqual(TEXT("TrueSight on"), Count(TEXT("Role.Changli.Buff.TrueSight")), 1);
				PressJump();
				Test->TestEqual(TEXT("jump with TrueSight on the ground: 心眼·跃 1205106"), CurrentSkill(), 1205106);
				Test->TestFalse(TEXT("no plain jump"), Movement->IsJumpRootMotionActive());
				Test->TestEqual(TEXT("TrueSight consumed at skill start (1205000002)"), Count(TEXT("Role.Changli.Buff.TrueSight")), 0);
				Test->TestEqual(TEXT("jump forbidden for 0.2 s (1205601001)"), Count(TEXT("Role.Changli.Action.JumpForbid")), 1);
				Test->TestEqual(TEXT("GroundJumpDerive tag during the skill"), Count(TEXT("Role.Changli.Action.GroundJumpDerive")), 1);
				Next(GameNow);
				return false;
			case 4:
				MaxRise = FMath::Max(MaxRise, float(Changli->GetActorLocation().Z - GroundZ));
				if (!Changli->SkillComponent->IsSkillAcceptingInput())
				{
					if (GameNow - StepAt > 2.0) { Test->AddError(TEXT("1205106 never opened its accept-input window.")); return true; }
					return false;
				}
				Test->AddInfo(FString::Printf(TEXT("心眼·跃 rose %.0f cm before its accept window"), MaxRise));
				Test->TestTrue(TEXT("心眼·跃 leaves the ground (KeepAwayFromGround ≥ 80 cm + root motion)"), MaxRise >= 80.f);
				Test->TestEqual(TEXT("still 1205106 in its window"), CurrentSkill(), 1205106);
				PressAttack();
				Test->TestEqual(TEXT("attack in 心眼·跃's window: plunge 1205105"), CurrentSkill(), 1205105);
				Next(GameNow);
				return false;
			case 5:
				if (!Movement->IsMovingOnGround() || Montage() != TEXT("AM_AirAttack_End"))
				{
					if (GameNow - StepAt > 5.0) { Test->AddError(FString::Printf(TEXT("plunge after 心眼·跃 never landed (montage %s)"), *Montage())); return true; }
					return false;
				}
				return true;
			default:
				return true;
			}
		}

		void PressSkill() { Press(FGameplayTag::RequestGameplayTag(TEXT("Input.Combat.Skill")), FWuwaGameTags::Get().Input_Route_Ability); }
		void PressUltimate() { Press(FGameplayTag::RequestGameplayTag(TEXT("Input.Combat.Ultimate")), FWuwaGameTags::Get().Input_Route_Ability); }
		float Attribute(const FGameplayAttribute& Attr) const { bool bFound = false; return ASC ? ASC->GetGameplayAttributeValue(Attr, bFound) : 0.f; }

		/**
		 * E（1205201，GA_Common_AirAttack）：空中 / 地面都能放；起手（地面放时按根运动腾空）→ 下落循环 → 落地段，最后回到地面；
		 * 开始加协奏 1400，起手 0.78 秒给心眼。
		 */
		bool UpdateSkill(double GameNow, bool bFromAir)
		{
			switch (Step)
			{
			case 3:
				if (bFromAir && !Jump(GameNow)) return Step == 99;
				Next(GameNow);
				return false;
			case 4:
				if (bFromAir && GameNow - StepAt < 0.3) return false;
				Concerto = Attribute(UWuwaAttributeSet::GetConcertoEnergyAttribute());
				StartZ = Changli->GetActorLocation().Z;
				PressSkill();
				Test->TestEqual(TEXT("E starts 1205201"), CurrentSkill(), 1205201);
				Test->TestEqual(TEXT("AM_Skill01_Start playing"), Montage(), FString(TEXT("AM_Skill01_Start")));
				Test->TestTrue(TEXT("E start buff: concerto +1400 (1205200001)"), Attribute(UWuwaAttributeSet::GetConcertoEnergyAttribute()) >= FMath::Min(Concerto + 1400.f, Attribute(UWuwaAttributeSet::GetMaxConcertoEnergyAttribute())) - 1.f);
				Next(GameNow);
				return false;
			case 5:
				MaxRise = FMath::Max(MaxRise, float(Changli->GetActorLocation().Z - StartZ));
				bSawLoop |= Montage() == TEXT("AM_Skill01_Loop");
				// 诊断：蒙太奇 / 移动模式变化的时间线（失败时一起打印）
				if (const FString Now = FString::Printf(TEXT("%s/%d/%s"), *Montage(), CurrentSkill(), *UEnum::GetValueAsString(Movement->MovementMode.GetValue())); Now != LastTrace)
				{
					Trace += FString::Printf(TEXT(" | %.2f %s z=%.0f"), GameNow - StepAt, *Now, Changli->GetActorLocation().Z);
					LastTrace = Now;
				}
				bGotTrueSight |= Count(TEXT("Role.Changli.Buff.TrueSight")) > 0;
				if (!Movement->IsMovingOnGround() || Montage() != TEXT("AM_Skill01_End"))
				{
					if (GameNow - StepAt > 6.0) { Test->AddError(FString::Printf(TEXT("E never landed (montage %s, skill %d)%s"), *Montage(), CurrentSkill(), *Trace)); return true; }
					return false;
				}
				Test->AddInfo(FString::Printf(TEXT("E from %s: rose %.0f cm, landed after %.2f s"), bFromAir ? TEXT("air") : TEXT("ground"), MaxRise, GameNow - StepAt));
				if (!bFromAir) Test->TestTrue(TEXT("E from the ground lifts off with the start anim root motion (original rises ~346 cm)"), MaxRise > 250.f);
				Test->TestTrue(TEXT("E played the fall loop"), bSawLoop);
				Test->TestTrue(TEXT("E start gives TrueSight at 0.78 s"), bGotTrueSight);
				Test->TestEqual(TEXT("landing segment of the same skill"), CurrentSkill(), 1205201);
				Next(GameNow);
				return false;
			case 6:
				if (CurrentSkill() == 1205201 && GameNow - StepAt < 6.0) return false;
				Test->TestNotEqual(TEXT("E ended by itself"), CurrentSkill(), 1205201);
				return true;
			default:
				return true;
			}
		}

		/**
		 * 大招（1205401，GA_ChangLi_Burst）：能量不满按 R 不出；能量满后在空中按 R → 先落到地面再播，能量清空；
		 * 时停冻住 Boss、无敌、不能切人、切到特写镜头（结束后视角回到角色）。
		 */
		bool UpdateBurst(double GameNow)
		{
			UWuwaSequenceCameraSubsystem* Cameras = UWuwaSequenceCameraSubsystem::Get(World);
			UWuwaTimeStopSubsystem* TimeStop = UWuwaTimeStopSubsystem::Get(World);
			switch (Step)
			{
			case 3:
			{
				ASC->SetNumericAttributeBase(UWuwaAttributeSet::GetEnergyAttribute(), 0.f);
				PressUltimate();
				Test->TestNotEqual(TEXT("no burst without full energy"), CurrentSkill(), 1205401);
				ASC->SetNumericAttributeBase(UWuwaAttributeSet::GetEnergyAttribute(), Attribute(UWuwaAttributeSet::GetMaxEnergyAttribute()));
				Next(GameNow);
				return false;
			}
			case 4:
				if (!Jump(GameNow)) return Step == 99;
				Next(GameNow);
				return false;
			case 5:
			{
				if (GameNow - StepAt < 0.35) return false;
				const double AirZ = Changli->GetActorLocation().Z;
				HeartFire = Attribute(UWuwaAttributeSet::GetSpecialEnergy1Attribute());
				PressUltimate();
				Test->TestEqual(TEXT("burst 1205401 starts in the air"), CurrentSkill(), 1205401);
				Test->TestEqual(TEXT("AM_Burst01 playing"), Montage(), FString(TEXT("AM_Burst01")));
				Test->AddInfo(FString::Printf(TEXT("burst: was %.0f cm above the ground, now %.0f"), AirZ - GroundZ, Changli->GetActorLocation().Z - GroundZ));
				Test->TestTrue(TEXT("burst in the air snaps to the ground first (GA_ChangLi_Burst TraceGround)"), FMath::Abs(Changli->GetActorLocation().Z - GroundZ) < 5.0);
				Test->TestTrue(TEXT("energy consumed (1205300001)"), Attribute(UWuwaAttributeSet::GetEnergyAttribute()) < 1.f);
				Next(GameNow);
				return false;
			}
			case 6:
			{
				if (GameNow - StepAt < 0.3) return false;
				Test->TestTrue(TEXT("time stop active"), TimeStop && TimeStop->IsTimeStopActive());
				for (TActorIterator<AWuwaCharactorBase> It(World); It; ++It)
					if (It->GetClass()->GetName().Contains(TEXT("BP_Monster_Fuludelisi")))
					{
						Test->AddInfo(FString::Printf(TEXT("boss frozen %d dilation %.3f distance %.0f"), TimeStop && TimeStop->IsFrozen(*It), It->CustomTimeDilation,
							FVector::Dist(It->GetActorLocation(), Changli->GetActorLocation())));
						Test->TestTrue(TEXT("boss frozen by the time stop"), TimeStop && TimeStop->IsFrozen(*It) && It->CustomTimeDilation == 0.f);
					}
				Test->TestFalse(TEXT("caster not frozen"), TimeStop && TimeStop->IsFrozen(Changli));
				Test->TestTrue(TEXT("invincible during the burst"), Count(TEXT("State.Combat.Invincible")) > 0);
				Test->TestTrue(TEXT("cannot switch during the burst"), Count(TEXT("State.Formation.SwitchForbid")) > 0);
				Test->TestTrue(TEXT("sequence camera playing"), Cameras && Cameras->IsPlaying());
				Test->TestTrue(TEXT("view on the sequence camera"), Cameras && Cameras->GetCameraActor() && PC->GetViewTarget() == Cameras->GetCameraActor());
				Next(GameNow);
				return false;
			}
			case 7:
				if (GameNow - StepAt < 4.0) return false;   // 镜头 3.17 秒 + 混回 0.5 秒
				Test->TestFalse(TEXT("sequence camera finished"), Cameras && Cameras->IsPlaying());
				Test->TestTrue(TEXT("view back on Changli"), PC->GetViewTarget() == Changli);
				Test->TestFalse(TEXT("time stop over"), TimeStop && TimeStop->IsTimeStopActive());
				Test->TestTrue(TEXT("heart fire +4 at 2.6 s (1205104005)"), Attribute(UWuwaAttributeSet::GetSpecialEnergy1Attribute()) >= FMath::Min(HeartFire + 4.f, Attribute(UWuwaAttributeSet::GetMaxSpecialEnergy1Attribute())) - 0.01f);
				return true;
			default:
				return true;
			}
		}

		/**
		 * 重击（原作 攻击长按 地面分支）：按下先出普攻 1，0.3 秒长按到点 → 1205006（打断等级 3 > 普攻 2）。
		 * AM_AttackL 0.66 秒加两层空中普攻次数（1205000003），上挑离地后在接招窗口按普攻 = 1205101 + 2 = 空中普攻 3。
		 */
		bool UpdateHeavy(double GameNow)
		{
			switch (Step)
			{
			case 3:
				ASC->SetNumericAttributeBase(UWuwaAttributeSet::GetSpecialEnergy1Attribute(), 0.f);
				PressAttack();
				Test->TestEqual(TEXT("press: ground attack 1 first"), CurrentSkill(), 1205001);
				Next(GameNow);
				return false;
			case 4:
				if (GameNow - StepAt < 0.3) return false;
				HoldAttack();
				Next(GameNow);
				return false;
			case 5:
				if (CurrentSkill() != 1205006)
				{
					if (GameNow - StepAt > 1.0) { Test->AddError(FString::Printf(TEXT("hold did not start the heavy attack (current %d)"), CurrentSkill())); return true; }
					return false;
				}
				Test->AddInfo(FString::Printf(TEXT("heavy started %.2f s after the hold event"), GameNow - StepAt));
				Test->TestEqual(TEXT("AM_AttackL playing"), Montage(), FString(TEXT("AM_AttackL")));
				StartZ = Changli->GetActorLocation().Z;
				Next(GameNow);
				return false;
			case 6:
			{
				MaxRise = FMath::Max(MaxRise, float(Changli->GetActorLocation().Z - StartZ));
				if (const FString Now = FString::Printf(TEXT("%s/%d/%s/count%d"), *Montage(), CurrentSkill(), *UEnum::GetValueAsString(Movement->MovementMode.GetValue()),
					Count(TEXT("Role.Changli.Action.AirAttackCount"))); Now != LastTrace)
				{
					Trace += FString::Printf(TEXT(" | %.2f %s z=%.0f"), GameNow - StepAt, *Now, Changli->GetActorLocation().Z - StartZ);
					LastTrace = Now;
				}
				if (GameNow - StepAt < 0.75 || CurrentSkill() != 1205006 || !Changli->SkillComponent->IsSkillAcceptingInput())
				{
					if (GameNow - StepAt > 2.5) { Test->AddError(FString::Printf(TEXT("heavy never reached its accept window in the air%s"), *Trace)); return true; }
					return false;
				}
				Test->AddInfo(FString::Printf(TEXT("heavy rose %.0f cm before its accept window%s"), MaxRise, *Trace));
				Test->TestEqual(TEXT("heavy adds two air-attack count layers"), Count(TEXT("Role.Changli.Action.AirAttackCount")), 2);
				Test->TestFalse(TEXT("heavy launches Changli into the air"), Movement->IsMovingOnGround());
				PressAttack();
				Test->TestEqual(TEXT("attack after the heavy in the air: air attack 3 (1205101 + 2)"), CurrentSkill(), 1205103);
				Next(GameNow);
				return false;
			}
			default:
				return true;
			}
		}

		/**
		 * 正对 Boss 头顶下砸：下落循环里胶囊不挡 Pawn（原作结束时 SetCollisionResponse(2, 2) 设回阻挡），
		 * 所以不会顶在 Boss 胶囊上被滑开；向下速度保持很大、很快落地，结束后 Pawn 通道恢复阻挡。
		 */
		bool UpdatePlungeOnBoss(double GameNow)
		{
			UCapsuleComponent* Capsule = Changli->GetCapsuleComponent();
			switch (Step)
			{
			case 3:
				for (TActorIterator<AWuwaCharactorBase> It(World); It; ++It)
					if (It->GetClass()->GetName().Contains(TEXT("BP_Monster_Fuludelisi")))
					{
						BossTopZ = It->GetActorLocation().Z + It->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
						Changli->SetActorLocation(It->GetActorLocation() + FVector(0.0, 0.0, It->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 900.0),
							false, nullptr, ETeleportType::TeleportPhysics);
					}
				Test->TestTrue(TEXT("boss found"), BossTopZ > -1e9);
				Next(GameNow);
				return false;
			case 4:
				if (GameNow - StepAt < 0.2 || Movement->IsMovingOnGround()) return false;
				HoldAttack();
				Test->TestEqual(TEXT("hold in the air above the boss: plunge 1205105"), CurrentSkill(), 1205105);
				Next(GameNow);
				return false;
			case 5:
				MinVz = FMath::Min(MinVz, float(Movement->Velocity.Z));
				if (Montage() == TEXT("AM_AirAttack_Loop"))
				{
					bSawLoop = true;
					Test->TestEqual(TEXT("capsule ignores pawns while plunging"), Capsule->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
				}
				if (!Movement->IsMovingOnGround())
				{
					if (GameNow - StepAt > 3.0) { Test->AddError(FString::Printf(TEXT("plunge onto the boss never landed (z %.0f, fastest fall %.0f)"), Changli->GetActorLocation().Z - BossTopZ, MinVz)); return true; }
					return false;
				}
				Test->AddInfo(FString::Printf(TEXT("plunge onto the boss: fastest fall %.0f cm/s, landed after %.2f s"), MinVz, GameNow - StepAt));
				Test->TestTrue(TEXT("plunge played the fall loop"), bSawLoop);
				Test->TestTrue(TEXT("not slowed by the boss capsule"), MinVz < -2500.f);
				Test->TestTrue(TEXT("landed quickly (not sliding on the boss)"), GameNow - StepAt < 1.5);
				Next(GameNow);
				return false;
			case 6:
				if (CurrentSkill() == 1205105 && GameNow - StepAt < 6.0) return false;
				Test->TestEqual(TEXT("pawn collision restored to block after the plunge"), Capsule->GetCollisionResponseToChannel(ECC_Pawn), ECR_Block);
				return true;
			default:
				return true;
			}
		}

		/** 心火 4 层长按 → 心眼·极 1205402（0.11 秒心火 -4，技能开始协奏 +1000）。 */
		bool UpdateHeavyXinHuo(double GameNow)
		{
			switch (Step)
			{
			case 3:
				ASC->SetNumericAttributeBase(UWuwaAttributeSet::GetSpecialEnergy1Attribute(), 4.f);
				PressAttack();
				Next(GameNow);
				return false;
			case 4:
				if (GameNow - StepAt < 0.3) return false;
				HoldAttack();
				Next(GameNow);
				return false;
			case 5:
				if (CurrentSkill() != 1205402)
				{
					if (GameNow - StepAt > 1.0) { Test->AddError(FString::Printf(TEXT("hold with 4 heart fire did not start 1205402 (current %d)"), CurrentSkill())); return true; }
					return false;
				}
				Test->TestEqual(TEXT("AM_Burst02 playing"), Montage(), FString(TEXT("AM_Burst02")));
				Next(GameNow);
				return false;
			case 6:
				if (GameNow - StepAt < 0.4) return false;
				Test->TestTrue(TEXT("heart fire consumed (1205104004, -4 at 0.11 s)"), Attribute(UWuwaAttributeSet::GetSpecialEnergy1Attribute()) < 0.01f);
				Test->TestEqual(TEXT("still 心眼·极"), CurrentSkill(), 1205402);
				return true;
			default:
				return true;
			}
		}

		bool UpdateDodge(double GameNow)
		{
			switch (Step)
			{
			case 3:
				if (!Jump(GameNow)) return Step == 99;
				Next(GameNow);
				return false;
			case 4:
			{
				if (GameNow - StepAt < 0.3) return false;
				PressDash();
				Test->TestEqual(TEXT("dash in the air without direction: air back dodge 100004"), CurrentSkill(), 100004);
				Test->TestTrue(TEXT("AM_Jump_Second_B playing"), Montage().EndsWith(TEXT("Jump_Second_B")));
				Test->TestEqual(TEXT("dodge forbidden while airborne (1101004005)"), Count(TEXT("State.Combat.DodgeForbid")), 1);
				const int64 Serial = Changli->SkillComponent->GetCurrentSkillData().SkillStartSerial;
				PressDash();
				Test->TestEqual(TEXT("second air dodge refused"), Changli->SkillComponent->GetCurrentSkillData().SkillStartSerial, Serial);
				Next(GameNow);
				return false;
			}
			case 5:
				if (!Movement->IsMovingOnGround() || CurrentSkill() == 100004)
				{
					if (GameNow - StepAt > 4.0) { Test->AddError(FString::Printf(TEXT("air dodge did not end after landing (skill %d, ground %d)"), CurrentSkill(), Movement->IsMovingOnGround())); return true; }
					return false;
				}
				Test->AddInfo(FString::Printf(TEXT("air dodge ended %.2f s after the press"), GameNow - StepAt));
				Test->TestEqual(TEXT("dodge forbid removed on the ground"), Count(TEXT("State.Combat.DodgeForbid")), 0);
				return true;
			default:
				return true;
			}
		}

		FAutomationTestBase* Test;
		EScenario Scenario;
		UWorld* World = nullptr;
		AWuwaPlayerController* PC = nullptr;
		AWuwaCharacter* Changli = nullptr;
		UWuwaMovementComponent* Movement = nullptr;
		UAbilitySystemComponent* ASC = nullptr;
		double StartedAt = -1.0;
		double StepAt = 0.0;
		double GroundZ = 0.0;
		float MaxRise = 0.f;
		float MinVz = 0.f;
		bool bSawLoop = false;
		FString Trace, LastTrace;
		int32 Expected = 0;
		int32 Step = 0;
		double StartZ = 0.0;
		float Concerto = 0.f;
		float HeartFire = 0.f;
		double BossTopZ = -1e10;
		bool bGotTrueSight = false;
	};
}

#define WUWA_AIR_STAGE_TEST(Class, Name, Scenario) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class, Name, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
	bool Class::RunTest(const FString& Parameters) \
	{ \
		if (!AutomationOpenMap(TEXT("/Game/Environment/DaKa/Maps/L_DaKa"))) return false; \
		ADD_LATENT_AUTOMATION_COMMAND(WuwaChangliAirStageTest::FScenario(this, WuwaChangliAirStageTest::EScenario::Scenario)); \
		ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); \
		return true; \
	}

WUWA_AIR_STAGE_TEST(FWuwaChangliAirChainTest, "Wuwa.Combat.RoleSkills.ChangliAirChain", Chain)
WUWA_AIR_STAGE_TEST(FWuwaChangliTrueSightLeapTest, "Wuwa.Combat.RoleSkills.ChangliTrueSightLeap", Leap)
WUWA_AIR_STAGE_TEST(FWuwaChangliAirDodgeTest, "Wuwa.Combat.RoleSkills.ChangliAirDodge", Dodge)
WUWA_AIR_STAGE_TEST(FWuwaChangliSkillFromAirTest, "Wuwa.Combat.RoleSkills.ChangliSkillFromAir", SkillAir)
WUWA_AIR_STAGE_TEST(FWuwaChangliSkillFromGroundTest, "Wuwa.Combat.RoleSkills.ChangliSkillFromGround", SkillGround)
WUWA_AIR_STAGE_TEST(FWuwaChangliBurstTest, "Wuwa.Combat.RoleSkills.ChangliBurst", Burst)
WUWA_AIR_STAGE_TEST(FWuwaChangliHeavyTest, "Wuwa.Combat.RoleSkills.ChangliHeavy", Heavy)
WUWA_AIR_STAGE_TEST(FWuwaChangliHeavyXinHuoTest, "Wuwa.Combat.RoleSkills.ChangliHeavyXinHuo", HeavyXinHuo)
WUWA_AIR_STAGE_TEST(FWuwaChangliPlungeOnBossTest, "Wuwa.Combat.RoleSkills.ChangliPlungeOnBoss", PlungeOnBoss)

#endif
