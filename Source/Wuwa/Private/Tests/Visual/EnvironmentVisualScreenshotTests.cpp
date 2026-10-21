#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/FileManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Kismet/KismetMathLibrary.h"
#include "Landscape.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

// 只用于人工看画面：在导入的原作地形关卡里，从几个固定机位截图。名字不含 "Wuwa"，不随常规测试套件运行。
//   -ExecCmds="Automation RunTests EnvironmentVisual.Crownless; Quit"（需要真实 RHI）
namespace EnvironmentVisualScreenshots
{
	UWorld* FindPlayWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) return Context.World();
		}
		return nullptr;
	}

	bool SavePlayViewport(UWorld* World, const FString& Path)
	{
		UGameViewportClient* ViewportClient = World->GetGameViewport();
		FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
		if (!Viewport || Viewport->GetSizeXY().X <= 0) return false;
		TArray<FColor> Pixels;
		if (!Viewport->ReadPixels(Pixels)) return false;
		for (FColor& Pixel : Pixels) Pixel.A = 255;
		const FIntPoint Size = Viewport->GetSizeXY();
		return FImageUtils::SaveImageByExtension(*Path, FImageView(Pixels.GetData(), Size.X, Size.Y));
	}

	struct FShot
	{
		FString Name;
		FVector Location;
		FVector LookAt;
		float Fov = 70.f;
		/** > 0：机位和注视点都贴地，取地面以上这个高度（厘米，向下射线找地面） */
		float GroundHeight = 0.f;
	};

	FVector OnGround(UWorld* World, const FVector& At, float Height)
	{
		FHitResult Hit;
		const FVector Top(At.X, At.Y, 1.0e6), Bottom(At.X, At.Y, -1.0e6);
		return World->LineTraceSingleByChannel(Hit, Top, Bottom, ECC_Visibility) ? Hit.ImpactPoint + FVector(0, 0, Height) : At;
	}

	/** 依次把视角切到每个机位的相机，等着色器编译完、画面稳定后截图。 */
	class FCaptureShots final : public IAutomationLatentCommand
	{
	public:
		/** bInRelativeToPlayerStart：机位（位置和注视点）是相对关卡里 PlayerStart 的偏移 */
		FCaptureShots(FAutomationTestBase* InTest, FString InDirectory, TArray<FShot> InShots, bool bInRelativeToPlayerStart = false)
			: Test(InTest), Directory(MoveTemp(InDirectory)), Shots(MoveTemp(InShots)), bRelativeToPlayerStart(bInRelativeToPlayerStart) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			UWorld* World = FindPlayWorld();
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			if (!Player)
			{
				if (Now - StartedAt > 30.0) { Test->AddError(TEXT("No player controller in PIE.")); return true; }
				return false;
			}
			// 等编队角色生成（项目的加载界面在角色就位后才关闭），否则截到的是加载界面
			if (!Player->GetPawn())
			{
				if (Now - StartedAt > 300.0) { Test->AddError(TEXT("No pawn spawned (loading never finished).")); return true; }
				return false;
			}
			if (PawnAt < 0.0) PawnAt = Now;
			if (Now - PawnAt < 3.0) return false;
			if (Index >= Shots.Num()) return true;
			if (bRelativeToPlayerStart)
			{
				TActorIterator<APlayerStart> Start(World);
				if (!Start) { Test->AddError(TEXT("No PlayerStart in the level.")); return true; }
				for (FShot& Each : Shots)
				{
					Each.Location += Start->GetActorLocation();
					Each.LookAt += Start->GetActorLocation();
				}
				bRelativeToPlayerStart = false;
			}
			const FShot& Shot = Shots[Index];
			if (!Camera.IsValid())
			{
				const FVector From = Shot.GroundHeight > 0.f ? OnGround(World, Shot.Location, Shot.GroundHeight) : Shot.Location;
				const FVector To = Shot.GroundHeight > 0.f ? OnGround(World, Shot.LookAt, Shot.GroundHeight) : Shot.LookAt;
				Camera = World->SpawnActor<ACameraActor>(From, UKismetMathLibrary::FindLookAtRotation(From, To));
				Camera->GetCameraComponent()->SetFieldOfView(Shot.Fov);
				Camera->GetCameraComponent()->bConstrainAspectRatio = false;
				Player->SetViewTarget(Camera.Get());
				SwitchedAt = Now;
				return false;
			}
			// 第一次进图要编译地形和天空的着色器；之后每个机位再等一会儿让流送和自动曝光稳定
			if (GShaderCompilingManager && GShaderCompilingManager->GetNumRemainingJobs() > 0 && Now - SwitchedAt < 600.0) return false;
			if (Now - SwitchedAt < 4.0) return false;
			const FString Path = Directory / (Shot.Name + TEXT(".png"));
			Test->TestTrue(*FString::Printf(TEXT("saved %s"), *Shot.Name), SavePlayViewport(World, Path));
			Test->AddInfo(FString::Printf(TEXT("Saved %s"), *Path));
			Camera->Destroy();
			Camera.Reset();
			++Index;
			return Index >= Shots.Num();
		}

	private:
		FAutomationTestBase* Test;
		FString Directory;
		TArray<FShot> Shots;
		bool bRelativeToPlayerStart = false;
		int32 Index = 0;
		double StartedAt = -1.0;
		double SwitchedAt = 0.0;
		double PawnAt = -1.0;
		TWeakObjectPtr<ACameraActor> Camera;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnvironmentCrownlessScreenshotTest, "EnvironmentVisual.Crownless",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnvironmentCrownlessScreenshotTest::RunTest(const FString& Parameters)
{
	using namespace EnvironmentVisualScreenshots;
	if (GUsingNullRHI)
	{
		AddWarning(TEXT("Needs a real RHI to take screenshots; skipped under NullRHI."));
		return true;
	}
	// 机位按地形配方算：地形中心、Boss 点
	FString Text;
	TSharedPtr<FJsonObject> Recipe;
	const FString RecipePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("ArtSource/Terrain/crownless.terrain.json"));
	if (!FFileHelper::LoadFileToString(Text, *RecipePath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Recipe) || !Recipe.IsValid())
	{
		AddError(FString::Printf(TEXT("Cannot read %s"), *RecipePath));
		return false;
	}
	auto Vec = [&Recipe](const TCHAR* Field)
	{
		const TArray<TSharedPtr<FJsonValue>>& V = Recipe->GetArrayField(Field);
		return FVector(V[0]->AsNumber(), V[1]->AsNumber(), V.Num() > 2 ? V[2]->AsNumber() : 0.0);
	};
	const FVector Location = Vec(TEXT("location"));
	const FVector Scale = Vec(TEXT("scale"));
	const FVector Boss = Vec(TEXT("boss"));
	const double Half = (Recipe->GetIntegerField(TEXT("size")) - 1) * Scale.X * 0.5;
	const FVector Center(Location.X + Half, Location.Y + Half, Boss.Z);

	TArray<FShot> Shots = {
		{TEXT("Crownless_TopDown"), Center + FVector(0, 1, 230000), Center, 60.f},
		{TEXT("Crownless_Aerial_SW"), Boss + FVector(-70000, 70000, 45000), Boss, 70.f},
		{TEXT("Crownless_Aerial_NE"), Boss + FVector(70000, -70000, 45000), Boss, 70.f},
		// 盆地东侧往西看：Boss 所在的盆地底和西边的悬崖
		{TEXT("Crownless_Ground"), Boss + FVector(15000, 3000, 0), Boss + FVector(-10000, 0, 0), 80.f, 250.f},
		{TEXT("Crownless_Ground_Cliff"), Boss + FVector(3000, -12000, 0), Boss + FVector(-12000, 4000, 0), 80.f, 250.f},
	};
	// 物件近景：配方里第一处摆放（摆放最多的网格），从 15 米外贴地看过去
	FString PropsText;
	TSharedPtr<FJsonObject> Props;
	if (FFileHelper::LoadFileToString(PropsText, *FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("ArtSource/Terrain/crownless.props.json")))
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(PropsText), Props) && Props.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>& Placements = Props->GetArrayField(TEXT("placements"));
		if (Placements.Num() > 0)
		{
			const TArray<TSharedPtr<FJsonValue>>& First = Placements[0]->AsArray();
			const FVector At(First[1]->AsNumber(), First[2]->AsNumber(), First[3]->AsNumber());
			Shots.Add({TEXT("Crownless_Props_Near"), At + FVector(1500, 300, 0), At, 70.f, 300.f});
			Shots.Add({TEXT("Crownless_Props_Wide"), At + FVector(6000, 2000, 0), At, 75.f, 600.f});
		}
	}
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Diagnostics/EnvironmentScreenshots"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	if (!AutomationOpenMap(TEXT("/Game/Environment/HuangLong_Crownless/Maps/L_HuangLong_Crownless")))
	{
		AddError(TEXT("Could not open L_HuangLong_Crownless."));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FCaptureShots(this, Directory, MoveTemp(Shots)));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// 教廷（WuwaSceneImport 从网友整理的 jiaoting.blend 导入）：机位围绕 PlayerStart
