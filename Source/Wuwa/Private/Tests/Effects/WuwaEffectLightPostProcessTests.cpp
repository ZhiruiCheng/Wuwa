#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/World.h"
#include "Tickable.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Game/Effect/System/WuwaEffectSystem.h"
#include "Game/EffectModel/Models/WuwaEffectModelLight.h"
#include "Game/EffectModel/Models/WuwaEffectModelPostProcess.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"

namespace WuwaEffectLightPostProcessTests
{
	UWorld* CreateTestWorld()
	{
		UWorld::InitializationValues Values;
		Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
		return UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	}

	template <typename T>
	T* FindLiveComponent(const UWorld* World)
	{
		for (TObjectIterator<T> It; It; ++It)
		{
			if (It->GetWorld() == World && It->IsRegistered() && IsValid(*It)) return *It;
		}
		return nullptr;
	}

	/** 两个关键帧的线性曲线（测试只关心端点值）。 */
	void SetLinear(FWuwaEffectFloatCurve& Curve, float Start, float End, float Duration)
	{
		Curve.bUseCurve = true;
		FRichCurve& Rich = Curve.Curve.EditorCurveData;
		Rich.Reset();
		Rich.SetKeyInterpMode(Rich.AddKey(0.f, Start), RCIM_Linear);
		Rich.SetKeyInterpMode(Rich.AddKey(Duration, End), RCIM_Linear);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaEffectLightSpecTest,
	"Wuwa.Effect.Light.PlaysCurvesAndEnds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaEffectLightSpecTest::RunTest(const FString& Parameters)
{
	using namespace WuwaEffectLightPostProcessTests;
	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("Test world exists"), World)) return false;
	UWuwaEffectSystem* System = World->GetSubsystem<UWuwaEffectSystem>();

	TStrongObjectPtr<UWuwaEffectModelLight> Model(NewObject<UWuwaEffectModelLight>());
	Model->StartTime = 0.4f;
	SetLinear(Model->Intensity, 0.f, 10.f, 0.4f);
	Model->LightColor = FWuwaEffectColorCurve(FLinearColor(0.4f, 0.29f, 1.f));
	Model->Location = FWuwaEffectVectorCurve(FVector(0, 20, -15));
	FWuwaEffectSpawnRequest Request;
	Request.Model = Model.Get();
	Request.Transform = FTransform(FVector(100, 0, 0));

	const auto Handle = System->SpawnEffect(Request);
	TestTrue(TEXT("Light play starts"), System->IsEffectActive(Handle));
	UPointLightComponent* Light = FindLiveComponent<UPointLightComponent>(World);
	if (TestNotNull(TEXT("A point light is spawned"), Light))
	{
		TestEqual(TEXT("Intensity starts at the curve's first key"), Light->Intensity, 0.f);
		TestEqual(TEXT("Units come from the model"), Light->IntensityUnits, Model->IntensityUnits);
		// 点光颜色存成 8 位 sRGB FColor，往返有量化误差。
		TestTrue(TEXT("Colour comes from the model"), Light->GetLightColor().Equals(FLinearColor(0.4f, 0.29f, 1.f), 0.01f));
		TestTrue(TEXT("Model location is added to the request transform"),
			Light->GetComponentLocation().Equals(FVector(100, 20, -15), 1e-3));
		FTickableGameObject::TickObjects(World, LEVELTICK_All, false, 0.2f);
		TestEqual(TEXT("Intensity follows the curve"), Light->Intensity, 5.f, 1e-3f);
	}
	FTickableGameObject::TickObjects(World, LEVELTICK_All, false, 0.25f);
	TestFalse(TEXT("Play ends after StartTime"), System->IsEffectActive(Handle));
	TestNull(TEXT("Light component is destroyed"), FindLiveComponent<UPointLightComponent>(World));
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaEffectPostProcessSpecTest,
	"Wuwa.Effect.PostProcess.PlaysCurvesAndEnds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaEffectPostProcessSpecTest::RunTest(const FString& Parameters)
{
	using namespace WuwaEffectLightPostProcessTests;
	UWorld* World = CreateTestWorld();
	if (!TestNotNull(TEXT("Test world exists"), World)) return false;
	UWuwaEffectSystem* System = World->GetSubsystem<UWuwaEffectSystem>();

	// 原作 DA_Fx_Post_Kaiqiang_01 的形状：三条曲线 0.233s 内归零。
	TStrongObjectPtr<UWuwaEffectModelPostProcess> Model(NewObject<UWuwaEffectModelPostProcess>());
	Model->StartTime = 0.233f;
	Model->BlurIntensityOverride = Model->SceneFringeIntensityOverride = Model->VignetteIntensityOverride = true;
	SetLinear(Model->BlurIntensity, 0.25f, 0.f, 0.233f);
	SetLinear(Model->SceneFringeIntensity, 5.f, 0.f, 0.233f);
	SetLinear(Model->VignetteIntensity, 0.25f, 0.f, 0.233f);
	Model->RadialBlurMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	FWuwaEffectSpawnRequest Request;
	Request.Model = Model.Get();

	const auto Handle = System->SpawnEffect(Request);
	TestTrue(TEXT("Post process play starts"), System->IsEffectActive(Handle));
	UPostProcessComponent* Post = FindLiveComponent<UPostProcessComponent>(World);
	if (TestNotNull(TEXT("A post process component is spawned"), Post))
	{
		TestTrue(TEXT("Unbound without a volume"), Post->bUnbound);
		TestTrue(TEXT("Enabled with blend weight = volume hardness"), Post->bEnabled && FMath::IsNearlyEqual(Post->BlendWeight, 1.f));
		TestTrue(TEXT("Scene fringe override is on"), Post->Settings.bOverride_SceneFringeIntensity);
		TestEqual(TEXT("Scene fringe starts at the first key"), Post->Settings.SceneFringeIntensity, 5.f, 1e-3f);
		TestEqual(TEXT("Vignette starts at the first key"), Post->Settings.VignetteIntensity, 0.25f, 1e-3f);
		const UMaterialInstanceDynamic* Blur = Post->Settings.WeightedBlendables.Array.Num() == 1
			? Cast<UMaterialInstanceDynamic>(Post->Settings.WeightedBlendables.Array[0].Object) : nullptr;
		if (TestNotNull(TEXT("Radial blur material is blended in"), Blur))
		{
			// 默认材质没有这个参数：只检查 Spec 确实写入了 MID 的覆盖值。
			TestTrue(TEXT("Blur intensity is written to the material"), Blur->ScalarParameterValues.ContainsByPredicate(
				[](const FScalarParameterValue& V) { return V.ParameterInfo.Name == TEXT("Intensity") && FMath::IsNearlyEqual(V.ParameterValue, 0.25f); }));
		}
		FTickableGameObject::TickObjects(World, LEVELTICK_All, false, 0.1165f);
		TestEqual(TEXT("Scene fringe follows the curve"), Post->Settings.SceneFringeIntensity, 2.5f, 1e-2f);
	}
	FTickableGameObject::TickObjects(World, LEVELTICK_All, false, 0.15f);
	TestFalse(TEXT("Play ends after StartTime"), System->IsEffectActive(Handle));
	TestNull(TEXT("Post process component is destroyed"), FindLiveComponent<UPostProcessComponent>(World));
	World->DestroyWorld(false);
	return true;
}

#endif
