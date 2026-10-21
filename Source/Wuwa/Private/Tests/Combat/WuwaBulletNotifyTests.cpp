#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Animation/AnimSequence.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_SpawnBullet.h"
#include "Game/NewWorld/Bullet/WuwaBulletSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaBulletNotifySendsRequestsTest, "Wuwa.Combat.Bullet.Notify.SendsRequests",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaBulletNotifySendsRequestsTest::RunTest(const FString& Parameters)
{
	UWuwaAnimNotify_SpawnBullet* Notify = NewObject<UWuwaAnimNotify_SpawnBullet>();
	TestEqual(TEXT("empty notify name"), Notify->GetNotifyName(), FString(TEXT("Spawn Bullet")));
	Notify->BulletIds = {TEXT("1205001000"), NAME_None, TEXT("1205001001")};
	TestEqual(TEXT("notify name lists the first bullet and the count"), Notify->GetNotifyName(), FString(TEXT("Bullet 1205001000 +2")));
	UAnimSequence* Animation = NewObject<UAnimSequence>(GetTransientPackage(), TEXT("TestBulletAnim"));

	// 游戏世界：每个非空 ID 一个请求，带发射者 / 动画；没有 GA 在播蒙太奇时技能 ID 为 0
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	UWuwaBulletSubsystem* Bullets = UWuwaBulletSubsystem::Get(World);
	if (!TestNotNull(TEXT("game world has a bullet subsystem"), Bullets)) return false;
	TArray<FWuwaBulletSpawnRequest> Received;
	Bullets->OnBulletRequested.AddLambda([&Received](const FWuwaBulletSpawnRequest& Request) { Received.Add(Request); });

	ASkeletalMeshActor* Attacker = World->SpawnActor<ASkeletalMeshActor>();
	// 这个发射者不是角色、没有子弹表：请求照常发出，子弹系统打警告后不生成
	AddExpectedMessagePlain(TEXT("has no BulletTable"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 2);
	Notify->Notify(Attacker->GetSkeletalMeshComponent(), Animation, FAnimNotifyEventReference());
	if (TestEqual(TEXT("one request per non-empty bullet id"), Received.Num(), 2))
	{
		TestEqual(TEXT("first bullet id"), Received[0].BulletId, FName(TEXT("1205001000")));
		TestEqual(TEXT("second bullet id"), Received[1].BulletId, FName(TEXT("1205001001")));
		TestTrue(TEXT("attacker is the mesh owner"), Received[0].Attacker.Get() == Attacker);
		TestEqual(TEXT("no animating ability -> skill 0"), Received[0].SkillId, 0);
		TestTrue(TEXT("source animation recorded"), Received[0].SourceAnimation.Get() == Animation);
	}

	// 没有发射者（网格不属于任何 Actor）时不发请求
	Received.Reset();
	Notify->Notify(nullptr, Animation, FAnimNotifyEventReference());
	TestEqual(TEXT("no mesh -> no request"), Received.Num(), 0);

	// 动画编辑器预览世界里没有子弹系统
	UWorld* Preview = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	ON_SCOPE_EXIT { Preview->DestroyWorld(false); };
	TestNull(TEXT("editor preview world has no bullet subsystem"), UWuwaBulletSubsystem::Get(Preview));
	return true;
}

#endif