//   -ExecCmds="Automation RunTests EnvironmentVisual.JiaoTing; Quit"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnvironmentJiaoTingScreenshotTest, "EnvironmentVisual.JiaoTing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnvironmentJiaoTingScreenshotTest::RunTest(const FString& Parameters)
{
	using namespace EnvironmentVisualScreenshots;
	if (GUsingNullRHI)
	{
		AddWarning(TEXT("Needs a real RHI to take screenshots; skipped under NullRHI."));
		return true;
	}
	// 相对 PlayerStart（导入时放在大殿中心的地板上，离地 1 米）
	TArray<FShot> Shots = {
		{TEXT("JiaoTing_TopDown"), FVector(0, 1, 25000), FVector::ZeroVector, 70.f},
		{TEXT("JiaoTing_Aerial_SW"), FVector(-9000, 9000, 6000), FVector::ZeroVector, 70.f},
		{TEXT("JiaoTing_Aerial_NE"), FVector(9000, -9000, 6000), FVector::ZeroVector, 70.f},
		{TEXT("JiaoTing_Inside_N"), FVector(0, 300, 70), FVector(0, -5000, 400), 85.f},
		{TEXT("JiaoTing_Inside_S"), FVector(0, -300, 70), FVector(0, 5000, 400), 85.f},
		{TEXT("JiaoTing_Inside_Up"), FVector(0, 0, 70), FVector(0, -3000, 3000), 85.f},
	};
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Diagnostics/EnvironmentScreenshots"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	if (!AutomationOpenMap(TEXT("/Game/Environment/JiaoTing/Maps/L_JiaoTing")))
	{
		AddError(TEXT("Could not open L_JiaoTing."));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FCaptureShots(this, Directory, MoveTemp(Shots), true));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// 大卡战斗场地（宫殿大厅里；WuwaSceneImport + WuwaArenaSetup，ArtSource/Environment/daka.arena.json）：机位相对 PlayerStart，
// PlayerStart 在 Boss 正前方 12 米、面朝 +X（Boss 在 +X 方向，离地 1 米）
//   -ExecCmds="Automation RunTests EnvironmentVisual.DaKa; Quit"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnvironmentDaKaScreenshotTest, "EnvironmentVisual.DaKa",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnvironmentDaKaScreenshotTest::RunTest(const FString& Parameters)
{
	using namespace EnvironmentVisualScreenshots;
	if (GUsingNullRHI)
	{
		AddWarning(TEXT("Needs a real RHI to take screenshots; skipped under NullRHI."));
		return true;
	}
	const FVector Boss(1200, 0, -100);
	TArray<FShot> Shots = {
		{TEXT("DaKa_PlayerView"), FVector(-500, 0, 250), Boss + FVector(0, 0, 250), 80.f},
		{TEXT("DaKa_Boss_Close"), Boss + FVector(-450, -120, 230), Boss + FVector(0, 0, 150), 50.f},
		{TEXT("DaKa_Hall_High"), Boss + FVector(-3500, -3500, 2500), Boss, 75.f},
		{TEXT("DaKa_Hall_Up"), Boss + FVector(-2000, 0, 200), Boss + FVector(0, 0, 6000), 85.f},
		{TEXT("DaKa_Hall_Side"), Boss + FVector(0, -3000, 300), Boss + FVector(0, 3000, 300), 85.f},
	};
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Diagnostics/EnvironmentScreenshots"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	if (!AutomationOpenMap(TEXT("/Game/Environment/DaKa/Maps/L_DaKa")))
	{
		AddError(TEXT("Could not open L_DaKa."));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FCaptureShots(this, Directory, MoveTemp(Shots), true));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
