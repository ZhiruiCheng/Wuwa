#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystemComponent.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Game/NewWorld/Bullet/WuwaBulletSubsystem.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaCombatAttributeSet.h"
#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"
#include "Game/NewWorld/Combat/WuwaDamage.h"

namespace WuwaDamageTest
{
	/** 测试世界：两个角色（玩家 / 怪物），BeginPlay 手动派发（测试世界不跑 BeginPlay），属性直接写。 */
	struct FArena
	{
		UWorld* World = nullptr;
		FArena()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			World->InitializeActorsForPlay(FURL());
		}
		~FArena() { World->DestroyWorld(false); }

		AWuwaCharactorBase* Spawn(const FVector& Location, FName Camp, TArray<FWuwaInitialAttribute> Attributes = {})
		{
			AWuwaCharactorBase* Actor = World->SpawnActorDeferred<AWuwaCharactorBase>(AWuwaCharactorBase::StaticClass(), FTransform(Location));
			Actor->Camp = Camp;
			Actor->InitialAttributes = MoveTemp(Attributes);
			Actor->FinishSpawning(FTransform(Location));
			Actor->DispatchBeginPlay();
			return Actor;
		}
	};

	void Set(AWuwaCharactorBase* Actor, const FGameplayAttribute& Attribute, float Value)
	{
		Actor->GetAbilitySystemComponent()->SetNumericAttributeBase(Attribute, Value);
	}

	float Get(const AWuwaCharactorBase* Actor, const FGameplayAttribute& Attribute)
	{
		return Actor->GetAbilitySystemComponent()->GetNumericAttribute(Attribute);
	}

	FWuwaInitialAttribute Initial(const FGameplayAttribute& Attribute, float Value)
	{
		FWuwaInitialAttribute Result;
		Result.Attribute = Attribute;
		Result.Value = Value;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaDamageFormulaTest, "Wuwa.Combat.Damage.Formula",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaDamageFormulaTest::RunTest(const FString& Parameters)
{
	using namespace WuwaDamageTest;
	using C = UWuwaCombatAttributeSet;
	using S = UWuwaAttributeSet;

	// 按等级取值（原作 GetLevelValue）
	const TArray<float> Levels = {1.f, 2.f, 3.f};
	TestEqual(TEXT("level 2 -> second entry"), WuwaDamage::GetLevelValue(Levels, 2, -1.f), 2.f);
	TestEqual(TEXT("level 0 -> first entry"), WuwaDamage::GetLevelValue(Levels, 0, -1.f), 1.f);
	TestEqual(TEXT("level beyond the array -> last entry"), WuwaDamage::GetLevelValue(Levels, 10, -1.f), 3.f);
	TestEqual(TEXT("empty array -> default"), WuwaDamage::GetLevelValue({}, 3, -1.f), -1.f);

	FArena Arena;
	AWuwaCharactorBase* Attacker = Arena.Spawn(FVector(0, 0, 100), TEXT("Player"));
	AWuwaCharactorBase* Target = Arena.Spawn(FVector(200, 0, 100), TEXT("Monster"));
	UAbilitySystemComponent* A = Attacker->GetAbilitySystemComponent();
	UAbilitySystemComponent* T = Target->GetAbilitySystemComponent();
	// 攻击者 90 级、攻击 2000、暴伤 2、伤害加成 0.1、热熔加成 0.3、普攻加成 0.2；目标防御 1512、热熔抗性 0.1
	Set(Attacker, S::GetLevelAttribute(), 90.f);
	Set(Attacker, S::GetAttackAttribute(), 2000.f);
	Set(Attacker, S::GetCritDamageAttribute(), 2.f);
	Set(Attacker, C::GetDamageBonusAttribute(), 0.1f);
	Set(Attacker, C::GetElementDamageBonus2Attribute(), 0.3f);
	Set(Attacker, C::GetAttackTypeBonus0Attribute(), 0.2f);
	Set(Attacker, C::GetAttackTypeBonus1Attribute(), 5.f);   // 不是这一击的攻击类型，不应算进去
	Set(Target, S::GetDefenseAttribute(), 1512.f);
	Set(Target, C::GetElementResistance2Attribute(), 0.1f);

	FWuwaDamageInfo Info;
	Info.Element = 2;
	Info.Type = 0;
	Info.RelatedProperty = 7;
	// 手算：倍率 0.5 × 攻击 2000 = 1000；防御系数 1 / (1512 / (800 + 90 × 8) + 1)；加成 1 + 0.1 + 0.3 + 0.2；抗性 1 - 0.1
	const double DefenseFactor = 1.0 / (1512.0 / 1520.0 + 1.0);
	const double Expected = 1000.0 * DefenseFactor * 1.6 * 0.9;
	TestNearlyEqual(TEXT("normal hit"), WuwaDamage::CalculateHurt(Info, 0.5f, A, T, false), static_cast<float>(Expected), 0.01f);
	TestNearlyEqual(TEXT("critical hit x crit damage"), WuwaDamage::CalculateHurt(Info, 0.5f, A, T, true), static_cast<float>(Expected * 2.0), 0.01f);

	// 无视防御 50%：防御按 756 算
	Set(Attacker, C::GetIgnoreDefenseRateAttribute(), 0.5f);
	TestNearlyEqual(TEXT("ignore defense"), WuwaDamage::CalculateHurt(Info, 0.5f, A, T, false),
		static_cast<float>(1000.0 / (756.0 / 1520.0 + 1.0) * 1.6 * 0.9), 0.01f);
	Set(Attacker, C::GetIgnoreDefenseRateAttribute(), 0.f);

	// 抗性分段：负抗性（-0.2）→ 1 + 0.2 / 2；高抗性（1.0 ≥ 0.8）→ 1 / (1 + 5)；无视抗性从抗性里减
	Set(Target, C::GetElementResistance2Attribute(), -0.2f);
	TestNearlyEqual(TEXT("negative resistance"), WuwaDamage::CalculateHurt(Info, 0.5f, A, T, false), static_cast<float>(1000.0 * DefenseFactor * 1.6 * 1.1), 0.01f);
	Set(Target, C::GetElementResistance2Attribute(), 1.f);
	TestNearlyEqual(TEXT("high resistance"), WuwaDamage::CalculateHurt(Info, 0.5f, A, T, false), static_cast<float>(1000.0 * DefenseFactor * 1.6 / 6.0), 0.01f);
	Set(Attacker, C::GetElementIgnoreResistance2Attribute(), 0.5f);
	TestNearlyEqual(TEXT("ignore resistance"), WuwaDamage::CalculateHurt(Info, 0.5f, A, T, false), static_cast<float>(1000.0 * DefenseFactor * 1.6 * 0.5), 0.01f);
	Set(Attacker, C::GetElementIgnoreResistance2Attribute(), 0.f);
	Set(Target, C::GetElementResistance2Attribute(), 0.1f);

	// 减伤：受伤减免、元素减免各一个乘区（最多 1）；特殊加成单独一个乘区
	Set(Target, C::GetDamageReduceAttribute(), 0.2f);
	Set(Target, C::GetElementDamageReduce2Attribute(), 0.5f);
	Set(Attacker, C::GetSpecialDamageBonusAttribute(), 0.25f);
	TestNearlyEqual(TEXT("reduce and special bonus"), WuwaDamage::CalculateHurt(Info, 0.5f, A, T, false),
		static_cast<float>(Expected * 0.8 * 0.5 * 1.25), 0.01f);
	Set(Target, C::GetDamageReduceAttribute(), 3.f);
	TestEqual(TEXT("damage reduce is capped at 1"), WuwaDamage::CalculateHurt(Info, 0.5f, A, T, false), 0.f);
	Set(Target, C::GetDamageReduceAttribute(), 0.f);
	Set(Target, C::GetElementDamageReduce2Attribute(), 0.f);
	Set(Attacker, C::GetSpecialDamageBonusAttribute(), 0.f);

	// 原作表里没有加成的攻击类型（如 11）只是不加成
	Info.Type = 11;
	TestNearlyEqual(TEXT("type without bonus"), WuwaDamage::CalculateHurt(Info, 0.5f, A, T, false), static_cast<float>(1000.0 * DefenseFactor * 1.4 * 0.9), 0.01f);
	Info.Type = 0;

	// 倍率乘生命上限（RelatedProperty 2）
	Set(Attacker, S::GetMaxHealthAttribute(), 10000.f);
	Info.RelatedProperty = 2;
	TestNearlyEqual(TEXT("rate x max health"), WuwaDamage::CalculateHurt(Info, 0.1f, A, T, false), static_cast<float>(Expected), 0.01f);
	Info.RelatedProperty = 7;

	// 收尾：按技能等级取倍率，在浮动上下限之间插值，向上取整
	Info.RateLv = {0.25f, 0.5f};
	Info.FluctuationLower = {0.9f};
	Info.FluctuationUpper = {1.1f};
	TestEqual(TEXT("final: level 2 rate, middle of the fluctuation, ceil"), WuwaDamage::CalculateFinal(Info, 2, A, T, false, 0.5f),
		FMath::CeilToFloat(static_cast<float>(Expected)));
	TestEqual(TEXT("final: lower bound"), WuwaDamage::CalculateFinal(Info, 2, A, T, false, 0.f), FMath::CeilToFloat(static_cast<float>(Expected * 0.9)));
	Info.FluctuationLower.Reset();
	Info.FluctuationUpper.Reset();
	TestEqual(TEXT("final: no fluctuation configured = 1"), WuwaDamage::CalculateFinal(Info, 1, A, T, false, 0.f),
		FMath::CeilToFloat(static_cast<float>(Expected * 0.5)));

	// 原作特殊公式还没移植：算 0 并警告
	AddExpectedMessagePlain(TEXT("is not ported yet"), ELogVerbosity::Warning);
	Info.FormulaType = 3;
	TestEqual(TEXT("unported formula type gives 0"), WuwaDamage::CalculateFinal(Info, 1, A, T, false, 0.f), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaDamageBulletHitTest, "Wuwa.Combat.Damage.BulletHitReducesHealth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaDamageBulletHitTest::RunTest(const FString& Parameters)
{
	using namespace WuwaDamageTest;
	using S = UWuwaAttributeSet;

	// 用真实伤害表的嘉贝普攻1（1208001001：热熔、共鸣技能类型、1 级倍率 0.2977）
	const FWuwaDamageInfo* Row = WuwaDamage::FindDamage(1208001001);
	if (!TestNotNull(TEXT("DT_Damage has 1208001001 (DamageTable in DefaultGame.ini)"), Row)) return false;
	TestEqual(TEXT("row element"), Row->Element, 2);
	TestEqual(TEXT("row type"), Row->Type, 1);
	TestNearlyEqual(TEXT("row level-1 rate"), WuwaDamage::GetLevelValue(Row->RateLv, 1, 0.f), 0.2977f, 1e-5f);

	// 测试用伤害倍率（DefaultGame.ini 的 DamageScale）用控制台变量临时压回 1，结束时恢复
	IConsoleVariable* ScaleVar = IConsoleManager::Get().FindConsoleVariable(TEXT("wuwa.Damage.Scale"));
	if (!TestNotNull(TEXT("wuwa.Damage.Scale exists"), ScaleVar)) return false;
	const float OldScale = ScaleVar->GetFloat();
	ScaleVar->Set(1.f, ECVF_SetByCode);
	ON_SCOPE_EXIT { ScaleVar->Set(OldScale, ECVF_SetByCode); };

	FArena Arena;
	UDataTable* Bullets = NewObject<UDataTable>(GetTransientPackage());
	Bullets->RowStruct = FWuwaBulletInfo::StaticStruct();
	FWuwaBulletInfo Shot;
	Shot.Shape = EWuwaBulletShape::Box;
	Shot.Size = FVector(150.f);
	Shot.BornOffset = FVector(200.f, 0.f, 88.f);
	Shot.Duration = 1.f;
	Shot.DamageId = 1208001001;
	Bullets->AddRow(TEXT("1"), Shot);

	// 攻击者：攻击 1000、1 级、暴击率 0（不会暴击）；怪物：初始属性生命上限 10000（BeginPlay 写入并回满）、防御 0、抗性 0
	AWuwaCharactorBase* Attacker = Arena.Spawn(FVector(0, 0, 100), TEXT("Player"), {Initial(S::GetAttackAttribute(), 1000.f), Initial(S::GetLevelAttribute(), 1.f)});
	Attacker->BulletTable = Bullets;
	AWuwaCharactorBase* Monster = Arena.Spawn(FVector(200, 0, 100), TEXT("Monster"), {Initial(S::GetMaxHealthAttribute(), 10000.f)});
	TestEqual(TEXT("initial attributes: health filled to max health"), Get(Monster, S::GetHealthAttribute()), 10000.f);
	TestEqual(TEXT("initial attributes: attack"), Get(Attacker, S::GetAttackAttribute()), 1000.f);

	UWuwaDamageSubsystem* Damage = UWuwaDamageSubsystem::Get(Arena.World);
	if (!TestNotNull(TEXT("damage subsystem exists in game worlds"), Damage)) return false;
	TArray<FWuwaDamageEvent> Events;
	Damage->OnDamage.AddLambda([&Events](const FWuwaDamageEvent& Event) { Events.Add(Event); });

	UWuwaBulletSubsystem* BulletSystem = UWuwaBulletSubsystem::Get(Arena.World);
	FWuwaBulletSpawnRequest Request;
	Request.Attacker = Attacker;
	Request.BulletId = TEXT("1");
	TestTrue(TEXT("bullet spawned"), BulletSystem->SpawnBullet(Request) != 0);
	for (int32 Frame = 0; Frame < 64 && BulletSystem->GetBullets().Num() > 0; ++Frame) BulletSystem->Tick(0.125f);

	// 0.2977 × 1000 × 防御系数 1 × 加成 1 × 抗性系数 1 = 297.7 → 向上取整 298
	if (TestEqual(TEXT("one damage event"), Events.Num(), 1))
	{
		TestEqual(TEXT("damage"), Events[0].Damage, 298.f);
		TestFalse(TEXT("no crit with crit rate 0"), Events[0].bCritical);
		TestTrue(TEXT("event victim"), Events[0].Victim.Get() == Monster);
		TestEqual(TEXT("event element"), Events[0].Element, 2);
		TestEqual(TEXT("event health after"), Events[0].VictimHealth, 10000.f - 298.f);
	}
	TestEqual(TEXT("monster health reduced"), Get(Monster, S::GetHealthAttribute()), 10000.f - 298.f);
	TestEqual(TEXT("attacker health untouched"), Get(Attacker, S::GetHealthAttribute()), Get(Attacker, S::GetMaxHealthAttribute()));
	return true;
}

#endif
