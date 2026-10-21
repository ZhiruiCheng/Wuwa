#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "AbilitySystemComponent.h"
#include "Core/ActorComponent/UIItem.h"
#include "Core/ActorComponent/UIText.h"
#include "Curves/CurveFloat.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Game/LGUI/Component/WuwaUiSlicedFill.h"
#include "Game/LGUI/Component/WuwaUiTweenGroup.h"
#include "Game/LGUI/Core/WuwaUiManager.h"
#include "Game/LGUI/View/WuwaUiState.h"
#include "Game/Module/BattleUi/WuwaBossStatePanel.h"
#include "Game/Module/DamageUi/WuwaDamageNumberSubsystem.h"
#include "Game/Module/Hud/WuwaMainHudController.h"
#include "Game/Module/Hud/WuwaMainHudView.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"
#include "Game/NewWorld/Combat/WuwaDamage.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaUiTweenEvaluateTest, "Wuwa.UI.BattleUi.TweenAndBuffer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaUiTweenEvaluateTest::RunTest(const FString& Parameters)
{
	// 补间：延迟、线性、结束
	FWuwaUiTween Tween;
	Tween.From = FVector4(1, 0, 0, 0);
	Tween.To = FVector4(0, 0, 0, 0);
	Tween.Duration = 0.4f;
	Tween.StartDelay = 0.6f;
	FVector4 Value;
	bool bFinished = false;
	TestFalse(TEXT("still in the start delay"), UWuwaUiTweenGroup::Evaluate(Tween, 0.3f, Value, bFinished));
	TestTrue(TEXT("after the delay"), UWuwaUiTweenGroup::Evaluate(Tween, 0.8f, Value, bFinished));
	TestNearlyEqual(TEXT("linear halfway"), Value.X, 0.5, 1e-4);
	TestFalse(TEXT("not finished halfway"), bFinished);
	UWuwaUiTweenGroup::Evaluate(Tween, 1.5f, Value, bFinished);
	TestTrue(TEXT("finished after duration"), bFinished && FMath::IsNearlyZero(Value.X));
	// 缓动：OutExpo 一开始就很快
	Tween.Ease = ELTweenEase::OutExpo;
	Tween.StartDelay = 0.f;
	UWuwaUiTweenGroup::Evaluate(Tween, 0.1f, Value, bFinished);
	TestTrue(TEXT("OutExpo is past halfway after a quarter"), Value.X < 0.5);
	// 曲线
	UCurveFloat* Curve = NewObject<UCurveFloat>();
	Curve->FloatCurve.AddKey(0.f, 0.f);
	Curve->FloatCurve.AddKey(1.f, 2.f);
	Tween.Ease = ELTweenEase::CurveFloat;
	Tween.Curve = Curve;
	Tween.From = FVector4(0, 0, 0, 0);
	Tween.To = FVector4(1, 0, 0, 0);
	UWuwaUiTweenGroup::Evaluate(Tween, 0.2f, Value, bFinished);
	TestNearlyEqual(TEXT("curve drives the progress"), Value.X, 1.0, 1e-3);
	// Yoyo 两轮：第二轮反向，结束在起点
	FWuwaUiTween Yoyo;
	Yoyo.Loop = ELTweenLoop::Yoyo;
	Yoyo.LoopCount = 2;
	Yoyo.Duration = 0.1f;
	UWuwaUiTweenGroup::Evaluate(Yoyo, 0.15f, Value, bFinished);
	TestNearlyEqual(TEXT("yoyo second cycle goes back"), Value.X, 0.5, 1e-3);
	UWuwaUiTweenGroup::Evaluate(Yoyo, 0.3f, Value, bFinished);
	TestTrue(TEXT("yoyo ends at the start"), bFinished && FMath::IsNearlyZero(Value.X, 1e-4));

	// 受击残影：停 0.25 秒、0.25 秒缩完；停顿期间第 9 次受击直接开始缩
	FWuwaHpBufferStateMachine Buffer;
	TestEqual(TEXT("no buffer at first"), Buffer.Update(0.1f), -1.f);
	Buffer.GetHit(0.8f, 1.f);
	TestEqual(TEXT("delay keeps the old percent"), Buffer.Update(0.1f), 1.f);
	Buffer.Update(0.2f);                       // 停顿结束
	TestNearlyEqual(TEXT("shrinks halfway"), Buffer.Update(0.125f), 0.9f, 1e-4f);
	TestNearlyEqual(TEXT("reaches the target"), Buffer.Update(0.2f), 0.8f, 1e-4f);
	TestEqual(TEXT("then disappears"), Buffer.Update(0.1f), -1.f);
	Buffer.GetHit(0.7f, 0.8f);
	for (int32 Hit = 0; Hit < 8; ++Hit) Buffer.GetHit(0.7f - Hit * 0.01f, 0.8f);
	Buffer.Update(0.01f);
	TestTrue(TEXT("ninth hit in the delay starts shrinking"), Buffer.Update(0.05f) < 0.8f);
	Buffer.GetHit(0.9f, 0.5f);
	TestTrue(TEXT("healing is ignored"), !Buffer.IsOriginState());

	// 伤害飘字选哪一行（原作 Q2t）
	TestEqual(TEXT("fusion damage -> row 2"), UWuwaDamageNumberSubsystem::SelectTextId(2, false, 100.f, 0), 2);
	TestEqual(TEXT("physical damage -> row 10"), UWuwaDamageNumberSubsystem::SelectTextId(0, false, 100.f, 0), 10);
	TestEqual(TEXT("heal -> row 8"), UWuwaDamageNumberSubsystem::SelectTextId(0, true, 100.f, 0), 8);
	TestEqual(TEXT("zero damage -> immune row 9"), UWuwaDamageNumberSubsystem::SelectTextId(2, false, 0.f, 0), 9);
	TestEqual(TEXT("damage text type overrides"), UWuwaDamageNumberSubsystem::SelectTextId(2, false, 100.f, 1002), 1002);
	TestEqual(TEXT("damage text type 2 falls back"), UWuwaDamageNumberSubsystem::SelectTextId(2, false, 100.f, 2), 2);
	return true;
}

