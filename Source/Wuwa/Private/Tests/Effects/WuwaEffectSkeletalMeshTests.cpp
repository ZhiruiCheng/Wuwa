#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Tickable.h"
#include "Game/Effect/System/WuwaEffectSystem.h"
#include "Game/EffectModel/Models/WuwaEffectModelSkeletalMesh.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"

namespace WuwaEffectSkeletalMeshTests
{
	UWorld* CreateTestWorld()
	{
		UWorld::InitializationValues Values;
		Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
		return UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	}

	USkeletalMeshComponent* FindEffectMesh(const UWorld* World, const USkeletalMesh* Mesh)
	{
		for (TObjectIterator<USkeletalMeshComponent> It; It; ++It)
		{
			if (It->GetWorld() == World && It->IsRegistered() && IsValid(*It) && It->GetSkeletalMeshAsset() == Mesh) return *It;
		}
		return nullptr;
	}
}

// 用项目里现成的角色网格和动画代替伞：只验证播放流程（原作 EffectModelSkeletalMeshSpec），不验证内容。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaEffectSkeletalMeshSpecTest,
	"Wuwa.Effect.SkeletalMesh.PlaysHiddenFirstThenEnds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaEffectSkeletalMeshSpecTest::RunTest(const FString& Parameters)
{
	using namespace WuwaEffectSkeletalMeshTests;
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Role/jiabei/Mesh/jiabei2_0.jiabei2_0"));
	UAnimSequence* Anim = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Characters/Role/jiabei/CommonAnim/Animations/jiabei2_0R2T1CalbrenaMd10011_LOD0_Skeleton_Attack01.jiabei2_0R2T1CalbrenaMd10011_LOD0_Skeleton_Attack01"));
	if (!TestNotNull(TEXT("Test mesh loads"), Mesh) || !TestNotNull(TEXT("Test animation loads"), Anim)) return false;

	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("Test world exists"), World)) return false;
	UWuwaEffectSystem* System = World->GetSubsystem<UWuwaEffectSystem>();

	TStrongObjectPtr<UWuwaEffectModelSkeletalMesh> Model(NewObject<UWuwaEffectModelSkeletalMesh>());
	Model->SkeletalMeshRef = Mesh;
	Model->AnimationRef = Anim;
	Model->HideFrames = 1;
	Model->StartTime = 0.5f;
	Model->Location = FWuwaEffectVectorCurve(FVector(0, 0, 30));
	FWuwaEffectSpawnRequest Request;
	Request.Model = Model.Get();
	Request.Transform = FTransform(FVector(100, 0, 0));

	const auto Handle = System->SpawnEffect(Request);
	TestTrue(TEXT("Skeletal mesh play starts"), System->IsEffectActive(Handle));
	USkeletalMeshComponent* Component = FindEffectMesh(World, Mesh);
	if (TestNotNull(TEXT("A skeletal mesh component is spawned"), Component))
	{
		TestFalse(TEXT("Hidden on the first frame (HideFrames + 1)"), Component->IsVisible());
		TestTrue(TEXT("Model location is added to the request transform"), Component->GetComponentLocation().Equals(FVector(100, 0, 30), 1e-3));
		TestEqual(TEXT("Plays the model's animation as a single node"), Component->GetAnimationMode(), EAnimationMode::AnimationSingleNode);
		FTickableGameObject::TickObjects(World, LEVELTICK_All, false, 0.05f);
		TestFalse(TEXT("Still hidden after one frame (HideFrames = 1)"), Component->IsVisible());
		FTickableGameObject::TickObjects(World, LEVELTICK_All, false, 0.05f);
		TestTrue(TEXT("Visible after HideFrames + 1 frames"), Component->IsVisible());
	}
	FTickableGameObject::TickObjects(World, LEVELTICK_All, false, 0.45f);
	TestFalse(TEXT("Play ends after StartTime"), System->IsEffectActive(Handle));
	TestNull(TEXT("Component is destroyed"), FindEffectMesh(World, Mesh));

	// 原作 OnInit：没有动画时初始化失败，不生成组件。
	Model->AnimationRef = nullptr;
	AddExpectedError(TEXT("skeletal mesh or animation is not configured"), EAutomationExpectedErrorFlags::Contains, 0);
	const auto Missing = System->SpawnEffect(Request);
	TestFalse(TEXT("Play without animation does not start"), System->IsEffectActive(Missing));
	World->DestroyWorld(false);
	return true;
}

#endif
