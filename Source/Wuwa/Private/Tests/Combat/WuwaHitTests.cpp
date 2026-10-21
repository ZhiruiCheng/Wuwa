#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Game/NewWorld/Bullet/WuwaBulletSubsystem.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaCombatAttributeSet.h"
#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"
#include "Game/NewWorld/Character/Monster/WuwaMonsterAnimInstance.h"
#include "Game/NewWorld/Combat/WuwaDamage.h"
#include "Game/NewWorld/Combat/WuwaHitComponent.h"

namespace WuwaHitTest
{
	using C = UWuwaCombatAttributeSet;

	/** 测试世界：BeginPlay 手动派发（测试世界不跑 BeginPlay），初始属性按顺序写（ToughMax 在 Tough 前）。 */
	struct FArena
	{
		UWorld* World = nullptr;
		FArena()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			World->InitializeActorsForPlay(FURL());
		}
		~FArena() { World->DestroyWorld(false); }

		AWuwaCharactorBase* Spawn(TArray<TPair<FGameplayAttribute, float>> Attributes)
		{
			AWuwaCharactorBase* Actor = World->SpawnActorDeferred<AWuwaCharactorBase>(AWuwaCharactorBase::StaticClass(), FTransform::Identity);
			for (const TPair<FGameplayAttribute, float>& Pair : Attributes)
			{
				FWuwaInitialAttribute& Initial = Actor->InitialAttributes.AddDefaulted_GetRef();
				Initial.Attribute = Pair.Key;
				Initial.Value = Pair.Value;
			}
			Actor->FinishSpawning(FTransform::Identity);
			Actor->DispatchBeginPlay();
			return Actor;
		}
	};

	float Get(const AWuwaCharactorBase* Actor, const FGameplayAttribute& Attribute)
	{
		return Actor->GetAbilitySystemComponent()->GetNumericAttribute(Attribute);
	}

	bool HasTag(const AWuwaCharactorBase* Actor, const TCHAR* Tag)
	{
		return Actor->GetAbilitySystemComponent()->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(Tag));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaHitToughTest, "Wuwa.Combat.Hit.Tough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaHitToughTest::RunTest(const FString& Parameters)
{
	using namespace WuwaHitTest;
	FArena Arena;

	// 芙露德莉斯（原作 BaseProperty 394080018）：韧性 2000、虚弱 0.1 秒、不回复
	AWuwaCharactorBase* Boss = Arena.Spawn({{C::GetToughMaxAttribute(), 2000.f}, {C::GetToughAttribute(), 2000.f}, {C::GetWeakTimeAttribute(), 0.1f}});
	UWuwaHitComponent* BossHit = Boss->HitComponent;
	if (!TestNotNull(TEXT("every character has a hit component"), BossHit)) return false;
	BossHit->ApplyToughDecrease(500.f);
	TestEqual(TEXT("tough reduced"), Get(Boss, C::GetToughAttribute()), 1500.f);
	TestFalse(TEXT("not weak while tough is left"), BossHit->IsWeak());
	BossHit->ApplyToughDecrease(5000.f);
	TestEqual(TEXT("tough clamps at 0"), Get(Boss, C::GetToughAttribute()), 0.f);
	TestTrue(TEXT("weak at 0"), BossHit->IsWeak() && HasTag(Boss, TEXT("State.Combat.Weak")));
	BossHit->AdvanceTimers(0.05f);
	TestTrue(TEXT("still weak inside WeakTime"), BossHit->IsWeak());
	BossHit->AdvanceTimers(0.06f);
	TestFalse(TEXT("weak ends after WeakTime"), BossHit->IsWeak() || HasTag(Boss, TEXT("State.Combat.Weak")));
	TestEqual(TEXT("no recover for the boss"), Get(Boss, C::GetToughAttribute()), 0.f);

	// 角色（原作 1205 / 1208：韧性 10000、每秒回 1000），这里加 0.5 秒回复延迟
	AWuwaCharactorBase* Role = Arena.Spawn({{C::GetToughMaxAttribute(), 10000.f}, {C::GetToughAttribute(), 10000.f}, {C::GetToughRecoverAttribute(), 1000.f},
		{C::GetToughRecoverDelayTimeAttribute(), 0.5f}, {C::GetWeakTimeAttribute(), 0.1f}});
	UWuwaHitComponent* RoleHit = Role->HitComponent;
	RoleHit->ApplyToughDecrease(3000.f);
	TestEqual(TEXT("recover delay starts"), RoleHit->GetToughRecoverDelayRemaining(), 0.5f);
	RoleHit->AdvanceTimers(0.4f);
	RoleHit->AdvanceTimers(0.2f);
	TestEqual(TEXT("no recover during the delay"), Get(Role, C::GetToughAttribute()), 7000.f);
	RoleHit->AdvanceTimers(1.f);
	TestNearlyEqual(TEXT("recovers ToughRecover per second"), Get(Role, C::GetToughAttribute()), 8000.f, 1e-2f);
	RoleHit->ApplyToughDecrease(9000.f);
	TestTrue(TEXT("weak at 0"), RoleHit->IsWeak());
	TestEqual(TEXT("recover speed zeroed while weak"), Role->GetAbilitySystemComponent()->GetNumericAttributeBase(C::GetToughRecoverAttribute()), 0.f);
	RoleHit->AdvanceTimers(0.2f);
	TestEqual(TEXT("recover speed restored after weak"), Role->GetAbilitySystemComponent()->GetNumericAttributeBase(C::GetToughRecoverAttribute()), 1000.f);
	RoleHit->AdvanceTimers(1.f);
	// 退出虚弱的那一帧（0.2 秒里虚弱只占 0.1）也按整帧回复：0.2 + 1 秒
	TestNearlyEqual(TEXT("recovers again"), Get(Role, C::GetToughAttribute()), 1200.f, 1e-2f);
	RoleHit->AdvanceTimers(20.f);
	TestEqual(TEXT("recover stops at ToughMax"), Get(Role, C::GetToughAttribute()), 10000.f);

	// 削韧值（原作 ToughCalculation）：ToughLv × 攻击者 ToughChange × 受击者 ToughReduce × 攻击者 SkillToughRatio
	FWuwaDamageInfo Info;
	Info.ToughLv = {1000.f, 2000.f};
	Role->GetAbilitySystemComponent()->SetNumericAttributeBase(C::GetToughChangeAttribute(), 1.5f);
	Role->GetAbilitySystemComponent()->SetNumericAttributeBase(C::GetSkillToughRatioAttribute(), 2.f);
	Boss->GetAbilitySystemComponent()->SetNumericAttributeBase(C::GetToughReduceAttribute(), 0.5f);
	TestNearlyEqual(TEXT("tough decrease formula"), WuwaDamage::CalculateTough(Info, 2, Role->GetAbilitySystemComponent(), Boss->GetAbilitySystemComponent()), 3000.f, 1e-3f);
	TestNearlyEqual(TEXT("defaults are 1"), WuwaDamage::CalculateTough(Info, 1, nullptr, nullptr), 1000.f, 1e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaHitTimeScaleTest, "Wuwa.Combat.Hit.TimeScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaHitTimeScaleTest::RunTest(const FString& Parameters)
{
	using namespace WuwaHitTest;
	// 没有曲线：整段都是缩放值
	TestEqual(TEXT("no curve -> constant value"), UWuwaHitComponent::EvaluateTimeScale(0.3f, nullptr, 0.05f, 0.1f), 0.3f);
	// 曲线（时间 0~0.5、值 0~1）：进度映射到曲线时间，值归一后 1 - v × (1 - 缩放值)
	UCurveFloat* Curve = NewObject<UCurveFloat>();
	Curve->FloatCurve.AddKey(0.f, 0.f);
	Curve->FloatCurve.AddKey(0.5f, 1.f);
	TestNearlyEqual(TEXT("curve start (value 0) -> no slow"), UWuwaHitComponent::EvaluateTimeScale(0.2f, Curve, 0.f, 1.f), 1.f, 1e-4f);
	TestNearlyEqual(TEXT("curve half -> half way"), UWuwaHitComponent::EvaluateTimeScale(0.2f, Curve, 0.5f, 1.f), 0.6f, 1e-4f);
	TestNearlyEqual(TEXT("curve end (value max) -> full slow"), UWuwaHitComponent::EvaluateTimeScale(0.2f, Curve, 1.f, 1.f), 0.2f, 1e-4f);
	// 原作 Yangyang Time 曲线（长离子弹用）导进项目后能加载
	const UCurveFloat* Imported = LoadObject<UCurveFloat>(nullptr, TEXT("/Game/Characters/Role/changli/Data/TimeScale/Time.Time"));
	TestTrue(TEXT("imported time scale curve has keys"), Imported && Imported->FloatCurve.GetNumKeys() > 0);

	// 优先级：高的生效，同优先级取更慢的；到时自动移除，CustomTimeDilation 跟着变
	FArena Arena;
	AWuwaCharactorBase* Actor = Arena.Spawn({});
	UWuwaHitComponent* Hit = Actor->HitComponent;
	const int32 Low = Hit->SetTimeScale(1, 0.1f, nullptr, 10.f);
	Hit->SetTimeScale(2, 0.5f, nullptr, 10.f);
	TestEqual(TEXT("higher priority wins"), Hit->GetCurrentTimeScale(), 0.5f);
	Hit->SetTimeScale(2, 0.3f, nullptr, 10.f);
	TestEqual(TEXT("same priority -> slower"), Hit->GetCurrentTimeScale(), 0.3f);
	TestEqual(TEXT("actor time dilation follows"), Actor->CustomTimeDilation, 0.3f);
	TestEqual(TEXT("zero duration is ignored"), Hit->SetTimeScale(9, 0.01f, nullptr, 0.f), -1);
	Hit->RemoveTimeScale(Low);
	TestEqual(TEXT("removing the low one keeps the top"), Hit->GetCurrentTimeScale(), 0.3f);
	return true;
}

namespace WuwaHitTest
{
	UWorld* FindPlayWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) return Context.World();
		}
		return nullptr;
	}

	/** 大卡：当前角色站到 Boss 面前，用自己子弹表里的真实子弹打 Boss，直到韧性打空 → 受击蒙太奇、顿帧。 */
	class FDaKaCheck final : public IAutomationLatentCommand
	{
	public:
		explicit FDaKaCheck(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			if (Now - StartedAt > 300.0) { Test->AddError(FString::Printf(TEXT("Timed out at step %d (%d shots)."), Step, Shots)); return true; }
			UWorld* World = FindPlayWorld();
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			AWuwaCharactorBase* Pawn = Player ? Cast<AWuwaCharactorBase>(Player->GetPawn()) : nullptr;
			if (!Pawn) return false;
			if (!Boss.IsValid())
			{
				for (TActorIterator<AWuwaCharactorBase> It(World); It; ++It)
				{
					if (It->GetClass()->GetName().Contains(TEXT("BP_Monster_Fuludelisi"))) Boss = *It;
				}
				if (!Boss.IsValid()) return false;
			}
			AWuwaCharactorBase* Target = Boss.Get();
			UWuwaHitComponent* Hit = Target->HitComponent;
			UAbilitySystemComponent* Abilities = Target->GetAbilitySystemComponent();
			UWuwaBulletSubsystem* Bullets = World->GetSubsystem<UWuwaBulletSubsystem>();
			if (Step == 0)
			{
				UAnimInstance* Anim = Target->GetMesh()->GetAnimInstance();
				Test->TestTrue(TEXT("boss uses the monster anim instance"), Anim && Anim->IsA<UWuwaMonsterAnimInstance>());
				Test->TestNotNull(TEXT("boss idle animation"), Target->IdleAnimation.Get());
				Test->TestEqual(TEXT("boss hit montages from the original state machine"), Target->BeHitMontages.Num(), 10);
				Test->TestEqual(TEXT("boss tough from BaseProperty"), WuwaHitTest::Get(Target, C::GetToughAttribute()), 2000.f);
				Test->TestEqual(TEXT("boss mass from BaseProperty"), WuwaHitTest::Get(Target, C::GetMassAttribute()), 600.f);
				Test->TestEqual(TEXT("test skill level 10"), Pawn->SkillLevel, 10);
				if (!Test->TestNotNull(TEXT("attacker hit effect table"), Pawn->HitEffectTable.Get()) || !Pawn->BulletTable) return true;
				// 挑一颗：从攻击者出生、有伤害、受击效果在表里且 Boss 有对应蒙太奇的子弹
				for (const TPair<FName, uint8*>& Row : Pawn->BulletTable->GetRowMap())
				{
					const FWuwaBulletInfo* Info = reinterpret_cast<const FWuwaBulletInfo*>(Row.Value);
					const FWuwaHitEffectInfo* Effect = Info->BeHitEffect.IsNone() ? nullptr
						: Pawn->HitEffectTable->FindRow<FWuwaHitEffectInfo>(Info->BeHitEffect, TEXT("Test"), false);
					if (Info->DamageId == 0 || !Effect || Info->BornStandard != EWuwaBulletBornStandard::Attacker || Info->BornOffset.Size() > 200.0
						|| !Target->BeHitMontages.Contains(static_cast<int32>(Effect->BeHitAnim))) continue;
					BulletId = Row.Key;
					ExpectedMontage = Target->BeHitMontages[static_cast<int32>(Effect->BeHitAnim)].LoadSynchronous();
					bVictimSlows = Info->VictimTimeScale.Duration > 0.f && !Info->bContinuousCollision;
					break;
				}
				if (!Test->TestFalse(TEXT("found a usable bullet"), BulletId.IsNone())) return true;
				Test->AddInfo(FString::Printf(TEXT("%s fires %s at the boss"), *Pawn->GetName(), *BulletId.ToString()));
				// 站到 Boss 面前 120cm、面向它
				const FVector Forward = Target->GetActorForwardVector();
				Pawn->SetActorLocation(Target->GetActorLocation() + Forward * 120.0, false, nullptr, ETeleportType::TeleportPhysics);
				Pawn->SetActorRotation(FRotator(0.0, (-Forward).Rotation().Yaw, 0.0));
				HealthBefore = WuwaHitTest::Get(Target, UWuwaAttributeSet::GetHealthAttribute());
				Step = 1;
				return false;
			}
			if (Step == 1)
			{
				// 每 0.3 秒一发，直到受击动作出来
				if (Hit->GetLastHitAnim().IsSet())
				{
					Test->TestTrue(TEXT("tough emptied before the hit anim"), WuwaHitTest::Get(Target, C::GetToughAttribute()) <= 0.f);
					Test->TestEqual(TEXT("hit montage from the state machine map"), Hit->GetLastHitMontage(), ExpectedMontage);
					UAnimInstance* Anim = Target->GetMesh()->GetAnimInstance();
					Test->TestTrue(TEXT("hit montage is playing"), Anim && ExpectedMontage && Anim->Montage_IsPlaying(ExpectedMontage));
					const float HealthNow = WuwaHitTest::Get(Target, UWuwaAttributeSet::GetHealthAttribute());
					Test->TestTrue(TEXT("boss took damage"), HealthNow < HealthBefore);
					Test->AddInfo(FString::Printf(TEXT("%d shots: health %.0f -> %.0f (%.0f per shot), time dilation %.3f"), Shots, HealthBefore, HealthNow,
						(HealthBefore - HealthNow) / FMath::Max(Shots, 1), Target->CustomTimeDilation));
					if (bVictimSlows) Test->TestTrue(TEXT("victim hit stop"), SawSlow);
					Step = 2;
					StepAt = Now;
					return false;
				}
				if (Target->CustomTimeDilation < 1.f) SawSlow = true;
				if (Shots >= 40) { Test->AddError(TEXT("no hit anim after 40 shots")); return true; }
				if (Now - LastShotAt < 0.3) return false;
				FWuwaBulletSpawnRequest Request;
				Request.Attacker = Pawn;
				Request.BulletId = BulletId;
				Test->TestTrue(TEXT("bullet spawned"), Bullets->SpawnBullet(Request) != 0);
				++Shots;
				LastShotAt = Now;
				return false;
			}
			if (Step == 2)
			{
				if (Now - StepAt < 3.0) return false;
				Test->TestEqual(TEXT("time dilation back to 1"), Target->CustomTimeDilation, 1.f);
				return true;
			}
			return true;
		}

	private:
		FAutomationTestBase* Test;
		TWeakObjectPtr<AWuwaCharactorBase> Boss;
		FName BulletId;
		UAnimMontage* ExpectedMontage = nullptr;
		bool bVictimSlows = false;
		bool SawSlow = false;
		float HealthBefore = 0.f;
		double StartedAt = -1.0;
		double StepAt = 0.0;
		double LastShotAt = 0.0;
		int32 Step = 0;
		int32 Shots = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaHitDaKaTest, "Wuwa.Combat.Hit.DaKaBossHitReaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaHitDaKaTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Environment/DaKa/Maps/L_DaKa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(WuwaHitTest::FDaKaCheck(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
