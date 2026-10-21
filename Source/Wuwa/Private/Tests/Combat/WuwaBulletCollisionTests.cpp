#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Game/NewWorld/Bullet/WuwaBulletSubsystem.h"
#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaBulletShapeTest, "Wuwa.Combat.Bullet.Collision.RingAndSector",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaBulletShapeTest::RunTest(const FString& Parameters)
{
	// 碰撞体用竖直胶囊的包围盒（半径 34、半高 88）
	const FVector Capsule(34.f, 34.f, 88.f);
	const FQuat Up = FQuat::Identity;
	// 圆环：外半径 300、内半径 100、半高 100
	const FVector Ring(300.f, 100.f, 100.f);
	TestTrue(TEXT("ring: between inner and outer radius"), WuwaBullet::IsBoxInRing(FVector::ZeroVector, Ring, FVector(200, 0, 0), Capsule, Up));
	TestFalse(TEXT("ring: entirely inside the inner radius"), WuwaBullet::IsBoxInRing(FVector::ZeroVector, Ring, FVector(0, 0, 0), Capsule, Up));
	TestFalse(TEXT("ring: outside the outer radius"), WuwaBullet::IsBoxInRing(FVector::ZeroVector, Ring, FVector(500, 0, 0), Capsule, Up));
	TestFalse(TEXT("ring: above the slab"), WuwaBullet::IsBoxInRing(FVector::ZeroVector, Ring, FVector(200, 0, 300), Capsule, Up));
	TestTrue(TEXT("ring: straddling the outer edge"), WuwaBullet::IsBoxInRing(FVector::ZeroVector, Ring, FVector(310, 0, 0), Capsule, Up));
	// 扇形：半径 400、张角 90°、半高 100，朝 +X
	const FVector Sector(400.f, 90.f, 100.f);
	TestTrue(TEXT("sector: straight ahead"), WuwaBullet::IsBoxInSector(FVector::ZeroVector, Sector, Up, FVector(200, 0, 0), Capsule, Up));
	TestFalse(TEXT("sector: off to the side (~56°)"), WuwaBullet::IsBoxInSector(FVector::ZeroVector, Sector, Up, FVector(200, 300, 0), Capsule, Up));
	TestFalse(TEXT("sector: behind"), WuwaBullet::IsBoxInSector(FVector::ZeroVector, Sector, Up, FVector(-200, 0, 0), Capsule, Up));
	TestFalse(TEXT("sector: beyond the radius"), WuwaBullet::IsBoxInSector(FVector::ZeroVector, Sector, Up, FVector(600, 0, 0), Capsule, Up));
	TestTrue(TEXT("sector: follows the shape rotation"),
		WuwaBullet::IsBoxInSector(FVector::ZeroVector, Sector, FRotator(0, 90, 0).Quaternion(), FVector(0, 200, 0), Capsule, Up));
	// 很窄的扇形（10°）：盒子的角都在两侧之外，但左右连线横跨扇形前方 → 命中（原作 Az）
	TestTrue(TEXT("narrow sector: box straddles the thin wedge"),
		WuwaBullet::IsBoxInSector(FVector::ZeroVector, FVector(400.f, 10.f, 100.f), Up, FVector(200, 0, 0), Capsule, Up));
	return true;
}

namespace WuwaBulletCollisionTest
{
	/** 发射者在原点朝 +X（Player 阵营），子弹表只有一行 "1"（在发射者前方 200 的盒子），按需改行。 */
	struct FArena
	{
		UWorld* World = nullptr;
		UDataTable* Table = nullptr;
		AWuwaCharactorBase* Attacker = nullptr;
		UWuwaBulletSubsystem* Bullets = nullptr;
		TArray<TPair<AActor*, int32>> Hits;