namespace WuwaBattleUiTest
{
	UWorld* FindPlayWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) return Context.World();
		}
		return nullptr;
	}

	/** 大卡场景：Boss 血条出现、名字等级血量正确；打一下 → 残影、受击动画、伤害数字；1.2 秒后数字回收。 */
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
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			APawn* Pawn = Player ? Player->GetPawn() : nullptr;
			UWuwaUiManager* Manager = Player ? UWuwaUiManager::Get(Player) : nullptr;
			const UWuwaMainHudView* Hud = Manager ? Cast<UWuwaMainHudView>(Manager->GetView(UWuwaMainHudController::MainHudViewName)) : nullptr;
			UWuwaBossStatePanel* Panel = Hud ? Hud->GetBossState() : nullptr;
			UWuwaCommonBossStateView* View = Panel ? Panel->GetView() : nullptr;
			if (!Pawn || !View || View->GetState() != EWuwaUiState::Shown || !Panel->GetShownBoss()) return false;
			AWuwaCharactorBase* Boss = Panel->GetShownBoss();
			UAbilitySystemComponent* BossAbilities = Boss->GetAbilitySystemComponent();
			UWuwaDamageNumberSubsystem* Numbers = UWuwaDamageNumberSubsystem::Get(World);
			if (Step == 0)
			{
				Test->TestTrue(TEXT("the shown boss is Fuludelisi"), Boss->GetClass()->GetName().Contains(TEXT("BP_Monster_Fuludelisi")));
				Test->TestEqual(TEXT("boss name from the original template"), Boss->DisplayName.ToString(), FString(TEXT("芙露德莉斯")));
				Test->TestNearlyEqual(TEXT("bar starts full"), View->GetCurrentBarPercent(), 1.f, 1e-4f);
				{
					FString Initial;
					for (const FWuwaInitialAttribute& A : Boss->InitialAttributes) Initial += FString::Printf(TEXT("%s=%g "), *A.Attribute.GetName(), A.Value);
					Test->AddInfo(FString::Printf(TEXT("Boss level attribute %g (base %g); initial: %s"), BossAbilities->GetNumericAttribute(UWuwaAttributeSet::GetLevelAttribute()),
						BossAbilities->GetNumericAttributeBase(UWuwaAttributeSet::GetLevelAttribute()), *Initial));
				}
				Test->TestEqual(TEXT("level text (original LevelShow format)"), View->GetLevelText(), FString(TEXT("Lv. 90")));
				Test->TestEqual(TEXT("name text"), View->GetNameText(), FString(TEXT("芙露德莉斯")));
				Test->TestFalse(TEXT("no hit buffer at first"), View->IsBufferVisible());
				// 打一下：伤害结算 → 血量变 → 血条 + 残影 + 受击动画；伤害数字排队
				FWuwaDamageEvent Event;
				Test->TestTrue(TEXT("damage applied"), UWuwaDamageSubsystem::Get(World)->ApplyDamage(Pawn, Boss, 1205001000, Boss->GetActorLocation(), &Event));
				Damage = Event.Damage;
				Test->TestTrue(TEXT("hit buffer shows right after the hit"), View->IsBufferVisible());
				Step = 1;
				StepAt = Now;
				return false;
			}
			if (Step == 1)
			{
				int32 ViewX = 0, ViewY = 0;
				Player->GetViewportSize(ViewX, ViewY);
				const bool bHasViewport = ViewX > 0 && ViewY > 0;
				if (bHasViewport && Numbers->GetActiveNumbers().Num() == 0 && Now - StepAt < 2.0) return false;
				const float Expected = BossAbilities->GetNumericAttribute(UWuwaAttributeSet::GetHealthAttribute()) / BossAbilities->GetNumericAttribute(UWuwaAttributeSet::GetMaxHealthAttribute());
				Test->TestNearlyEqual(TEXT("bar follows health"), View->GetCurrentBarPercent(), Expected, 1e-5f);
				Test->TestEqual(TEXT("small hit animation (tiny damage)"), View->GetLastHitAnimation(), 8);
				// NullRHI 下视口是 0×0，没法投影到屏幕（伤害数字由 UIVisual.BossBarAndDamageNumbers 截图验证）
				if (!bHasViewport)
				{
					Test->AddInfo(TEXT("No viewport (NullRHI): damage number projection is skipped."));
					Test->TestEqual(TEXT("damage request was processed"), Numbers->GetQueuedCount(), 0);
				}
				else if (Test->TestEqual(TEXT("one damage number"), Numbers->GetActiveNumbers().Num(), 1))
				{
					const UWuwaDamageNumber* Number = Numbers->GetActiveNumbers()[0];
					Test->TestEqual(TEXT("number text is the damage"), Number->GetNumberText()->GetText().ToString(), FString::Printf(TEXT("%d"), FMath::FloorToInt(Damage)));
					Test->TestEqual(TEXT("monster damage animation"), Number->GetPlayingAnimation(), FName(TEXT("Ani_MonsterDamageSequence")));
					Test->AddInfo(FString::Printf(TEXT("Damage %.0f, number color %s"), Damage, *Number->GetNumberText()->GetColor().ToString()));
				}
				// 大受击：一次掉 20%
				BossAbilities->SetNumericAttributeBase(UWuwaAttributeSet::GetHealthAttribute(),
					BossAbilities->GetNumericAttribute(UWuwaAttributeSet::GetHealthAttribute()) - 0.2f * BossAbilities->GetNumericAttribute(UWuwaAttributeSet::GetMaxHealthAttribute()));
				Test->TestEqual(TEXT("big hit animation (20% drop)"), View->GetLastHitAnimation(), 9);
				Step = 2;
				StepAt = Now;
				return false;
			}
			if (Step == 2)
			{
				if (Now - StepAt < 1.6) return false;
				Test->TestEqual(TEXT("damage number recycled after its lifetime"), Numbers->GetActiveNumbers().Num(), 0);
				Test->TestFalse(TEXT("buffer gone after shrinking"), View->IsBufferVisible());
				return true;
			}
			return true;
		}

	private:
		FAutomationTestBase* Test;
		double StartedAt = -1.0;
		double StepAt = 0.0;
		int32 Step = 0;
		float Damage = 0.f;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaBattleUiDaKaTest, "Wuwa.UI.BattleUi.DaKaBossBarAndDamageNumbers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaBattleUiDaKaTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Environment/DaKa/Maps/L_DaKa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(WuwaBattleUiTest::FCheck(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
