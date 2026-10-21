#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Tickable.h"
#include "Game/Effect/System/WuwaEffectSystem.h"
#include "Game/EffectModel/Models/WuwaEffectModelGhost.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"

namespace WuwaEffectGhostTests
{
	TArray<UPoseableMeshComponent*> LiveGhosts(const UWorld* World, bool bVisibleOnly)
	{
		TArray<UPoseableMeshComponent*> Result;
		for (TObjectIterator<UPoseableMeshComponent> It; It; ++It)
		{
			if (It->GetWorld() == World && It->IsRegistered() && IsValid(*It) && (!bVisibleOnly || It->IsVisible())) Result.Add(*It);
		}
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaEffectGhostSpecTest,
	"Wuwa.Effect.Ghost.SpawnsFadesAndEnds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaEffectGhostSpecTest::RunTest(const FString& Parameters)
{
	using namespace WuwaEffectGhostTests;
	USkeletalMesh* Cube = LoadObject<USkeletalMesh>(nullptr, TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube"));
	if (!TestNotNull(TEXT("Engine skeletal cube loads"), Cube)) return false;

	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("Test world exists"), World)) return false;
	UWuwaEffectSystem* System = World->GetSubsystem<UWuwaEffectSystem>();

	USkeletalMeshComponent* Source = NewObject<USkeletalMeshComponent>(World);
	Source->SetSkeletalMeshAsset(Cube);
	Source->SetWorldLocation(FVector(100, 200, 0));
	Source->RegisterComponentWithWorld(World);

	TStrongObjectPtr<UWuwaEffectModelGhost> Model(NewObject<UWuwaEffectModelGhost>());
	Model->MaterialRef = UMaterial::GetDefaultMaterial(MD_Surface);
	// 横轴是剩余寿命比例：线性 0→1，便于检查取样点。
	Model->AlphaCurve.bUseCurve = true;
	FRichCurve& Curve = Model->AlphaCurve.Curve.EditorCurveData;
	Curve.SetKeyInterpMode(Curve.AddKey(0.f, 0.f), RCIM_Linear);
	Curve.SetKeyInterpMode(Curve.AddKey(1.f, 1.f), RCIM_Linear);

	// 原作嘉贝莉娜 AM_Charge 的参数：10 个/秒，单个寿命 0.6 秒。
	FWuwaEffectSpawnRequest Request;
	Request.Model = Model.Get();
	Request.AttachTo = Source;
	Request.Ghost.SpawnRate = 10.f;
	Request.Ghost.GhostLifeTime = 0.6f;
	const auto Handle = System->SpawnEffect(Request);
	TestTrue(TEXT("Ghost play starts"), System->IsEffectActive(Handle));

	TArray<UPoseableMeshComponent*> Ghosts = LiveGhosts(World, true);
	if (TestEqual(TEXT("First ghost spawns immediately"), Ghosts.Num(), 1))
	{
		UPoseableMeshComponent* Ghost = Ghosts[0];
		TestTrue(TEXT("Ghost copies the source mesh"), Ghost->GetSkinnedAsset() == Cube);
		TestTrue(TEXT("Ghost is placed at the source"), Ghost->GetComponentLocation().Equals(FVector(100, 200, 0), 1e-3));
		TestTrue(TEXT("All material slots use the ghost material"), Ghost->GetNumMaterials() > 0 && Ghost->GetMaterial(0) == Model->MaterialRef);
		FTickableGameObject::TickObjects(World, LEVELTICK_All, false, 0.15f);
		TestEqual(TEXT("Alpha follows the remaining-life ratio"), Ghost->GetCustomPrimitiveData().Data[0], 0.75f, 1e-3f);
	}
	TestEqual(TEXT("A second ghost spawns after one interval"), LiveGhosts(World, true).Num(), 2);

	System->StopEffect(Handle, false);
	TestTrue(TEXT("Non-immediate stop lets existing ghosts fade"), System->IsEffectActive(Handle));
	FTickableGameObject::TickObjects(World, LEVELTICK_All, false, 0.3f);
	TestEqual(TEXT("No ghosts spawn after stop"), LiveGhosts(World, false).Num(), 2);
	FTickableGameObject::TickObjects(World, LEVELTICK_All, false, 0.4f);
	TestFalse(TEXT("Play ends once the last ghost expired"), System->IsEffectActive(Handle));
	TestEqual(TEXT("Ghost components are destroyed"), LiveGhosts(World, false).Num(), 0);

	World->DestroyWorld(false);
	return true;
}

#endif
