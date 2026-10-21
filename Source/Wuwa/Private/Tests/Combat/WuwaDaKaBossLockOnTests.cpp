#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Engine/LocalPlayer.h"
#include "Game/Module/Formation/WuwaFormationModel.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Engine/DataTable.h"
#include "Game/NewWorld/Bullet/WuwaBulletSubsystem.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "Game/NewWorld/Combat/WuwaCamp.h"
#include "Game/NewWorld/Combat/WuwaCreatureSubsystem.h"
#include "Game/NewWorld/Combat/WuwaDamage.h"
#include "Game/NewWorld/Combat/WuwaCombatSettings.h"
#include "HAL/IConsoleManager.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Game/NewWorld/Combat/WuwaLockOnComponent.h"

namespace WuwaDaKaBossLockOn
{
	UWorld* FindPlayWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) return Context.World();
		}
		return nullptr;
	}

	/** 等玩家角色出来，然后检查：大卡登记在实体表里、是敌对的、玩家用嘉贝普攻的锁定参数（114，14 米）能锁到它，嘉贝的枪能打中它并扣血。 */
	class FCheck final : public IAutomationLatentCommand
	{
	public:
		explicit FCheck(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			if (Now - StartedAt > 300.0) { Test->AddError(TEXT("Timed out waiting for the player pawn.")); return true; }
			UWorld* World = FindPlayWorld();
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			AWuwaCharacter* Role = Player ? Cast<AWuwaCharacter>(Player->GetPawn()) : nullptr;
			if (!Role) return false;
			if (bFired)
			{
				if ((!HitVictims->Contains(ExpectedBoss) || DamageEvents->IsEmpty()) && Now - FiredAt < 2.0) return false;
				Test->TestTrue(TEXT("the shot hits the DaKa boss"), HitVictims->Contains(ExpectedBoss));
				// 伤害：嘉贝普攻1（1208001001）按公式扣大卡的血
				const UAbilitySystemComponent* BossASC = ExpectedBoss.IsValid() ? Cast<AWuwaCharactorBase>(ExpectedBoss.Get())->GetAbilitySystemComponent() : nullptr;
				if (Test->TestEqual(TEXT("one damage event on the boss"), DamageEvents->Num(), 1) && BossASC)
				{
					const FWuwaDamageEvent& Event = (*DamageEvents)[0];
					const float Health = BossASC->GetNumericAttribute(UWuwaAttributeSet::GetHealthAttribute());
					Test->AddInfo(FString::Printf(TEXT("Damage %.0f%s, boss health %.0f -> %.0f"), Event.Damage, Event.bCritical ? TEXT(" (crit)") : TEXT(""), BossHealthBefore, Health));
					Test->TestTrue(TEXT("damage is positive"), Event.Damage > 0.f);
					Test->TestEqual(TEXT("boss health reduced by the damage"), Health, BossHealthBefore - Event.Damage);
					// 命中回能：原作伤害表 1208001001 Energy 83 / ElementPower 116，加给攻击者（嘉贝）。协奏上限按 DT_QteTag 普通QTE 10000。
					const UAbilitySystemComponent* RoleASC = Role->GetAbilitySystemComponent();
					const float Energy = RoleASC->GetNumericAttribute(UWuwaAttributeSet::GetEnergyAttribute());
					const float Concerto = RoleASC->GetNumericAttribute(UWuwaAttributeSet::GetConcertoEnergyAttribute());
					Test->AddInfo(FString::Printf(TEXT("energy %.0f -> %.0f (+%.0f), concerto %.0f -> %.0f (+%.0f) / %.0f"), EnergyBefore, Energy, Event.EnergyGain,
						ConcertoBefore, Concerto, Event.ConcertoGain, RoleASC->GetNumericAttribute(UWuwaAttributeSet::GetMaxConcertoEnergyAttribute())));
					// 共鸣能量 = 表值 × 共鸣效率 × 测试倍率（DefaultGame.ini EnergyGainScale / wuwa.Damage.EnergyScale）
					const IConsoleVariable* EnergyScaleVar = IConsoleManager::Get().FindConsoleVariable(TEXT("wuwa.Damage.EnergyScale"));
					const float EnergyScale = EnergyScaleVar && EnergyScaleVar->GetFloat() >= 0.f ? EnergyScaleVar->GetFloat() : GetDefault<UWuwaCombatSettings>()->EnergyGainScale;
					const float ExpectedEnergy = 83.f * RoleASC->GetNumericAttribute(UWuwaAttributeSet::GetEnergyEfficiencyAttribute()) * EnergyScale;
					Test->TestEqual(TEXT("energy efficiency initialized to 100%"), RoleASC->GetNumericAttribute(UWuwaAttributeSet::GetEnergyEfficiencyAttribute()), 1.f);
					Test->TestEqual(TEXT("energy gain = damage row x efficiency x test scale"), Event.EnergyGain, ExpectedEnergy);
					Test->TestEqual(TEXT("concerto gain from the damage row"), Event.ConcertoGain, 116.f);
					Test->TestEqual(TEXT("attacker energy increased"), Energy, FMath::Min(EnergyBefore + ExpectedEnergy, RoleASC->GetNumericAttribute(UWuwaAttributeSet::GetMaxEnergyAttribute())));
					Test->TestEqual(TEXT("attacker concerto increased"), Concerto, ConcertoBefore + 116.f);
					Test->TestEqual(TEXT("concerto max = 10000 (DT_QteTag)"), RoleASC->GetNumericAttribute(UWuwaAttributeSet::GetMaxConcertoEnergyAttribute()), 10000.f);
					// UI 模型监听属性变化：当前成员的协奏同步更新
					const ULocalPlayer* Local = Role->GetWorld()->GetFirstLocalPlayerFromController();
					const UWuwaFormationModel* Model = Local ? Local->GetSubsystem<UWuwaFormationModel>() : nullptr;
					bool bModelUpdated = false;
					if (Model) for (const FWuwaTeamMemberUiData& Member : Model->GetMembers()) bModelUpdated |= FMath::IsNearlyEqual(Member.ConcertoEnergy, Concerto);
					Test->TestTrue(TEXT("formation UI model sees the new concerto"), bModelUpdated);
				}
				return true;
			}

			const UWuwaCreatureSubsystem* Creatures = UWuwaCreatureSubsystem::Get(World);
			AWuwaCharactorBase* Boss = nullptr;
			for (const TWeakObjectPtr<AWuwaCharactorBase>& Entry : Creatures->GetCreatures())
			{
				if (Entry.IsValid() && Entry->GetClass()->GetName().Contains(TEXT("BP_Monster_Fuludelisi"))) Boss = Entry.Get();
			}
			if (!Test->TestNotNull(TEXT("DaKa boss is registered in the creature table"), Boss)) return true;
			// 不依赖 PlayerStart 摆在哪（可能在高台上）：放到 Boss 正前方 12 米、同一地面，落地站稳再量。
			if (PlacedAt < 0.0)
			{
				const FVector Forward = Boss->GetActorForwardVector();
				Role->SetActorLocation(Boss->GetActorLocation() + Forward * 1200.0, false, nullptr, ETeleportType::TeleportPhysics);
				Role->SetActorRotation(FRotator(0.0, (-Forward).Rotation().Yaw, 0.0));
				PlacedAt = Now;
				return false;
			}
			if (Now - PlacedAt < 1.0 || !Role->GetCharacterMovement() || !Role->GetCharacterMovement()->IsMovingOnGround()) return false;
			Test->AddInfo(FString::Printf(TEXT("Player %s at %s, boss at %s (%.0f cm)"), *Role->GetName(), *Role->GetActorLocation().ToCompactString(),
				*Boss->GetActorLocation().ToCompactString(), FVector::Dist(Role->GetActorLocation(), Boss->GetActorLocation())));
			Test->TestEqual(TEXT("boss camp"), UWuwaCampLibrary::GetCamp(Boss), FName(TEXT("Monster")));
			Test->TestTrue(TEXT("boss is hostile to the player"), UWuwaCampLibrary::IsHostile(Role, Boss));
			Test->TestTrue(TEXT("boss stands on the floor (not falling through)"), Boss->GetActorLocation().Z > -200.0);
			// 开场距离 12 米：默认软锁定（1 号，8 米）够不到，嘉贝普攻的 114 号（14 米）能锁到
			UWuwaLockOnComponent* LockOn = Role->LockOnComponent;
			Test->TestFalse(TEXT("config 1 (8 m) does not reach the boss at 12 m"), LockOn->DetectSoftLockTarget(1, EWuwaSkillTargetPriority::InputDirection));
			Test->TestTrue(TEXT("config 114 (14 m) locks the boss"), LockOn->DetectSoftLockTarget(114, EWuwaSkillTargetPriority::InputDirection)
				&& LockOn->GetCurrentTarget().Actor.Get() == Boss);

			// 嘉贝普攻1的枪（技能目标出生基准）：技能目标设成大卡，发一颗子弹，应在几帧内命中大卡（真实场景里的胶囊碰撞）
			if (!bFired)
			{
				if (!Role->BulletTable || !Role->BulletTable->FindRowUnchecked(TEXT("1208001001")))
				{
					Test->AddInfo(TEXT("Current role is not Jiabei; skip the shot."));
					return true;
				}
				UWuwaBulletSubsystem* Bullets = UWuwaBulletSubsystem::Get(World);
				// 共享数组：命令结束后子弹系统还可能广播，不能捕获 this
				Bullets->OnBulletHit.AddLambda([Victims = HitVictims](const FWuwaBulletHit& Hit) { Victims->Add(Hit.Victim); });
				UWuwaDamageSubsystem::Get(World)->OnDamage.AddLambda([Events = DamageEvents](const FWuwaDamageEvent& Event) { Events->Add(Event); });
				// 大卡的初始属性（WuwaMonsterSetup 写进蓝图的原作模板 665800000 的 90 级属性），开场满血
				const UAbilitySystemComponent* BossASC = Boss->GetAbilitySystemComponent();
				BossHealthBefore = BossASC->GetNumericAttribute(UWuwaAttributeSet::GetHealthAttribute());
				const UAbilitySystemComponent* RoleASC = Role->GetAbilitySystemComponent();
				EnergyBefore = RoleASC->GetNumericAttribute(UWuwaAttributeSet::GetEnergyAttribute());
				ConcertoBefore = RoleASC->GetNumericAttribute(UWuwaAttributeSet::GetConcertoEnergyAttribute());
				Test->TestEqual(TEXT("boss max health from InitialAttributes"), BossASC->GetNumericAttribute(UWuwaAttributeSet::GetMaxHealthAttribute()), 1298586.f);
				Test->TestEqual(TEXT("boss starts at full health"), BossHealthBefore, 1298586.f);
				Role->SkillComponent->SetSkillTarget(Boss, NAME_None);
				FWuwaBulletSpawnRequest Request;
				Request.Attacker = Role;
				Request.BulletId = TEXT("1208001001");
				Test->TestTrue(TEXT("shot spawned"), Bullets->SpawnBullet(Request) != 0);
				bFired = true;
				FiredAt = Now;
				ExpectedBoss = Boss;
				return false;
			}
			return true;
		}

	private:
		FAutomationTestBase* Test;
		double StartedAt = -1.0;
		double PlacedAt = -1.0;
		bool bFired = false;
		double FiredAt = 0.0;
		TWeakObjectPtr<AActor> ExpectedBoss;
		TSharedRef<TArray<TWeakObjectPtr<AActor>>> HitVictims = MakeShared<TArray<TWeakObjectPtr<AActor>>>();
		TSharedRef<TArray<FWuwaDamageEvent>> DamageEvents = MakeShared<TArray<FWuwaDamageEvent>>();
		float BossHealthBefore = 0.f;
		float EnergyBefore = 0.f;
		float ConcertoBefore = 0.f;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaDaKaBossLockOnTest, "Wuwa.Combat.LockOn.DaKaBoss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaDaKaBossLockOnTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Environment/DaKa/Maps/L_DaKa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(WuwaDaKaBossLockOn::FCheck(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
