#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Game/LGUI/Core/WuwaUiManager.h"
#include "Game/LGUI/View/WuwaUiState.h"
#include "Game/Module/BattleUi/WuwaBossStatePanel.h"
#include "Game/Module/DamageUi/WuwaDamageNumberSubsystem.h"
#include "Game/Module/Hud/WuwaMainHudController.h"
#include "Game/Module/Hud/WuwaMainHudView.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
#include "Game/NewWorld/Bullet/WuwaBulletSubsystem.h"
#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"
#include "Game/NewWorld/Combat/WuwaHitComponent.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Formation/WuwaFormationComponent.h"
#include "Game/Module/Hud/Buff/WuwaHudBuffBar.h"
#include "Game/Module/Hud/Buff/WuwaHudBuffItem.h"
#include "Game/Module/Hud/WuwaHudRoleStatePanel.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Engine/DataTable.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "RHI.h"
#include "TextureCompiler.h"
#include "UnrealClient.h"

// 只用于人工看画面（不随常规测试套件运行）：大卡场景里 Boss 血条和伤害数字，连续截图到 Saved/Diagnostics/BattleUi。
namespace BattleUiVisual
{
	UWorld* FindPlayWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) return Context.World();
		}
		return nullptr;
	}

	bool Save(UWorld* World, const FString& Path)
	{
		UGameViewportClient* Client = World->GetGameViewport();
		FViewport* Viewport = Client ? Client->Viewport : nullptr;
		TArray<FColor> Pixels;
		if (!Viewport || Viewport->GetSizeXY().X <= 0 || !Viewport->ReadPixels(Pixels)) return false;
		for (FColor& Pixel : Pixels) Pixel.A = 255;
		const FIntPoint Size = Viewport->GetSizeXY();
		return FImageUtils::SaveImageByExtension(*Path, FImageView(Pixels.GetData(), Size.X, Size.Y));
	}

	class FCapture final : public IAutomationLatentCommand
	{
	public:
		FCapture(FAutomationTestBase* InTest, FString InDirectory) : Test(InTest), Directory(MoveTemp(InDirectory)) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			if (Now - StartedAt > 300.0) { Test->AddError(TEXT("Boss bar never showed.")); return true; }
			UWorld* World = FindPlayWorld();
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			APawn* Pawn = Player ? Player->GetPawn() : nullptr;
			UWuwaUiManager* Manager = Player ? UWuwaUiManager::Get(Player) : nullptr;
			const UWuwaMainHudView* Hud = Manager ? Cast<UWuwaMainHudView>(Manager->GetView(UWuwaMainHudController::MainHudViewName)) : nullptr;
			UWuwaBossStatePanel* Panel = Hud ? Hud->GetBossState() : nullptr;
			if (!Pawn || !Panel || !Panel->GetShownBoss() || !Panel->GetView() || Panel->GetView()->GetState() != EWuwaUiState::Shown) return false;
			AWuwaCharactorBase* Boss = Panel->GetShownBoss();
			UWuwaDamageNumberSubsystem* Numbers = UWuwaDamageNumberSubsystem::Get(World);
			if (ReadyAt < 0.0)
			{
				FTextureCompilingManager::Get().FinishAllCompilation();
				ReadyAt = Now;
			}
			if (Now - ReadyAt < 3.0) return false;
			// 镜头对着 Boss：伤害数字在 Boss 身上
			const FVector Head = Boss->GetActorLocation() + FVector(0, 0, 60);
			if (Shot == 0)
			{
				Player->SetControlRotation((Head - Pawn->GetActorLocation()).Rotation());
				Save(World, Directory / TEXT("Battle_0_Full.png"));
				// 一个普通、一个暴击（热熔），再掉 20% 血（大受击 + 残影）
				FWuwaDamageNumberRequest Normal;
				Normal.Damage = 5070.f;
				Normal.Element = 2;
				Normal.WorldLocation = Head;
				Numbers->ApplyDamage(Normal);
				FWuwaDamageNumberRequest Critical = Normal;
				Critical.Damage = 12846.f;
				Critical.bCritical = true;
				Critical.WorldLocation = Head + FVector(0, 0, 40);
				Numbers->ApplyDamage(Critical);
				UAbilitySystemComponent* Abilities = Boss->GetAbilitySystemComponent();
				Abilities->SetNumericAttributeBase(UWuwaAttributeSet::GetHealthAttribute(),
					Abilities->GetNumericAttribute(UWuwaAttributeSet::GetHealthAttribute()) - 0.2f * Abilities->GetNumericAttribute(UWuwaAttributeSet::GetMaxHealthAttribute()));
				Shot = 1;
				ShotAt = Now;
				return false;
			}
			// 之后按时间点截：0.15、0.4、0.8 秒
			static const double Times[] = {0.15, 0.4, 0.8};
			if (Shot <= 3 && Now - ShotAt >= Times[Shot - 1])
			{
				const FString Path = Directory / FString::Printf(TEXT("Battle_%d_%03dms.png"), Shot, static_cast<int32>(Times[Shot - 1] * 1000));
				Test->TestTrue(*FString::Printf(TEXT("saved %s"), *Path), Save(World, Path));
				Test->AddInfo(Path);
				++Shot;
			}
			return Shot > 3;
		}

	private:
		FAutomationTestBase* Test;
		FString Directory;
		double StartedAt = -1.0;
		double ReadyAt = -1.0;
		double ShotAt = 0.0;
		int32 Shot = 0;
	};
}

