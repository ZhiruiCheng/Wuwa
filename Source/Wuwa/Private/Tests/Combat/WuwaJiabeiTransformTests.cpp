#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "AbilitySystemComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Camera/WuwaSequenceCameraSubsystem.h"
#include "Game/Common/WuwaGameTags.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Formation/WuwaFormationComponent.h"
#include "Game/Input/WuwaInputRouterComponent.h"
#include "Game/Input/WuwaInputTypes.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

/**
 * 嘉贝变身（原作强化 E 1208101 + 变身 buff 1208001001），输入都走输入路由，站在 Boss 前 4 米：
 *  Charge：没变身时普攻命中 Boss → 能量条 SpecialEnergy1 涨（子弹 能量恢复类GE 1208003305 等），不碰弹药 SpecialEnergy3。
 *  Enhanced：SpecialEnergy1 满（100）按 E → 强化 E；开始 buff：弹药 = 能量（100）、能量清空、变身中 Tag；
 *            强化 E 结束、连段窗口过了再按普攻 → EX 普攻 1，接招窗口里再按 → EX 普攻 2；长按 → EX 重击 1。
 *  ExitOnEmpty：变身中把弹药扣到 0 → 0.1 秒后（1208001002 → 1208001032）变身中 Tag 没了，再按普攻回到普攻 1。
 *  JumpKick：没满按 E → E 跳踢 1208007，踢中 Boss（子弹命中发 Role.Jiabei.Event.SkillBranch）接后半段 AM_Jiabei_Attack_Jump_next。
 *  Burst：共鸣能量满按大招 → 1208301，共鸣能量清空，特写镜头在播。
 *  AirPlunge：空中按普攻 → 下落攻击 1208008（起手 → 下落循环 → 落地段 AM_Jiabei_AirAttack_End）。
 *  ExCamera：变身后连按普攻打到 EX 普攻 3（模型跳到胶囊上方 2 米）：镜头修改通知把镜头拉远（原作 10 米、俯 20°、中心抬 130 cm），
 *            招式结束后镜头混回原来的距离。
 */
