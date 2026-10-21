#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/EngineBaseTypes.h"
#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"
#include "Game/NewWorld/Combat/WuwaCreatureSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCreatureSubsystemTest, "Wuwa.Combat.Creature.RegistryAndRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCreatureSubsystemTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	// 让生成的 Actor 完成初始化（bActorInitialized），销毁时才会走 EndPlay，和真实游戏一样
	World->InitializeActorsForPlay(FURL());
	UWuwaCreatureSubsystem* Creatures = UWuwaCreatureSubsystem::Get(World);
	if (!TestNotNull(TEXT("game world has a creature registry"), Creatures)) return false;
	int32 Registered = 0, Unregistered = 0;
	Creatures->OnCreatureRegistered.AddLambda([&Registered](AWuwaCharactorBase*) { ++Registered; });
	Creatures->OnCreatureUnregistered.AddLambda([&Unregistered](AWuwaCharactorBase*) { ++Unregistered; });

	// BeginPlay 自动登记（测试世界不跑 BeginPlay，手动派发）
	auto Spawn = [World](const FVector& Location)
	{
		AWuwaCharactorBase* Creature = World->SpawnActor<AWuwaCharactorBase>(Location, FRotator::ZeroRotator);
		if (Creature) Creature->DispatchBeginPlay();
		return Creature;
	};
	AWuwaCharactorBase* Far = Spawn(FVector(1000.f, 0.f, 0.f));
	AWuwaCharactorBase* Near = Spawn(FVector(0.f, 300.f, 0.f));
	AWuwaCharactorBase* Here = Spawn(FVector(0.f, 0.f, 50.f));
	if (!TestTrue(TEXT("creatures spawned"), Far && Near && Here)) return false;
	TestEqual(TEXT("BeginPlay registers every character"), Creatures->GetCreatures().Num(), 3);
	TestEqual(TEXT("registered event per character"), Registered, 3);
	Creatures->Register(Near);
	TestEqual(TEXT("registering twice keeps one entry"), Creatures->GetCreatures().Num(), 3);

	// 范围查询：只要半径内的，按距离从近到远
	TArray<AWuwaCharactorBase*> Found;
	Creatures->GetCreaturesInRange(FVector::ZeroVector, 500.f, Found);
	if (TestEqual(TEXT("two creatures within 5m"), Found.Num(), 2))
	{
		TestTrue(TEXT("nearest first"), Found[0] == Here && Found[1] == Near);
	}
	Creatures->GetCreaturesInRange(FVector::ZeroVector, 1000.f, Found);
	TestEqual(TEXT("radius is inclusive"), Found.Num(), 3);
	Creatures->GetCreaturesInRange(FVector(5000.f, 0.f, 0.f), 100.f, Found);
	TestEqual(TEXT("nothing around an empty spot"), Found.Num(), 0);

	// EndPlay 自动注销；被销毁的单位查询时跳过
	Near->Destroy();
	TestEqual(TEXT("EndPlay unregisters"), Creatures->GetCreatures().Num(), 2);
	TestEqual(TEXT("unregistered event"), Unregistered, 1);
	Creatures->GetCreaturesInRange(FVector::ZeroVector, 2000.f, Found);
	TestFalse(TEXT("destroyed creature is not returned"), Found.Contains(Near));

	// 动画编辑器预览世界里没有登记表
	UWorld* Preview = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	ON_SCOPE_EXIT { Preview->DestroyWorld(false); };
	TestNull(TEXT("editor preview world has no creature registry"), UWuwaCreatureSubsystem::Get(Preview));
	return true;
}

#endif