namespace BattleUiVisual
{
	/** 受击：当前角色站到 Boss 面前用自己的子弹打，韧性打空后受击蒙太奇开始，按时间点截图。 */
	class FHitCapture final : public IAutomationLatentCommand
	{
	public:
		FHitCapture(FAutomationTestBase* InTest, FString InDirectory) : Test(InTest), Directory(MoveTemp(InDirectory)) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			if (Now - StartedAt > 300.0) { Test->AddError(TEXT("Timed out.")); return true; }
			UWorld* World = FindPlayWorld();
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			AWuwaCharactorBase* Pawn = Player ? Cast<AWuwaCharactorBase>(Player->GetPawn()) : nullptr;
			if (!Pawn || !Pawn->BulletTable || !Pawn->HitEffectTable) return false;
			if (!Boss.IsValid())
			{
				for (TActorIterator<AWuwaCharactorBase> It(World); It; ++It)
				{
					if (It->GetClass()->GetName().Contains(TEXT("BP_Monster_Fuludelisi"))) Boss = *It;
				}
				if (!Boss.IsValid()) return false;
				FTextureCompilingManager::Get().FinishAllCompilation();
				ReadyAt = Now;
			}
			if (Now - ReadyAt < 3.0) return false;
			AWuwaCharactorBase* Target = Boss.Get();
			if (BulletId.IsNone())
			{
				for (const TPair<FName, uint8*>& Row : Pawn->BulletTable->GetRowMap())
				{
					const FWuwaBulletInfo* Info = reinterpret_cast<const FWuwaBulletInfo*>(Row.Value);
					const FWuwaHitEffectInfo* Effect = Info->BeHitEffect.IsNone() ? nullptr : Pawn->HitEffectTable->FindRow<FWuwaHitEffectInfo>(Info->BeHitEffect, TEXT("Visual"), false);
					if (Info->DamageId != 0 && Effect && Info->BornStandard == EWuwaBulletBornStandard::Attacker && Info->BornOffset.Size() <= 200.0
						&& Target->BeHitMontages.Contains(static_cast<int32>(Effect->BeHitAnim)))
					{
						BulletId = Row.Key;
						break;
					}
				}
				if (BulletId.IsNone()) { Test->AddError(TEXT("No usable bullet.")); return true; }
				// 侧面看：角色站在 Boss 面前 150cm，镜头从角色右后方看向 Boss
				const FVector Forward = Target->GetActorForwardVector();
				Pawn->SetActorLocation(Target->GetActorLocation() + Forward * 150.0, false, nullptr, ETeleportType::TeleportPhysics);
				Pawn->SetActorRotation(FRotator(0.0, (-Forward).Rotation().Yaw, 0.0));
				Player->SetControlRotation(FRotator(-10.0, (-Forward).Rotation().Yaw + 35.0, 0.0));
				return false;
			}
			UWuwaHitComponent* Hit = Target->HitComponent;
			if (!Hit->GetLastHitAnim().IsSet())
			{
				if (Shots >= 40) { Test->AddError(TEXT("No hit anim after 40 shots.")); return true; }
				if (Now - LastShotAt < 0.3) return false;
				if (Shots == 0) Save(World, Directory / TEXT("Hit_0_Before.png"));
				FWuwaBulletSpawnRequest Request;
				Request.Attacker = Pawn;
				Request.BulletId = BulletId;
				World->GetSubsystem<UWuwaBulletSubsystem>()->SpawnBullet(Request);
				++Shots;
				LastShotAt = Now;
				return false;
			}
			if (HitAt < 0.0)
			{
				HitAt = Now;
				Test->AddInfo(FString::Printf(TEXT("%s: hit anim %d, montage %s after %d shots"), *BulletId.ToString(),
					static_cast<int32>(Hit->GetLastHitAnim().GetValue()), *GetNameSafe(Hit->GetLastHitMontage()), Shots));
			}
			static const double Times[] = {0.05, 0.25, 0.5};
			if (Shot < 3 && Now - HitAt >= Times[Shot])
			{
				const FString Path = Directory / FString::Printf(TEXT("Hit_%d_%03dms.png"), Shot + 1, static_cast<int32>(Times[Shot] * 1000));
				Test->TestTrue(*FString::Printf(TEXT("saved %s"), *Path), Save(World, Path));
				Test->AddInfo(Path);
				++Shot;
			}
			return Shot >= 3;
		}