namespace WuwaJiabeiTransformTest
{
	enum class EScenario : uint8 { Charge, Enhanced, ExitOnEmpty, JumpKick, Burst, AirPlunge, ExCamera };

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
			case EScenario::Charge: return UpdateCharge(GameNow);
			case EScenario::Enhanced: return UpdateEnhanced(GameNow);
			case EScenario::ExitOnEmpty: return UpdateExitOnEmpty(GameNow);
			case EScenario::JumpKick: return UpdateJumpKick(GameNow);
			case EScenario::AirPlunge: return UpdateAirPlunge(GameNow);
			case EScenario::ExCamera: return UpdateExCamera(GameNow);
			default: return UpdateBurst(GameNow);
			}
		}

	private:
		int32 CurrentSkill() const
		{
			const FWuwaSkillData Skill = Jiabei->SkillComponent->GetCurrentSkillData();
			return IsValid(Skill.ActiveAbility) ? Skill.SkillId : 0;
		}
		bool Accepting() const { return Jiabei->SkillComponent->IsSkillAcceptingInput(); }
		bool Transformed() const { return ASC && ASC->HasMatchingGameplayTag(Tag(TEXT("Role.Jiabei.State.Transformed"))); }
		float Get(const FGameplayAttribute& Attribute) const { bool bFound = false; return ASC->GetGameplayAttributeValue(Attribute, bFound); }
		void Set(const FGameplayAttribute& Attribute, float Value) const { ASC->SetNumericAttributeBase(Attribute, Value); }
		void Next(double GameNow) { ++Step; StepAt = GameNow; }
		void Send(const FGameplayTag& InputTag, EWuwaInputPhase Phase = EWuwaInputPhase::Pressed)
		{
			FWuwaInputEvent Event;
			Event.InputTag = InputTag;
			Event.RouteTag = FWuwaGameTags::Get().Input_Route_Ability;
			Event.Phase = Phase;
			Event.Timestamp = World->GetTimeSeconds();
			PC->GetInputRouter()->DispatchInput(Event);
		}
		void Attack(EWuwaInputPhase Phase = EWuwaInputPhase::Pressed) { Send(FWuwaGameTags::Get().Input_Combat_Attack, Phase); }
		FString PlayingMontage() const
		{
			const UAnimInstance* Anim = Jiabei->GetMesh()->GetAnimInstance();
			const UAnimMontage* Montage = Anim ? Anim->GetCurrentActiveMontage() : nullptr;
			return Montage ? Montage->GetName() : FString();
		}
		/** 变身：能量条满了按 E。 */
		void StartEnhanced()
		{
			Set(UWuwaAttributeSet::GetSpecialEnergy1Attribute(), Get(UWuwaAttributeSet::GetMaxSpecialEnergy1Attribute()));
			Send(Tag(TEXT("Input.Combat.Skill")));
		}
		bool WaitIdle(double GameNow, double Timeout, const TCHAR* What)
		{
			if (CurrentSkill() == 0) return true;
			if (GameNow - StepAt > Timeout) { Test->AddError(FString::Printf(TEXT("%s never ended (current %d)"), What, CurrentSkill())); bFailed = true; }
			return false;
		}

		bool UpdateCharge(double GameNow)
		{
			switch (Step)
			{
			case 3:
				Test->TestEqual(TEXT("energy bar max (inferred 100)"), Get(UWuwaAttributeSet::GetMaxSpecialEnergy1Attribute()), 100.f);
				Set(UWuwaAttributeSet::GetSpecialEnergy1Attribute(), 0.f);
				Attack();
				Test->TestEqual(TEXT("press: attack 1"), CurrentSkill(), 1208001);
				Next(GameNow);
				return false;
			case 4:
				if (Get(UWuwaAttributeSet::GetSpecialEnergy1Attribute()) <= 0.f)
				{
					if (GameNow - StepAt > 2.0) { Test->AddError(TEXT("attack 1 hit never charged the energy bar")); return true; }
					return false;
				}
				Test->AddInfo(FString::Printf(TEXT("SpecialEnergy1 %.0f after %.2f s"), Get(UWuwaAttributeSet::GetSpecialEnergy1Attribute()), GameNow - StepAt));
				Test->TestFalse(TEXT("not transformed by charging"), Transformed());
				return true;
			default:
				return true;
			}
		}

		bool UpdateEnhanced(double GameNow)
		{
			switch (Step)
			{
			case 3:
				StartEnhanced();
				Test->TestEqual(TEXT("E with a full bar: enhanced E"), CurrentSkill(), 1208101);
				Next(GameNow);
				return false;
			case 4:
				if (GameNow - StepAt < 0.1) return false;
				Test->AddInfo(FString::Printf(TEXT("after enhanced E: SE1 %.1f / %.1f, SE3 %.1f / %.1f"),
					Get(UWuwaAttributeSet::GetSpecialEnergy1Attribute()), Get(UWuwaAttributeSet::GetMaxSpecialEnergy1Attribute()),
					Get(UWuwaAttributeSet::GetSpecialEnergy3Attribute()), Get(UWuwaAttributeSet::GetMaxSpecialEnergy3Attribute())));
				Test->TestTrue(TEXT("transformed"), Transformed());
				Test->TestEqual(TEXT("ammo = energy (100)"), Get(UWuwaAttributeSet::GetSpecialEnergy3Attribute()), 100.f);
				Test->TestEqual(TEXT("energy bar emptied"), Get(UWuwaAttributeSet::GetSpecialEnergy1Attribute()), 0.f);
				Next(GameNow);
				return false;
			case 5:
				if (!WaitIdle(GameNow, 10.0, TEXT("enhanced E"))) return bFailed;
				Next(GameNow);
				return false;
			case 6:
				if (GameNow - StepAt < 1.0) return false;
				Attack();
				Test->TestEqual(TEXT("transformed press after the combo window: EX attack 1"), CurrentSkill(), 1208011);
				Next(GameNow);
				return false;
			case 7:
				if (!Accepting())
				{
					if (GameNow - StepAt > 3.0) { Test->AddError(TEXT("EX attack 1 never opened its accept window")); return true; }
					return false;
				}
				Attack();
				Test->TestEqual(TEXT("press in EX attack 1's window: EX attack 2"), CurrentSkill(), 1208012);
				Next(GameNow);
				return false;
			case 8:
				if (!WaitIdle(GameNow, 10.0, TEXT("EX attack 2"))) return bFailed;
				Next(GameNow);
				return false;
			case 9:
				if (GameNow - StepAt < 1.0) return false;
				Attack(EWuwaInputPhase::Held);
				Test->TestEqual(TEXT("transformed hold: EX heavy 1"), CurrentSkill(), 1208020);
				Test->TestTrue(TEXT("ammo spent by EX hits"), Get(UWuwaAttributeSet::GetSpecialEnergy3Attribute()) < 100.f);
				Test->AddInfo(FString::Printf(TEXT("ammo left %.0f"), Get(UWuwaAttributeSet::GetSpecialEnergy3Attribute())));
				return true;
			default:
				return true;
			}
		}

		bool UpdateExitOnEmpty(double GameNow)
		{
			switch (Step)
			{
			case 3:
				StartEnhanced();
				Next(GameNow);
				return false;
			case 4:
				if (GameNow - StepAt < 0.2) return false;
				Test->TestTrue(TEXT("transformed"), Transformed());
				Set(UWuwaAttributeSet::GetSpecialEnergy3Attribute(), 0.f);
				Next(GameNow);
				return false;
			case 5:
				if (Transformed())
				{
					if (GameNow - StepAt > 1.0) { Test->AddError(TEXT("still transformed 1 s after the ammo ran out")); return true; }
					return false;
				}
				Test->AddInfo(FString::Printf(TEXT("transform ended %.2f s after the ammo ran out"), GameNow - StepAt));
				Next(GameNow);
				return false;
			case 6:
				if (!WaitIdle(GameNow, 10.0, TEXT("enhanced E"))) return bFailed;
				Next(GameNow);
				return false;
			case 7:
				if (GameNow - StepAt < 1.0) return false;
				Attack();
				Test->TestEqual(TEXT("after the transform: attack 1"), CurrentSkill(), 1208001);
				return true;
			default:
				return true;
			}
		}

		bool UpdateJumpKick(double GameNow)
		{
			switch (Step)
			{
			case 3:
				Set(UWuwaAttributeSet::GetSpecialEnergy1Attribute(), 0.f);
				Send(Tag(TEXT("Input.Combat.Skill")));
				Test->TestEqual(TEXT("E without a full bar: jump kick"), CurrentSkill(), 1208007);
				Next(GameNow);
				return false;
			case 4:
				if (PlayingMontage() != TEXT("AM_Jiabei_Attack_Jump_next"))
				{
					if (CurrentSkill() != 1208007 || GameNow - StepAt > 2.0)
					{
						Test->AddError(FString::Printf(TEXT("the kick never branched into the second half (current %d, montage %s)"), CurrentSkill(), *PlayingMontage()));
						return true;
					}
					return false;
				}
				Test->AddInfo(FString::Printf(TEXT("kick hit -> second half after %.2f s"), GameNow - StepAt));
				Test->TestEqual(TEXT("still the jump kick skill"), CurrentSkill(), 1208007);
				return true;
			default:
				return true;
			}
		}

		bool UpdateAirPlunge(double GameNow)
		{
			switch (Step)
			{
			case 3:
				Jiabei->SetActorLocation(Jiabei->GetActorLocation() + FVector(0.0, 0.0, 600.0), false, nullptr, ETeleportType::TeleportPhysics);
				Next(GameNow);
				return false;
			case 4:
				if (GameNow - StepAt < 0.1) return false;
				Test->TestFalse(TEXT("in the air"), Jiabei->GetWuwaMovementComponent()->IsMovingOnGround());
				Attack();
				Test->TestEqual(TEXT("press in the air: plunge"), CurrentSkill(), 1208008);
				Next(GameNow);
				return false;
			case 5:
				if (PlayingMontage() != TEXT("AM_Jiabei_AirAttack_End"))
				{
					if (CurrentSkill() != 1208008 || GameNow - StepAt > 3.0)
					{
						Test->AddError(FString::Printf(TEXT("plunge never landed (current %d, montage %s)"), CurrentSkill(), *PlayingMontage()));
						return true;
					}
					return false;
				}
				Test->AddInfo(FString::Printf(TEXT("plunge landed after %.2f s"), GameNow - StepAt));
				Test->TestTrue(TEXT("on the ground"), Jiabei->GetWuwaMovementComponent()->IsMovingOnGround());
				return true;
			default:
				return true;
			}
		}

		float CameraDistance() const
		{
			const APlayerCameraManager* Camera = PC ? PC->PlayerCameraManager.Get() : nullptr;
			return Camera ? FVector::Dist(Camera->GetCameraLocation(), Jiabei->GetActorLocation()) : 0.f;
		}

		bool UpdateExCamera(double GameNow)
		{
			switch (Step)
			{
			case 3:
				BaseDistance = CameraDistance();
				StartEnhanced();
				Next(GameNow);
				return false;
			case 4:
				// 一直补满弹药、每 0.3 秒按一次普攻，直到 EX 普攻 3 开始
				Set(UWuwaAttributeSet::GetSpecialEnergy3Attribute(), 100.f);
				if (CurrentSkill() == 1208013)
				{
					Next(GameNow);
					return false;
				}
				if (GameNow - StepAt > 15.0) { Test->AddError(FString::Printf(TEXT("never reached EX attack 3 (current %d)"), CurrentSkill())); return true; }
				if (GameNow - LastPress >= 0.3 && GameNow - StepAt > 1.5)
				{
					Attack();
					LastPress = GameNow;
				}
				return false;
			case 5:
			{
				Set(UWuwaAttributeSet::GetSpecialEnergy3Attribute(), 100.f);
				MaxDistance = FMath::Max(MaxDistance, CameraDistance());
				if (GameNow - StepAt < 1.5) return false;
				Test->AddInfo(FString::Printf(TEXT("camera distance: base %.0f, max during EX attack 3 %.0f"), BaseDistance, MaxDistance));
				Test->TestTrue(TEXT("EX attack 3 pulls the camera out (original arm length 1000)"), MaxDistance > BaseDistance + 300.f);
				Next(GameNow);
				return false;
			}
			case 6:
				if (CurrentSkill() != 0 || GameNow - StepAt < 2.5)
				{
					if (GameNow - StepAt > 15.0) { Test->AddError(TEXT("EX attack 3 never ended")); return true; }
					return false;
				}
				Test->AddInfo(FString::Printf(TEXT("camera distance after the skill: %.0f"), CameraDistance()));
				Test->TestTrue(TEXT("camera blends back after the skill"), FMath::Abs(CameraDistance() - BaseDistance) < 80.f);
				return true;
			default:
				return true;
			}
		}

		bool UpdateBurst(double GameNow)
		{
			switch (Step)
			{
			case 3:
				Set(UWuwaAttributeSet::GetEnergyAttribute(), Get(UWuwaAttributeSet::GetMaxEnergyAttribute()));
				Send(Tag(TEXT("Input.Combat.Ultimate")));
				Test->TestEqual(TEXT("burst"), CurrentSkill(), 1208301);
				Next(GameNow);
				return false;
			case 4:
			{
				if (GameNow - StepAt < 0.2) return false;
				Test->TestEqual(TEXT("burst spends the resonance energy"), Get(UWuwaAttributeSet::GetEnergyAttribute()), 0.f);
				const UWuwaSequenceCameraSubsystem* Camera = World->GetSubsystem<UWuwaSequenceCameraSubsystem>();
				Test->TestTrue(TEXT("burst close-up sequence playing"), Camera && Camera->IsPlaying());
				return true;
			}
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
		bool bFailed = false;
		double LastPress = 0.0;
		float BaseDistance = 0.f;
		float MaxDistance = 0.f;
	};
}

