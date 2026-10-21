#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Tickable.h"
#include "Game/Effect/System/WuwaEffectSystem.h"
#include "Game/EffectModel/Models/WuwaCharacterMaterialControllerData.h"
#include "Game/EffectModel/Models/WuwaEffectModelMaterialController.h"
#include "UObject/StrongObjectPtr.h"

namespace WuwaMaterialControllerTests
{
	float Scalar(const UMaterialInstanceDynamic* Material, FName Name)
	{
		const FScalarParameterValue* Found = Material->ScalarParameterValues.FindByPredicate(
			[Name](const FScalarParameterValue& V) { return V.ParameterInfo.Name == Name; });
		return Found ? Found->ParameterValue : -1.f;
	}

	FLinearColor Vector(const UMaterialInstanceDynamic* Material, FName Name)
	{
		const FVectorParameterValue* Found = Material->VectorParameterValues.FindByPredicate(
			[Name](const FVectorParameterValue& V) { return V.ParameterInfo.Name == Name; });
		return Found ? Found->ParameterValue : FLinearColor(-1, -1, -1, -1);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaEffectMaterialControllerSpecTest,
	"Wuwa.Effect.MaterialController.RimFollowsTimelineAndReverts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaEffectMaterialControllerSpecTest::RunTest(const FString& Parameters)
{
	using namespace WuwaMaterialControllerTests;
	USkeletalMesh* Cube = LoadObject<USkeletalMesh>(nullptr, TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube"));
	if (!TestNotNull(TEXT("Engine skeletal cube loads"), Cube)) return false;
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("Test world exists"), World)) return false;
	UWuwaEffectSystem* System = World->GetSubsystem<UWuwaEffectSystem>();

	AActor* Character = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Character actor spawns"), Character)) return false;
	USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(Character);
	Mesh->SetSkeletalMeshAsset(Cube);
	Mesh->SetMaterial(0, UMaterial::GetDefaultMaterial(MD_Surface));
	Character->AddInstanceComponent(Mesh);
	Mesh->RegisterComponent();

	// 只有开始段（1 秒）：强度 1→0 线性，范围与颜色为常量。
	TStrongObjectPtr<UWuwaCharacterMaterialControllerData> Data(NewObject<UWuwaCharacterMaterialControllerData>());
	Data->StartTime = 1.f;
	Data->bUseRim = true;
	Data->bRimUseTex = true;
	Data->RimRange.Start = FWuwaEffectFloatCurve(3.f);
	Data->RimColor.Start = FWuwaEffectColorCurve(FLinearColor(0.8f, 1.2f, 5.f, 0.f));
	Data->RimIntensity.Start.bUseCurve = true;
	FRichCurve& Curve = Data->RimIntensity.Start.Curve.EditorCurveData;
	Curve.SetKeyInterpMode(Curve.AddKey(0.f, 1.f), RCIM_Linear);
	Curve.SetKeyInterpMode(Curve.AddKey(1.f, 0.f), RCIM_Linear);
	TStrongObjectPtr<UWuwaEffectModelMaterialController> Model(NewObject<UWuwaEffectModelMaterialController>());
	Model->ControllerData = Data.Get();

	FWuwaEffectSpawnRequest Request;
	Request.Model = Model.Get();
	Request.AttachTo = Mesh;
	const auto Handle = System->SpawnEffect(Request);
	TestTrue(TEXT("Controller play starts"), System->IsEffectActive(Handle));
	UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
	if (!TestNotNull(TEXT("Character material slot becomes a dynamic instance"), Material)) return false;

	FTickableGameObject::TickObjects(World, LEVELTICK_All, false, 0.25f);
	TestEqual(TEXT("UseRim is switched on"), Scalar(Material, TEXT("E_Rim_UseRim")), 1.f);
	TestEqual(TEXT("UseTex follows the data"), Scalar(Material, TEXT("E_Rim_UseTex")), 1.f);
	TestEqual(TEXT("Range follows the start group"), Scalar(Material, TEXT("E_Rim_RimRange")), 3.f);
	TestEqual(TEXT("Intensity is sampled at start-phase progress 0.25"), Scalar(Material, TEXT("E_Rim_Intensity")), 0.75f, 1e-4f);
	TestTrue(TEXT("Colour follows the start group"), Vector(Material, TEXT("E_Rim_RimColor")).Equals(FLinearColor(0.8f, 1.2f, 5.f, 0.f)));
	TestTrue(TEXT("Unset channel means all RGB"), Vector(Material, TEXT("E_Rim_Channel")).Equals(FLinearColor(-1.f, 0.f, 0.f, 0.f)));

	FTickableGameObject::TickObjects(World, LEVELTICK_All, false, 0.25f);
	TestEqual(TEXT("Intensity keeps following the curve"), Scalar(Material, TEXT("E_Rim_Intensity")), 0.5f, 1e-4f);
	// 原作：累计时间达到「总时长 - 本帧时长」即结束（0.8 >= 1 - 0.3）。
	FTickableGameObject::TickObjects(World, LEVELTICK_All, false, 0.3f);
	TestFalse(TEXT("Play ends after the whole length"), System->IsEffectActive(Handle));
	TestEqual(TEXT("UseRim is restored at the end"), Scalar(Material, TEXT("E_Rim_UseRim")), 0.f);

	World->DestroyWorld(false);
	return true;
}

#endif