	private:
		FAutomationTestBase* Test;
		FString Directory;
		TWeakObjectPtr<AWuwaCharactorBase> Boss;
		FName BulletId;
		double StartedAt = -1.0;
		double ReadyAt = 0.0;
		double LastShotAt = 0.0;
		double HitAt = -1.0;
		int32 Shots = 0;
		int32 Shot = 0;
	};
}

namespace BattleUiVisual
{
	/** 长离普攻 4 加心眼 → 血条上方的专属 buff 栏出现图标：出现动画中、1 秒后、剩余不到 20%（闪烁）各截一张。 */
	class FBuffCapture final : public IAutomationLatentCommand
	{
	public:
		FBuffCapture(FAutomationTestBase* InTest, FString InDirectory) : Test(InTest), Directory(MoveTemp(InDirectory)) {}

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
			if (!Formation || !Formation->IsFormationReady() || !RoleState || !RoleState->GetUniqueBuffBar() || Hud->GetState() != EWuwaUiState::Shown) return false;
			const int32 Index = Formation->GetMembers().IndexOfByPredicate([](const AWuwaCharacter* M) { return M && M->GetClass()->GetName().Contains(TEXT("Changli")); });
			if (Index == INDEX_NONE) { Test->AddError(TEXT("No Changli.")); return true; }
			AWuwaCharacter* Changli = Formation->GetMembers()[Index];
			if (Step == 0)
			{
				if (Formation->GetCurrentIndex() != Index) Formation->RequestSwitch(Index);
				FTextureCompilingManager::Get().FinishAllCompilation();
				Step = 1;
				StepAt = Now;
				return false;
			}
			if (Step == 1)
			{
				if (Now - StepAt < 3.0) return false;
				const FWuwaSkillInfo* Info = Changli->SkillTable ? Changli->SkillTable->FindRow<FWuwaSkillInfo>(TEXT("1205004"), TEXT("Visual")) : nullptr;
				if (!Info || !Info->Ability || !Changli->GetAbilitySystemComponent()->TryActivateAbilityByClass(Info->Ability)) { Test->AddError(TEXT("Attack 4 failed.")); return true; }
				Step = 2;
				StepAt = Now;
				return false;
			}
			const TArray<UWuwaHudBuffItem*> Items = RoleState->GetUniqueBuffBar()->GetShownItems();
			if (Step == 2)
			{
				if (Items.IsEmpty()) { if (Now - StepAt > 3.0) { Test->AddError(TEXT("No buff icon.")); return true; } return false; }
				ShownAt = Now;
				Step = 3;
				return false;
			}
			// 出现动画中（0.1 s）、1 秒后、剩余不到 20%（12 s 的 buff：9.8 s 后）
			static const double Times[] = {0.1, 1.0, 9.8};
			static const TCHAR* Names[] = {TEXT("Buff_1_Start.png"), TEXT("Buff_2_1s.png"), TEXT("Buff_3_Fading.png")};
			const int32 Shot = Step - 3;
			if (Shot < 3 && Now - ShownAt >= Times[Shot])
			{
				const FString Path = Directory / Names[Shot];
				Test->TestTrue(*FString::Printf(TEXT("saved %s"), *Path), Save(World, Path));
				if (Items.Num() > 0) Test->AddInfo(FString::Printf(TEXT("%s: fill %.3f, fading %d"), Names[Shot], Items[0]->GetDisplayedFill(), Items[0]->IsFading()));
				++Step;
			}
			return Step >= 6;
		}

	private:
		FAutomationTestBase* Test;
		FString Directory;
		double StartedAt = -1.0;
		double StepAt = 0.0;
		double ShownAt = 0.0;
		int32 Step = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuffBarVisualTest, "UIVisual.BuffBar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBuffBarVisualTest::RunTest(const FString& Parameters)
{
	if (GUsingNullRHI)
	{
		AddWarning(TEXT("Needs a real RHI to take screenshots; skipped under NullRHI."));
		return true;
	}
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Diagnostics/BattleUi"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	if (!AutomationOpenMap(TEXT("/Game/Environment/DaKa/Maps/L_DaKa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(BattleUiVisual::FBuffCapture(this, Directory));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossHitVisualTest, "UIVisual.BossHitReaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBossHitVisualTest::RunTest(const FString& Parameters)
{
	if (GUsingNullRHI)
	{
		AddWarning(TEXT("Needs a real RHI to take screenshots; skipped under NullRHI."));
		return true;
	}
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Diagnostics/BattleUi"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	if (!AutomationOpenMap(TEXT("/Game/Environment/DaKa/Maps/L_DaKa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(BattleUiVisual::FHitCapture(this, Directory));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleUiVisualTest, "UIVisual.BossBarAndDamageNumbers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBattleUiVisualTest::RunTest(const FString& Parameters)
{
	if (GUsingNullRHI)
	{
		AddWarning(TEXT("Needs a real RHI to take screenshots; skipped under NullRHI."));
		return true;
	}
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Diagnostics/BattleUi"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	if (!AutomationOpenMap(TEXT("/Game/Environment/DaKa/Maps/L_DaKa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(BattleUiVisual::FCapture(this, Directory));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
