#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/CapsuleComponent.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Game/NewWorld/Bullet/WuwaBulletSubsystem.h"
#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"

namespace WuwaBulletSubsystemTest
{
	FWuwaBulletInfo MakeBox(float Duration, EWuwaBulletFollowType Follow)
	{
		FWuwaBulletInfo Info;
		Info.Shape = EWuwaBulletShape::Box;
		Info.Size = FVector(50.f);
		Info.BornOffset = FVector(100.f, 0.f, 0.f);
		Info.Duration = Duration;
		Info.FollowType = Follow;
		return Info;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaBulletCollisionWindowTest, "Wuwa.Combat.Bullet.CollisionWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaBulletCollisionWindowTest::RunTest(const FString& Parameters)
{
	FWuwaBulletInfo Info = WuwaBulletSubsystemTest::MakeBox(1.f, EWuwaBulletFollowType::Fixed);
	// 默认（延迟 -1、时长 -1）：出生就判定，一直判定
	TestTrue(TEXT("no delay -> active at birth"), WuwaBullet::IsCollisionActive(Info, 0.f));
	TestTrue(TEXT("no duration -> active until the end"), WuwaBullet::IsCollisionActive(Info, 0.99f));
	// 延迟 0.1、时长 0.2：窗口 [0.1, 0.3]，两端包含
	Info.CollisionDelay = 0.1f;
	Info.CollisionDuration = 0.2f;
	TestFalse(TEXT("before the delay"), WuwaBullet::IsCollisionActive(Info, 0.05f));
	TestTrue(TEXT("window start"), WuwaBullet::IsCollisionActive(Info, 0.1f));
	TestTrue(TEXT("window end"), WuwaBullet::IsCollisionActive(Info, 0.3f));
	TestFalse(TEXT("after the window"), WuwaBullet::IsCollisionActive(Info, 0.31f));
	// 没有碰撞（尺寸为 0，只当父子弹用）永远不判定
	Info.Size = FVector::ZeroVector;
	TestFalse(TEXT("no collision -> never active"), WuwaBullet::IsCollisionActive(Info, 0.2f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaBulletSpawnerTest, "Wuwa.Combat.Bullet.Spawner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaBulletSpawnerTest::RunTest(const FString& Parameters)
{
	using namespace WuwaBulletSubsystemTest;
	UDataTable* Table = NewObject<UDataTable>(GetTransientPackage());
	Table->RowStruct = FWuwaBulletInfo::StaticStruct();
	FWuwaBulletInfo Windowed = MakeBox(0.5f, EWuwaBulletFollowType::Fixed);
	Windowed.CollisionDelay = 0.1f;
	Windowed.CollisionDuration = 0.2f;
	Table->AddRow(TEXT("1000"), Windowed);
	Table->AddRow(TEXT("1001"), MakeBox(1.f, EWuwaBulletFollowType::FollowBone));
	FWuwaBulletInfo SkillBound = MakeBox(1.f, EWuwaBulletFollowType::Fixed);
	SkillBound.bDestroyOnSkillEnd = true;
	Table->AddRow(TEXT("1002"), SkillBound);

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	UWuwaBulletSubsystem* Bullets = UWuwaBulletSubsystem::Get(World);
	if (!TestNotNull(TEXT("bullet subsystem"), Bullets)) return false;
	TArray<TPair<int32, EWuwaBulletEndReason>> Ended;
	Bullets->OnBulletEnded.AddLambda([&Ended](const FWuwaBullet& Bullet, EWuwaBulletEndReason Reason) { Ended.Emplace(Bullet.Handle, Reason); });

	AWuwaCharactorBase* Attacker = World->SpawnActor<AWuwaCharactorBase>(FVector(0.f, 0.f, 100.f), FRotator(0.f, 90.f, 0.f));
	if (!TestNotNull(TEXT("attacker"), Attacker)) return false;
	Attacker->BulletTable = Table;
	const float HalfHeight = Attacker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FWuwaBulletSpawnRequest Request;
	Request.Attacker = Attacker;

	// 出生：偏移按发射者朝向（朝 +Y）旋转，Z 从脚底算
	Request.BulletId = TEXT("1000");
	const int32 Fixed = Bullets->SpawnBullet(Request);
	const FWuwaBullet* Bullet = Bullets->FindBullet(Fixed);
	if (!TestNotNull(TEXT("bullet spawned"), Bullet)) return false;
	TestTrue(TEXT("born in front of the attacker, from the feet"),
		Bullet->Transform.GetLocation().Equals(FVector(0.f, 100.f, 100.f - HalfHeight), 0.01));
	TestFalse(TEXT("collision waits for the delay"), Bullet->IsCollisionActive());

	// 跟随：发射者移动后，FollowBone（没配骨骼 = 跟网格）的子弹跟着走，Fixed 的留在原地
	Request.BulletId = TEXT("1001");
	const int32 Following = Bullets->SpawnBullet(Request);
	Attacker->SetActorLocation(FVector(500.f, 0.f, 100.f));
	Bullets->Tick(0.15f);
	TestTrue(TEXT("fixed bullet stays"), Bullets->FindBullet(Fixed)->Transform.GetLocation().Equals(FVector(0.f, 100.f, 100.f - HalfHeight), 0.01));
	TestTrue(TEXT("following bullet moves with the attacker"),
		Bullets->FindBullet(Following)->Transform.GetLocation().Equals(FVector(500.f, 100.f, 100.f - HalfHeight), 0.01));
	TestTrue(TEXT("collision window open at 0.15s"), Bullets->FindBullet(Fixed)->IsCollisionActive());
	Bullets->Tick(0.2f);
	TestFalse(TEXT("collision window closed at 0.35s"), Bullets->FindBullet(Fixed)->IsCollisionActive());

	// 存活时间：0.5 秒到点销毁（原因 TimeOut），1 秒的还在
	Bullets->Tick(0.2f);
	TestNull(TEXT("timed out bullet removed"), Bullets->FindBullet(Fixed));
	TestTrue(TEXT("ended by timeout"), Ended.Contains(TPair<int32, EWuwaBulletEndReason>(Fixed, EWuwaBulletEndReason::TimeOut)));
	TestNotNull(TEXT("longer bullet still alive"), Bullets->FindBullet(Following));

	// 不生成：表里没有的 ID、勾了技能结束销毁但不在技能里、客户端
	AddExpectedMessagePlain(TEXT("is not in"), ELogVerbosity::Warning);
	Request.BulletId = TEXT("9999");
	TestEqual(TEXT("unknown bullet id -> no bullet"), Bullets->SpawnBullet(Request), 0);
	Request.BulletId = TEXT("1002");
	TestEqual(TEXT("destroy-on-skill-end bullet outside a skill -> no bullet"), Bullets->SpawnBullet(Request), 0);
	Request.BulletId = TEXT("1000");
	Attacker->SetRole(ROLE_SimulatedProxy);
	TestEqual(TEXT("no authority -> no bullet"), Bullets->SpawnBullet(Request), 0);
	Attacker->SetRole(ROLE_Authority);

	// 发射者没了：子弹跟着没（原因 AttackerGone）
	Attacker->Destroy();
	Bullets->Tick(0.01f);
	TestNull(TEXT("bullet removed with its attacker"), Bullets->FindBullet(Following));
	TestTrue(TEXT("ended because the attacker is gone"),
		Ended.Contains(TPair<int32, EWuwaBulletEndReason>(Following, EWuwaBulletEndReason::AttackerGone)));
	TestEqual(TEXT("no bullets left"), Bullets->GetBullets().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaBulletChildrenTest, "Wuwa.Combat.Bullet.ChildrenAndSpawnTags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaBulletChildrenTest::RunTest(const FString& Parameters)
{
	using namespace WuwaBulletSubsystemTest;
	const FGameplayTag Transformed = FGameplayTag::RequestGameplayTag(TEXT("Role.Jiabei.State.Transformed"));
	UDataTable* Table = NewObject<UDataTable>(GetTransientPackage());
	Table->RowStruct = FWuwaBulletInfo::StaticStruct();
	// 父子弹：0.3 秒后生成两颗子子弹，隔 0.2 秒；变身中不生成（嘉贝大招起爆）
	FWuwaBulletInfo Parent = MakeBox(1.f, EWuwaBulletFollowType::Fixed);
	FWuwaBulletChild& Spawn = Parent.Children.AddDefaulted_GetRef();
	Spawn.BulletId = TEXT("2001");
	Spawn.Delay = 0.3f;
	Spawn.Count = 2;
	Spawn.Interval = 0.2f;
	Parent.SpawnBlockedTags.AddTag(Transformed);
	Table->AddRow(TEXT("2000"), Parent);
	// 子子弹：从父子弹位置往上 50
	FWuwaBulletInfo Child = MakeBox(2.f, EWuwaBulletFollowType::Fixed);
	Child.BornStandard = EWuwaBulletBornStandard::Parent;
	Child.BornOffset = FVector(0.f, 0.f, 50.f);
	Table->AddRow(TEXT("2001"), Child);
	// 变身版：变身中才生成
	FWuwaBulletInfo TransformedOnly = MakeBox(1.f, EWuwaBulletFollowType::Fixed);
	TransformedOnly.SpawnRequiredTags.AddTag(Transformed);
	Table->AddRow(TEXT("2002"), TransformedOnly);

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	UWuwaBulletSubsystem* Bullets = UWuwaBulletSubsystem::Get(World);
	if (!TestNotNull(TEXT("bullet subsystem"), Bullets)) return false;
	AWuwaCharactorBase* Attacker = World->SpawnActor<AWuwaCharactorBase>(FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("attacker"), Attacker)) return false;
	Attacker->BulletTable = Table;
	UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Attacker);
	if (!TestNotNull(TEXT("attacker ability system"), AbilitySystem)) return false;
	FWuwaBulletSpawnRequest Request;
	Request.Attacker = Attacker;
	Request.SkillId = 7;

	Request.BulletId = TEXT("2002");
	TestEqual(TEXT("required tag missing -> no bullet"), Bullets->SpawnBullet(Request), 0);
	Request.BulletId = TEXT("2000");
	const int32 ParentHandle = Bullets->SpawnBullet(Request);
	if (!TestNotEqual(TEXT("parent spawned"), ParentHandle, 0)) return false;
	const FVector ParentLocation = Bullets->FindBullet(ParentHandle)->Transform.GetLocation();

	auto CountChildren = [Bullets]()
	{
		return Bullets->GetBullets().FilterByPredicate([](const FWuwaBullet& B) { return B.BulletId == TEXT("2001"); }).Num();
	};
	Bullets->Tick(0.25f);
	TestEqual(TEXT("no child before the delay"), CountChildren(), 0);
	Bullets->Tick(0.1f);
	TestEqual(TEXT("first child at 0.3s"), CountChildren(), 1);
	const FWuwaBullet* First = Bullets->GetBullets().FindByPredicate([](const FWuwaBullet& B) { return B.BulletId == TEXT("2001"); });
	TestTrue(TEXT("child born at the parent, offset up"), First->Transform.GetLocation().Equals(ParentLocation + FVector(0.f, 0.f, 50.f), 0.01));
	TestEqual(TEXT("child keeps the parent's skill"), First->SkillId, 7);
	Bullets->Tick(0.2f);
	TestEqual(TEXT("second child after the interval"), CountChildren(), 2);
	Bullets->Tick(0.3f);
	TestEqual(TEXT("no more than Count children"), CountChildren(), 2);

	AbilitySystem->AddLooseGameplayTag(Transformed);
	TestEqual(TEXT("blocked tag present -> no bullet"), Bullets->SpawnBullet(Request), 0);
	Request.BulletId = TEXT("2002");
	TestNotEqual(TEXT("required tag present -> bullet"), Bullets->SpawnBullet(Request), 0);
	return true;
}

#endif