		explicit FArena(const FWuwaBulletInfo& Info)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			World->InitializeActorsForPlay(FURL());
			Table = NewObject<UDataTable>(GetTransientPackage());
			Table->RowStruct = FWuwaBulletInfo::StaticStruct();
			Table->AddRow(TEXT("1"), Info);
			Attacker = Spawn(FVector(0, 0, 100), TEXT("Player"));
			Attacker->BulletTable = Table;
			Bullets = UWuwaBulletSubsystem::Get(World);
			Bullets->OnBulletHit.AddLambda([this](const FWuwaBulletHit& Hit) { Hits.Emplace(Hit.Victim.Get(), Hit.VictimHitIndex); });
		}
		~FArena() { World->DestroyWorld(false); }

		AWuwaCharactorBase* Spawn(const FVector& Location, FName Camp)
		{
			AWuwaCharactorBase* Actor = World->SpawnActor<AWuwaCharactorBase>(Location, FRotator::ZeroRotator);
			Actor->Camp = Camp;
			return Actor;
		}
		void Fire()
		{
			FWuwaBulletSpawnRequest Request;
			Request.Attacker = Attacker;
			Request.BulletId = TEXT("1");
			Bullets->SpawnBullet(Request);
		}
		/** 按固定步长推进到子弹消失（步长取二进制精确的 0.125 秒，阶段计算没有浮点误差）。 */
		void RunToEnd(float Step = 0.125f)
		{
			for (int32 Frame = 0; Frame < 64 && Bullets->GetBullets().Num() > 0; ++Frame) Bullets->Tick(Step);
		}
		int32 HitsOn(const AActor* Victim) const
		{
			return Hits.FilterByPredicate([Victim](const TPair<AActor*, int32>& Hit) { return Hit.Key == Victim; }).Num();
		}
	};

	FWuwaBulletInfo Box()
	{
		FWuwaBulletInfo Info;
		Info.Shape = EWuwaBulletShape::Box;
		Info.Size = FVector(150.f, 150.f, 150.f);
		Info.BornOffset = FVector(200.f, 0.f, 88.f);   // 从脚底算，抬到胶囊中心高度
		Info.Duration = 1.f;
		return Info;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaBulletHitCountTest, "Wuwa.Combat.Bullet.Collision.HitRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaBulletHitCountTest::RunTest(const FString& Parameters)
{
	using namespace WuwaBulletCollisionTest;
	{
		// 默认：每个单位 1 次；只打敌方，友方、范围外的不打
		FArena Arena(Box());
		AWuwaCharactorBase* Monster = Arena.Spawn(FVector(200, 0, 100), TEXT("Monster"));
		AWuwaCharactorBase* Ally = Arena.Spawn(FVector(200, 80, 100), TEXT("Player"));
		AWuwaCharactorBase* Far = Arena.Spawn(FVector(800, 0, 100), TEXT("Monster"));
		Arena.Fire();
		Arena.RunToEnd();
		TestEqual(TEXT("monster in the box is hit once"), Arena.HitsOn(Monster), 1);
		TestEqual(TEXT("ally is not hit"), Arena.HitsOn(Ally), 0);
		TestEqual(TEXT("monster outside the box is not hit"), Arena.HitsOn(Far), 0);
		TestEqual(TEXT("attacker is not hit by its own enemy-only bullet"), Arena.HitsOn(Arena.Attacker), 0);
	}
	{
		// 判定窗口：延迟 0.5 秒开，窗口外进来的不算；窗口内进来才算
		FWuwaBulletInfo Info = Box();
		Info.CollisionDelay = 0.5f;
		FArena Arena(Info);
		AWuwaCharactorBase* Monster = Arena.Spawn(FVector(200, 0, 100), TEXT("Monster"));
		Arena.Fire();
		Arena.Bullets->Tick(0.25f);
		TestEqual(TEXT("no hit before the collision delay"), Arena.HitsOn(Monster), 0);
		Arena.RunToEnd();
		TestEqual(TEXT("hit once the window opens"), Arena.HitsOn(Monster), 1);
	}
	{
		// 对自己（开枪顿帧子弹）：只打发射者
		FWuwaBulletInfo Info = Box();
		Info.BornOffset = FVector(0, 0, 88.f);
		Info.HitCamp = static_cast<int32>(EWuwaBulletCamp::Self);
		FArena Arena(Info);
		AWuwaCharactorBase* Monster = Arena.Spawn(FVector(60, 0, 100), TEXT("Monster"));
		Arena.Fire();
		Arena.RunToEnd();
		TestEqual(TEXT("self bullet hits the attacker"), Arena.HitsOn(Arena.Attacker), 1);
		TestEqual(TEXT("self bullet does not hit enemies"), Arena.HitsOn(Monster), 0);
	}
	{
		// 命中个数 1：两个怪只打一个
		FWuwaBulletInfo Info = Box();
		Info.VictimCount = 1;
		FArena Arena(Info);
		AWuwaCharactorBase* A = Arena.Spawn(FVector(200, -60, 100), TEXT("Monster"));
		AWuwaCharactorBase* B = Arena.Spawn(FVector(200, 60, 100), TEXT("Monster"));
		Arena.Fire();
		Arena.RunToEnd();
		TestEqual(TEXT("victim count 1 -> one victim"), Arena.HitsOn(A) + Arena.HitsOn(B), 1);
	}
	{
		// 整颗子弹共用间隔 0.25 秒、次数不限：t = 0.125 / 0.25 / 0.5 / 0.75 / 1.0 各一次 → 5 次
		FWuwaBulletInfo Info = Box();
		Info.HitsPerVictim = -1;
		Info.HitInterval = 0.25f;
		FArena Arena(Info);
		AWuwaCharactorBase* Monster = Arena.Spawn(FVector(200, 0, 100), TEXT("Monster"));
		Arena.Fire();
		Arena.RunToEnd();
		TestEqual(TEXT("shared interval: one hit per stage"), Arena.HitsOn(Monster), 5);
	}
	{
		// 每个单位各自计间隔 0.25 秒：t = 0.125 / 0.375 / 0.625 / 0.875 → 4 次
		FWuwaBulletInfo Info = Box();
		Info.HitsPerVictim = -1;
		Info.HitInterval = 0.25f;
		Info.bIntervalPerVictim = true;
		FArena Arena(Info);
		AWuwaCharactorBase* Monster = Arena.Spawn(FVector(200, 0, 100), TEXT("Monster"));
		Arena.Fire();
		Arena.RunToEnd();
		TestEqual(TEXT("per-victim interval: one hit per interval since the last hit"), Arena.HitsOn(Monster), 4);
	}
	{
		// 总作用次数 3、没有间隔、次数不限：每帧一次，到 3 次停
		FWuwaBulletInfo Info = Box();
		Info.HitsPerVictim = -1;
		Info.TotalHitLimit = 3;
		FArena Arena(Info);
		AWuwaCharactorBase* Monster = Arena.Spawn(FVector(200, 0, 100), TEXT("Monster"));
		Arena.Fire();
		Arena.RunToEnd();
		TestEqual(TEXT("total hit limit"), Arena.HitsOn(Monster), 3);
	}
	{
		// 扇形 90°：正前方的打到，侧面的打不到（粗检测盒子覆盖到了，精确检测排除）
		FWuwaBulletInfo Info = Box();
		Info.Shape = EWuwaBulletShape::Sector;
		Info.Size = FVector(400.f, 90.f, 150.f);
		Info.BornOffset = FVector(0, 0, 88.f);
		FArena Arena(Info);
		AWuwaCharactorBase* Ahead = Arena.Spawn(FVector(250, 0, 100), TEXT("Monster"));
		AWuwaCharactorBase* Side = Arena.Spawn(FVector(150, 270, 100), TEXT("Monster"));
		Arena.Fire();
		Arena.RunToEnd();
		TestEqual(TEXT("sector hits the monster ahead"), Arena.HitsOn(Ahead), 1);
		TestEqual(TEXT("sector misses the monster at the side"), Arena.HitsOn(Side), 0);
	}
	return true;
}

#endif