#define WUWA_JIABEI_TRANSFORM_TEST(Class, Name, Scenario) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class, Name, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
	bool Class::RunTest(const FString& Parameters) \
	{ \
		if (!AutomationOpenMap(TEXT("/Game/Environment/DaKa/Maps/L_DaKa"))) return false; \
		ADD_LATENT_AUTOMATION_COMMAND(WuwaJiabeiTransformTest::FScenario(this, WuwaJiabeiTransformTest::EScenario::Scenario)); \
		ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); \
		return true; \
	}

WUWA_JIABEI_TRANSFORM_TEST(FWuwaJiabeiChargeTest, "Wuwa.Combat.RoleSkills.JiabeiCharge", Charge)
WUWA_JIABEI_TRANSFORM_TEST(FWuwaJiabeiEnhancedTest, "Wuwa.Combat.RoleSkills.JiabeiEnhancedSkill", Enhanced)
WUWA_JIABEI_TRANSFORM_TEST(FWuwaJiabeiExitOnEmptyTest, "Wuwa.Combat.RoleSkills.JiabeiTransformExit", ExitOnEmpty)
WUWA_JIABEI_TRANSFORM_TEST(FWuwaJiabeiJumpKickTest, "Wuwa.Combat.RoleSkills.JiabeiJumpKick", JumpKick)
WUWA_JIABEI_TRANSFORM_TEST(FWuwaJiabeiBurstTest, "Wuwa.Combat.RoleSkills.JiabeiBurst", Burst)
WUWA_JIABEI_TRANSFORM_TEST(FWuwaJiabeiAirPlungeTest, "Wuwa.Combat.RoleSkills.JiabeiAirPlunge", AirPlunge)
WUWA_JIABEI_TRANSFORM_TEST(FWuwaJiabeiExCameraTest, "Wuwa.Combat.RoleSkills.JiabeiExCamera", ExCamera)

#endif
