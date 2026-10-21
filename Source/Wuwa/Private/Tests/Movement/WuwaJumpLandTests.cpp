#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Common/WuwaGameTags.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Formation/WuwaFormationComponent.h"
#include "Game/Input/WuwaInputRouterComponent.h"
#include "Game/Input/WuwaInputTypes.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimInstance.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputIntentComponent.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"
#include "AbilitySystemComponent.h"
#include "Engine/DataTable.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

namespace WuwaJumpLandTest
{
	UWorld* FindPlayWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) return Context.World();
		return nullptr;
	}

	void Press(AWuwaPlayerController* PC, const FGameplayTag& Input, const FGameplayTag& Route)
	{
		FWuwaInputEvent Event;
		Event.InputTag = Input;
		Event.RouteTag = Route;
		Event.Timestamp = PC->GetWorld()->GetTimeSeconds();
		Event.Phase = EWuwaInputPhase::Pressed;
		PC->GetInputRouter()->DispatchInput(Event);
		Event.Phase = EWuwaInputPhase::Released;
		PC->GetInputRouter()->DispatchInput(Event);
	}

	void PressJump(AWuwaPlayerController* PC)
	{
		Press(PC, FWuwaGameTags::Get().Player_Common_Movement_Jump, FWuwaGameTags::Get().Input_Route_Movement);
	}

	/** 一次跳跃从起跳到落地完的记录。 */
	struct FJumpRecord
	{
		double PressedAt = 0.0, TakeoffEndAt = 0.0, LandedAt = 0.0;
		float StartZ = 0.f, MaxRise = 0.f, LandingSpeed = 0.f;
		FVector StartLocation = FVector::ZeroVector, TakeoffEndLocation = FVector::ZeroVector;
		bool bSawJumpStart = false, bSawLand = false, bSawTakeoffWindow = false;
		FName LandedState;
	};

	/**
	 * 大卡：
	 *  1. 原地跳：起跳动画根运动升起约 219 cm（Jump_Walk），落地后进 Land（低速档、轻落地），没有输入播完回 Ground，根运动模式恢复成起跳前的。
	 *  2. 助跑跳：横轴 = 速度 / 900，起跳期间向前位移，落地有输入立刻回 Ground（低速档出口）。
	 *  3. 普攻（等级 2）开始 0.1 秒按跳跃：跳跃（等级 8）打断普攻，技能结束、蒙太奇停掉。
	 *  4. 冲刺（等级 133）刚开始按跳跃：打断不了，不起跳。
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
			if (!Formation || !Formation->IsFormationReady() || Formation->GetMembers().Num() < 1) return false;
			AWuwaCharacter* Role = Formation->GetMembers()[0];
			if (PC->GetPawn() != Role) return false;
			UWuwaMovementComponent* Movement = Role->GetWuwaMovementComponent();
			UWuwaAnimInstance* Anim = Cast<UWuwaAnimInstance>(Role->GetMesh()->GetAnimInstance());
			if (!Movement || !Anim) return false;
			const FName State = Anim->GetDebugStateName(WuwaJumpLand::Locomotion);
			const double GameNow = World->GetTimeSeconds();
			const FWuwaGameTags& Tags = FWuwaGameTags::Get();

			switch (Step)
			{
			case 0:
			{
				for (TActorIterator<AWuwaCharactorBase> It(World); It; ++It)
					if (It->GetClass()->GetName().Contains(TEXT("BP_Monster_Fuludelisi"))) Boss = *It;
				if (!Boss) return false;
				// 站到 Boss 正前方 1500 cm、面向 Boss：后面助跑跳朝 Boss 跑，场地够长。
				const FVector Forward = Boss->GetActorForwardVector();
				Role->SetActorLocation(Boss->GetActorLocation() + Forward * 1500.0, false, nullptr, ETeleportType::TeleportPhysics);
				Role->SetActorRotation(FRotator(0.0, (-Forward).Rotation().Yaw, 0.0));
				Next(GameNow);
				return false;
			}
			case 1:
			{
				if (GameNow - StepAt < 1.5 || !Movement->IsMovingOnGround()) return false;
				Test->TestTrue(TEXT("has the jump/land graph"), Anim->HasJumpLandGraph());
				ModeBeforeJump = Anim->RootMotionMode.GetValue();
				Record = FJumpRecord();
				Record.StartZ = Role->GetActorLocation().Z;
				Record.StartLocation = Role->GetActorLocation();
				Record.PressedAt = GameNow;
				PressJump(PC);
				Test->TestTrue(TEXT("standing jump: takeoff root motion started"), Movement->IsJumpRootMotionActive());
				Test->TestTrue(TEXT("standing jump: falling mode without initial speed"), Movement->IsFalling());
				Test->TestEqual(TEXT("standing jump: root motion from everything during takeoff"),
					Anim->RootMotionMode.GetValue(), ERootMotionMode::RootMotionFromEverything);
				Next(GameNow);
				return false;
			}
			case 2:
			case 6:
			{
				// 跟踪起跳 → 落地。
				Record.MaxRise = FMath::Max(Record.MaxRise, float(Role->GetActorLocation().Z - Record.StartZ));
				Record.bSawJumpStart |= State == WuwaJumpLand::JumpStart;
				Record.bSawTakeoffWindow |= Movement->IsJumpRootMotionActive();
				// 空中按普攻（长离出空中普攻 1205101、不出地面普攻）见 Wuwa.Combat.RoleSkills.ChangliAirChain；这里不按，免得延长滞空影响下面的时长检查。
				if (Record.TakeoffEndAt == 0.0 && !Movement->IsJumpRootMotionActive())
				{
					Record.TakeoffEndAt = GameNow;
					Record.TakeoffEndLocation = Role->GetActorLocation();
				}
				if (Record.LandedAt == 0.0 && Movement->IsMovingOnGround() && GameNow > Record.PressedAt)
				{
					Record.LandedAt = GameNow;
					Record.LandingSpeed = Movement->GetLastLandingVelocity().Z;
				}
				if (Record.LandedAt > 0.0)
				{
					Record.bSawLand |= State == WuwaJumpLand::Land;
					if (State == WuwaJumpLand::Land || State == WuwaJumpLand::Ground) Record.LandedState = State;
					if (GameNow - Record.LandedAt >= 0.15) { Next(GameNow); }
				}
				if (GameNow - Record.PressedAt > 4.0) { Test->AddError(TEXT("Jump never landed.")); return true; }
				return false;
			}
			case 3:
			{
				const double Air = Record.LandedAt - Record.PressedAt;
				Test->AddInfo(FString::Printf(TEXT("standing jump: rise %.0f cm, takeoff window %.2f s, air %.2f s, landing vz %.0f, index %d, heavy %d, state %s"),
					Record.MaxRise, Record.TakeoffEndAt - Record.PressedAt, Air, Record.LandingSpeed, Anim->LandSpeedIndex, Anim->bLandHeavy, *Record.LandedState.ToString()));
				Test->TestTrue(TEXT("standing jump entered JumpStart"), Record.bSawJumpStart);
				Test->TestFalse(TEXT("buffered air attack does not fire on landing"), IsValid(Role->SkillComponent->GetCurrentSkillData().ActiveAbility));
				Test->TestTrue(TEXT("standing jump rises like Jump_Walk root motion (~219 cm)"), Record.MaxRise > 180.f && Record.MaxRise < 250.f);
				Test->TestTrue(TEXT("standing jump air time about the 0.7 s takeoff"), Air > 0.5 && Air < 0.95);
				Test->TestTrue(TEXT("standing jump: landing plays Land (low tier, light)"), Record.bSawLand && Anim->LandSpeedIndex == 3 && !Anim->bLandHeavy);
				Test->TestEqual(TEXT("root motion mode restored after takeoff"), Anim->RootMotionMode.GetValue(), ModeBeforeJump);
				Next(GameNow);
				return false;
			}
			case 4:
			{
				// 没有输入：轻落地播完（剩 0.1 秒）回地面。
				if (State == WuwaJumpLand::Ground)
				{
					Test->AddInfo(FString::Printf(TEXT("standing jump: Land -> Ground after %.2f s"), GameNow - Record.LandedAt));
					Test->TestTrue(TEXT("no input: Land_Light (2.0 s) plays out before Ground"), GameNow - Record.LandedAt > 1.5);
					Next(GameNow);
					return false;
				}
				if (GameNow - StepAt > 4.0) { Test->AddError(TEXT("Land never finished into Ground.")); return true; }
				return false;
			}
			case 5:
			{
				// 助跑 1 秒再跳。
				if (StepAt == GameNow || Role->InputIntent->GetMoveIntent().bHasInput == false) Role->InputIntent->SetMoveAxis(FVector2D(0.0, 1.0));
				if (GameNow - StepAt < 1.0) return false;
				Speed = Movement->Velocity.Size2D();
				Record = FJumpRecord();
				Record.StartZ = Role->GetActorLocation().Z;
				Record.StartLocation = Role->GetActorLocation();
				Record.PressedAt = GameNow;
				PressJump(PC);
				Test->TestTrue(TEXT("running jump: takeoff started"), Movement->IsJumpRootMotionActive());
				Next(GameNow);
				return false;
			}
			case 7:
			{
				const float Forward = FVector::Dist2D(Record.StartLocation, Record.TakeoffEndLocation);
				Test->AddInfo(FString::Printf(TEXT("running jump: speed %.0f, blend %.2f, right foot %d, rise %.0f cm, takeoff forward %.0f cm, landing vz %.0f, state %s"),
					Speed, Anim->JumpBlend, Anim->bJumpRightFoot, Record.MaxRise, Forward, Record.LandingSpeed, *Record.LandedState.ToString()));
				Test->TestTrue(TEXT("running jump blend = speed / 900"), FMath::IsNearlyEqual(Anim->JumpBlend, FMath::Clamp(Speed / 900.f, 0.f, 1.f), 0.05f));
				Test->TestTrue(TEXT("running jump moves forward with the takeoff root motion"), Forward > 150.f);
				Test->TestTrue(TEXT("running jump rises"), Record.MaxRise > 150.f);
				// 有输入：低速档立刻出落地（同一帧或下一帧回到 Ground）。
				Test->TestTrue(TEXT("landing with input goes straight back to Ground"), State == WuwaJumpLand::Ground);
				Role->InputIntent->SetMoveAxis(FVector2D::ZeroVector);
				Next(GameNow);
				return false;
			}
			case 8:
			{
				// 等停稳、落地动画结束再普攻。
				if (GameNow - StepAt < 3.0 || State != WuwaJumpLand::Ground) return false;
				Press(PC, Tags.Input_Combat_Attack, Tags.Input_Route_Ability);
				Next(GameNow);
				return false;
			}
			case 9:
			{
				const FWuwaSkillData Skill = Role->SkillComponent->GetCurrentSkillData();
				if (!IsValid(Skill.ActiveAbility) || GameNow - StepAt < 0.1)
				{
					if (GameNow - StepAt > 2.0) { Test->AddError(TEXT("Attack did not start.")); return true; }
					return false;
				}
				Test->AddInfo(FString::Printf(TEXT("attack %d level %d accept %d readyEnd %d"), Skill.SkillId, Skill.InterruptLevel, Skill.bSkillAcceptInput, Skill.bMainSkillReadyEnd));
				Test->TestEqual(TEXT("attack interrupt level imported from the original (2)"), Skill.InterruptLevel, 2);
				// 原作 WalkOffLedge = 真（地面普攻）：技能期间边缘保护，不能走出平台边缘。
				Test->TestFalse(TEXT("ledge protection during the attack (bCanWalkOffLedges off)"), Movement->bCanWalkOffLedges);
				Test->TestEqual(TEXT("one ledge protection record"), Movement->GetWalkOffProtectCount(), 1);
				PressJump(PC);
				Test->TestEqual(TEXT("attack ended: ledge protection returned"), Movement->GetWalkOffProtectCount(), 0);
				Test->TestTrue(TEXT("jump (level 8) interrupts the attack"), Movement->IsJumpRootMotionActive());
				Test->TestFalse(TEXT("attack skill ended"), IsValid(Role->SkillComponent->GetCurrentSkillData().ActiveAbility));
				Test->TestNull(TEXT("montage stopped (Montage_Stop(0))"), Role->GetMesh()->GetAnimInstance()->GetCurrentActiveMontage());
				Next(GameNow);
				return false;
			}
			case 10:
			{
				if (GameNow - StepAt < 3.5 || !Movement->IsMovingOnGround() || State != WuwaJumpLand::Ground) return false;
				Test->TestTrue(TEXT("walking off ledges allowed again one frame after the skill"), Movement->bCanWalkOffLedges);
				Press(PC, Tags.Input_Combat_Dash, Tags.Input_Route_Ability);
				Next(GameNow);
				return false;
			}
			case 11:
			{
				const FWuwaSkillData Skill = Role->SkillComponent->GetCurrentSkillData();
				if (!IsValid(Skill.ActiveAbility))
				{
					if (GameNow - StepAt > 2.0) { Test->AddError(TEXT("Dash did not start.")); return true; }
					return false;
				}
				PressJump(PC);
				Test->TestFalse(TEXT("jump cannot interrupt a fresh dash (level 133)"), Movement->IsJumpRootMotionActive());
				Test->TestTrue(TEXT("dash keeps running"), IsValid(Role->SkillComponent->GetCurrentSkillData().ActiveAbility));
				return true;
			}
			default:
				return true;
			}
		}

	private:
		void Next(double GameNow) { ++Step; StepAt = GameNow; }
		FAutomationTestBase* Test;
		AActor* Boss = nullptr;
		FJumpRecord Record;
		float Speed = 0.f;
		ERootMotionMode::Type ModeBeforeJump = ERootMotionMode::RootMotionFromMontagesOnly;
		double StartedAt = -1.0;
		double StepAt = 0.0;
		int32 Step = 0;
	};
}

namespace WuwaJumpLandTest
{
	/**
	 * 空中蒙太奇根运动的 Z 要生效（原作 Kuro 根运动：空中有根运动时速度全听根运动，空中普攻的滞空 / 上挑 / 下落都在动画 Z 里）。
	 * 大卡：起跳 → 起跳播完、按重力下落时直接激活普攻 1 的 GA（绕过只限地面的输入规则）→ 技能期间竖直速度等于蒙太奇根运动的速度，
	 * 不会按重力一路加速（自由落体 0.25 秒会再快约 245 cm/s）。
	 */
	class FAirRootMotion final : public IAutomationLatentCommand
	{
	public:
		explicit FAirRootMotion(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			if (Now - StartedAt > 300.0) { Test->AddError(FString::Printf(TEXT("Timed out at step %d."), Step)); return true; }
			UWorld* World = FindPlayWorld();
			AWuwaPlayerController* PC = World ? Cast<AWuwaPlayerController>(World->GetFirstPlayerController()) : nullptr;
			UWuwaFormationComponent* Formation = PC ? PC->FormationComponent.Get() : nullptr;
			if (!Formation || !Formation->IsFormationReady() || Formation->GetMembers().Num() < 1) return false;
			AWuwaCharacter* Role = Formation->GetMembers()[0];
			if (PC->GetPawn() != Role) return false;
			UWuwaMovementComponent* Movement = Role->GetWuwaMovementComponent();
			const double GameNow = World->GetTimeSeconds();
			switch (Step)
			{
			case 0:
			{
				AActor* Boss = nullptr;
				for (TActorIterator<AWuwaCharactorBase> It(World); It; ++It)
					if (It->GetClass()->GetName().Contains(TEXT("BP_Monster_Fuludelisi"))) Boss = *It;
				if (!Boss) return false;
				const FVector Forward = Boss->GetActorForwardVector();
				Role->SetActorLocation(Boss->GetActorLocation() + Forward * 1200.0, false, nullptr, ETeleportType::TeleportPhysics);
				Role->SetActorRotation(FRotator(0.0, (-Forward).Rotation().Yaw, 0.0));
				Step = 1; StepAt = GameNow;
				return false;
			}
			case 1:
				if (GameNow - StepAt < 1.5 || !Movement->IsMovingOnGround()) return false;
				PressJump(PC);
				Test->TestTrue(TEXT("jump started"), Movement->IsJumpRootMotionActive());
				Step = 2; StepAt = GameNow;
				return false;
			case 2:
			{
				// 起跳一过最高点就放：普攻 1 的蒙太奇开始后起跳窗口关闭，空中位移改由蒙太奇根运动决定。
				if (Movement->Velocity.Z > -50.f || GameNow - StepAt < 0.4)
				{
					if (GameNow - StepAt > 2.0) { Test->AddError(TEXT("Never started descending.")); return true; }
					return false;
				}
				const FWuwaSkillInfo* Info = nullptr;
				for (const auto& Pair : Role->SkillTable->GetRowMap())
				{
					const FWuwaSkillInfo* Row = reinterpret_cast<const FWuwaSkillInfo*>(Pair.Value);
					if (Row->Ability && Row->SkillTags.HasTag(FWuwaGameTags::Get().Skill_Type_NormalAttack)) { Info = Row; break; }
				}
				if (!Test->TestNotNull(TEXT("role has a normal attack in its skill table"), Info)) return true;
				VzBefore = Movement->Velocity.Z;
				Test->TestTrue(TEXT("attack GA activates in the air"), Role->GetAbilitySystemComponent()->TryActivateAbilityByClass(Info->Ability));
				Step = 3; StepAt = GameNow;
				return false;
			}
			case 3:
			{
				UAnimInstance* Anim = Role->GetMesh()->GetAnimInstance();
				if (GameNow - StepAt < 0.25) return false;
				const float Vz = Movement->Velocity.Z;
				Test->AddInfo(FString::Printf(TEXT("air attack: vz %.0f -> %.0f after 0.25 s, falling %d, montage %s, window %d"),
					VzBefore, Vz, Movement->IsFalling(), *GetNameSafe(Anim ? Anim->GetCurrentActiveMontage() : nullptr), Movement->IsJumpRootMotionActive()));
				Test->TestNotNull(TEXT("attack montage playing in the air"), Anim ? Anim->GetCurrentActiveMontage() : nullptr);
				Test->TestFalse(TEXT("takeoff window ended when the montage started"), Movement->IsJumpRootMotionActive());
				if (Movement->IsFalling())
					Test->TestTrue(TEXT("vertical speed follows the montage root motion, not gravity"), Vz > VzBefore - 100.f);
				return true;
			}
			default:
				return true;
			}
		}

	private:
		FAutomationTestBase* Test;
		double StartedAt = -1.0;
		double StepAt = 0.0;
		float VzBefore = 0.f;
		int32 Step = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaAirRootMotionTest, "Wuwa.Movement.AirRootMotionZ",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaAirRootMotionTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Environment/DaKa/Maps/L_DaKa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(WuwaJumpLandTest::FAirRootMotion(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaJumpLandTest, "Wuwa.Movement.JumpLand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaJumpLandTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Environment/DaKa/Maps/L_DaKa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(WuwaJumpLandTest::FCheck(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
