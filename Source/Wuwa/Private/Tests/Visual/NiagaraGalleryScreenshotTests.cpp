#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "NiagaraSystem.h"
#include "RHI.h"
#include "UnrealClient.h"
#include "ShaderCompiler.h"
#include "AssetCompilingManager.h"

// 只用于人工看画面：不放角色，把若干 Niagara 系统并排摆在固定镜头前播放并按时间截图，用来单独检查重建的特效是否可见。
// 不含 "Wuwa"，不随常规测试套件运行。
// -GallerySystems=/Game/.../NS_A.NS_A,/Game/.../NS_B.NS_B  -GalleryShots=0.05,0.15,0.3  -GalleryTag=Attack01  -GalleryScale=1  -GalleryPitch=-32  -GalleryDistance=830
namespace NiagaraGallery
{
	UWorld* FindPlayWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) return Context.World();
		}
		return nullptr;
	}

	bool SaveViewport(UWorld* World, const FString& Path)
	{
		UGameViewportClient* Client = World->GetGameViewport();
		FViewport* Viewport = Client ? Client->Viewport : nullptr;
		if (!Viewport || Viewport->GetSizeXY().X <= 0) return false;
		TArray<FColor> Pixels;
		if (!Viewport->ReadPixels(Pixels)) return false;
		for (FColor& Pixel : Pixels) Pixel.A = 255;
		return FImageUtils::SaveImageByExtension(*Path, FImageView(Pixels.GetData(), Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y));
	}

	class FCaptureGallery final : public IAutomationLatentCommand
	{
	public:
		FCaptureGallery(FAutomationTestBase* InTest, TArray<FString> InSystems, TArray<float> InShots, FString InTag, float InScale, float InCameraPitch, float InCameraDistance, FString InDirectory)
			: Test(InTest), Systems(MoveTemp(InSystems)), Shots(MoveTemp(InShots)), Tag(MoveTemp(InTag)), Scale(InScale)
			, CameraPitch(InCameraPitch), CameraDistance(InCameraDistance), Directory(MoveTemp(InDirectory)) {}

		virtual bool Update() override
		{
			UWorld* World = FindPlayWorld();
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			// 等编队角色生成（加载界面此时还盖在最上面，会在角色就绪后淡出）
			if (!Player || !Player->GetPawn())
			{
				return FPlatformTime::Seconds() - StartedAt > 120.0 ? (Test->AddError(TEXT("No PIE player pawn.")), true) : false;
			}
			const double Now = World->GetTimeSeconds();
			if (!Camera)
			{
				// 固定镜头：沿 +X 方向、按俯角和距离斜向下看原点附近一排系统，远离地图里的角色和建筑
				const FVector Origin(0.f, 3000.f, 50.f);
				const FRotator View(CameraPitch, 0.f, 0.f);
				Camera = World->SpawnActor<ACameraActor>(Origin - View.Vector() * CameraDistance, View);
				Camera->GetCameraComponent()->SetFieldOfView(70.f);
				Player->SetViewTargetWithBlend(Camera, 0.f);
				if (APawn* Pawn = Player->GetPawn()) Pawn->SetActorHiddenInGame(true);
				Base = Origin;
				if (bDarkFloor)
				{
					// 无光照纯黑地面，对照夜景视频（叠加 / 半透明特效在浅色地板上会发白）
					AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(Origin - FVector(0.f, 0.f, 49.f), FRotator::ZeroRotator);
					UStaticMeshComponent* Plane = Floor->GetStaticMeshComponent();
					Plane->SetMobility(EComponentMobility::Movable);
					Plane->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
					Plane->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/BlackUnlitMaterial.BlackUnlitMaterial")));
					Floor->SetActorScale3D(FVector(200.f, 200.f, 1.f));
					Player->ConsoleCommand(TEXT("ShowFlag.EyeAdaptation 0"));  // 见 EffectVisualScreenshotTests
				}
				SetupAt = Now;
				return false;
			}
			if (Phase == 0)
			{
				if (Now - SetupAt < 4.0) return false; // 等加载界面关闭、镜头与曝光稳定
				// 第一次只播放不截图：第一次加载时 shader / 资源可能还没准备好
				Spawn(World);
				PlayedAt = Now;
				Phase = 1;
				return false;
			}
			if (Phase == 1)
			{
				if (Now - PlayedAt < 3.0) return false;
				// 第一次播放触发的按需 shader（Niagara 网格顶点工厂排列等）必须编完，否则截到的是默认材质、没有 WPO 的原尺寸网格
				FAssetCompilingManager::Get().FinishAllCompilation();
				if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
				Spawn(World);
				PlayedAt = Now;
				Phase = 2;
				ShotIndex = 0;
				return false;
			}
			if (ShotIndex >= Shots.Num()) return true;
			if (Now - PlayedAt < Shots[ShotIndex]) return false;
			const FString Path = Directory / FString::Printf(TEXT("%s_%04dms.png"), *Tag, FMath::RoundToInt(Shots[ShotIndex] * 1000.f));
			IFileManager::Get().Delete(*Path);
			if (!SaveViewport(World, Path)) { Test->AddError(TEXT("Screenshot failed: ") + Path); return true; }
			Test->AddInfo(TEXT("Saved ") + Path);
			++ShotIndex;
			return false;
		}

	private:
		void Spawn(UWorld* World)
		{
			// 默认一排沿 Y 方向等距摆放；-GalleryColumns=N 时排成网格（行沿 +X 往远处，即俯视画面里从下往上），按顺序左→右、下→上
			const int32 Cols = Columns > 0 ? Columns : Systems.Num();
			const int32 Rows = FMath::DivideAndRoundUp(Systems.Num(), Cols);
			for (int32 Index = 0; Index < Systems.Num(); ++Index)
			{
				UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *Systems[Index]);
				if (!System) { Test->AddError(TEXT("Missing ") + Systems[Index]); continue; }
				const float Col = Index % Cols - 0.5f * (Cols - 1), Row = Index / Cols - 0.5f * (Rows - 1);
				const FVector Location = Base + FVector(Row * Spacing, Col * Spacing, 0.f);
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, Location, FRotator::ZeroRotator, FVector(Scale), true, true);
			}
		}

		FAutomationTestBase* Test;
		TArray<FString> Systems;
		TArray<float> Shots;
		FString Tag;
		float Scale = 1.f;
		float CameraPitch = -32.f;
		float CameraDistance = 830.f;
	public:
		/** -GalleryDarkFloor：在系统下方铺无光照纯黑地面。 */
		bool bDarkFloor = false;
		/** -GalleryColumns=N 排成 N 列的网格；-GallerySpacing=厘米 间距（默认 400）。 */
		int32 Columns = 0;
		float Spacing = 400.f;
	private:
		FString Directory;
		ACameraActor* Camera = nullptr;
		FVector Base = FVector::ZeroVector;
		double StartedAt = FPlatformTime::Seconds();
		double SetupAt = 0.0;
		double PlayedAt = 0.0;
		int32 Phase = 0;
		int32 ShotIndex = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEffectVisualNiagaraGalleryTest, "EffectVisual.NiagaraGallery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEffectVisualNiagaraGalleryTest::RunTest(const FString& Parameters)
{
	if (GUsingNullRHI)
	{
		AddWarning(TEXT("Needs a real RHI to take screenshots; skipped under NullRHI."));
		return true;
	}
	FString SystemsText, ShotsText = TEXT("0.05,0.15,0.3,0.6"), Tag = TEXT("Gallery");
	float Scale = 1.f, CameraPitch = -32.f, CameraDistance = 830.f;
	FParse::Value(FCommandLine::Get(), TEXT("GallerySystems="), SystemsText, false);
	FParse::Value(FCommandLine::Get(), TEXT("GalleryShots="), ShotsText, false);
	FParse::Value(FCommandLine::Get(), TEXT("GalleryTag="), Tag);
	FParse::Value(FCommandLine::Get(), TEXT("GalleryScale="), Scale);
	FParse::Value(FCommandLine::Get(), TEXT("GalleryPitch="), CameraPitch);
	FParse::Value(FCommandLine::Get(), TEXT("GalleryDistance="), CameraDistance);
	TArray<FString> Systems, ShotParts;
	SystemsText.ParseIntoArray(Systems, TEXT(","));
	ShotsText.ParseIntoArray(ShotParts, TEXT(","));
	TArray<float> Shots;
	for (const FString& Part : ShotParts) Shots.Add(FCString::Atof(*Part));
	if (Systems.IsEmpty())
	{
		AddError(TEXT("Pass -GallerySystems=<object path>,..."));
		return false;
	}
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Diagnostics/EffectScreenshots"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	if (!AutomationOpenMap(TEXT("/Game/Map/Wuwa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	auto* Capture = new NiagaraGallery::FCaptureGallery(this, Systems, Shots, Tag, Scale, CameraPitch, CameraDistance, Directory);
	Capture->bDarkFloor = FParse::Param(FCommandLine::Get(), TEXT("GalleryDarkFloor"));
	FParse::Value(FCommandLine::Get(), TEXT("GalleryColumns="), Capture->Columns);
	FParse::Value(FCommandLine::Get(), TEXT("GallerySpacing="), Capture->Spacing);
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShareable(Capture));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
