#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Animation/AnimSequence.h"
#include "Animation/SkeletalMeshActor.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/FileManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Kismet/KismetMathLibrary.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

// 只用于人工看画面：在教廷大殿里放一个导入的芙露德莉丝（Boss MB1FuludelisiMd00401），挂上大剑和三把召唤剑，
// 摆几个动作的固定帧截图。名字不含 "Wuwa"，不随常规测试套件运行。
//   -ExecCmds="Automation RunTests CharacterVisual.Fuludelisi; Quit"（需要真实 RHI）
namespace CharacterPreviewScreenshots
{
	const TCHAR* Root = TEXT("/Game/Characters/Monster/Boss/Fuludelisi");

	struct FPose
	{
		FString Name;
		FString Anim;
		float Time = 0.f;
		/** 相机相对角色（角色面朝 -X，即朝向 PlayerStart）的偏移和注视点高度 */
		FVector CameraOffset = FVector(-450, 0, 60);
		float LookAtHeight = 0.f;
	};

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

	template <typename T>
	T* Load(const FString& Path) { return LoadObject<T>(nullptr, *Path); }

	class FCapturePoses final : public IAutomationLatentCommand
	{
	public:
		FCapturePoses(FAutomationTestBase* InTest, FString InDirectory, TArray<FPose> InPoses)
			: Test(InTest), Directory(MoveTemp(InDirectory)), Poses(MoveTemp(InPoses)) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			UWorld* World = FindPlayWorld();
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			// 等编队角色生成（加载界面在角色就位后才关闭）
			if (!Player || !Player->GetPawn())
			{
				if (Now - StartedAt > 300.0) { Test->AddError(TEXT("No pawn spawned (loading never finished).")); return true; }
				return false;
			}
			if (!Boss.IsValid() && !SpawnBoss(World)) return true;
			if (Index >= Poses.Num()) return true;
			const FPose& Pose = Poses[Index];
			if (!Camera.IsValid())
			{
				UAnimSequence* Anim = Load<UAnimSequence>(FString(Root) / TEXT("CommonAnim") / (TEXT("MB1FuludelisiMd00401_") + Pose.Anim));
				if (!Anim) { Test->AddError(FString::Printf(TEXT("Missing anim %s"), *Pose.Anim)); return true; }
				USkeletalMeshComponent* Body = Boss->GetSkeletalMeshComponent();
				Body->PlayAnimation(Anim, false);
				Body->SetPosition(Pose.Time, false);
				Body->SetPlayRate(0.f);
				const FVector Base = Boss->GetActorLocation();
				const FVector From = Base + Pose.CameraOffset;
				const FVector To = Base + FVector(0, 0, Pose.LookAtHeight);
				Camera = World->SpawnActor<ACameraActor>(From, UKismetMathLibrary::FindLookAtRotation(From, To));
				Camera->GetCameraComponent()->SetFieldOfView(60.f);
				Camera->GetCameraComponent()->bConstrainAspectRatio = false;
				Player->SetViewTarget(Camera.Get());
				SwitchedAt = Now;
				return false;
			}
			if (GShaderCompilingManager && GShaderCompilingManager->GetNumRemainingJobs() > 0 && Now - SwitchedAt < 600.0) return false;
			if (Now - SwitchedAt < 3.0) return false;
			const FString Path = Directory / (TEXT("Fuludelisi_") + Pose.Name + TEXT(".png"));
			Test->TestTrue(*FString::Printf(TEXT("saved %s"), *Pose.Name), SavePlayViewport(World, Path));
			Test->AddInfo(FString::Printf(TEXT("Saved %s"), *Path));
			Camera->Destroy();
			Camera.Reset();
			++Index;
			return Index >= Poses.Num();
		}

	private:
		/** PlayerStart 前方 6 米、面朝 PlayerStart，直接放在地面上（原作 BP 里网格相对胶囊中心下移 110）。 */
		bool SpawnBoss(UWorld* World)
		{
			TActorIterator<APlayerStart> Start(World);
			USkeletalMesh* Mesh = Load<USkeletalMesh>(FString(Root) / TEXT("Mesh/MB1FuludelisiMd00401"));
			if (!Start || !Mesh) { Test->AddError(TEXT("Missing PlayerStart or the Fuludelisi mesh.")); return false; }
			const FVector At = Start->GetActorLocation() + FVector(600, 0, -100);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ASkeletalMeshActor* Actor = World->SpawnActor<ASkeletalMeshActor>(At, FRotator(0, 180, 0), Params);
			USkeletalMeshComponent* Body = Actor->GetSkeletalMeshComponent();
			Body->SetSkeletalMesh(Mesh);
			// 网格就是 ASkeletalMeshActor 的根组件，这里设的是世界朝向：角色 BP 里网格相对胶囊偏航 -90（和原作一样）
			// 时面朝 Actor 前方，所以面朝 -X（PlayerStart）要 180 - 90 = 90
			Body->SetWorldRotation(FRotator(0, 90, 0));
			Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			// 原作 BP：大剑挂 WeaponProp02，召唤剑挂 WeaponProp05/06/07（都是跟着动画走的骨骼）
			const TPair<const TCHAR*, const TCHAR*> Weapons[] = {
				{TEXT("EFuludelisiDajianMd10011"), TEXT("WeaponProp02")},
				{TEXT("MB1FuludelisiMd00401_Gongmingzhejian"), TEXT("WeaponProp05")},
				{TEXT("MB1FuludelisiMd00401_Suizhujian"), TEXT("WeaponProp06")},
				{TEXT("MB1FuludelisiMd00401_Mingshijian"), TEXT("WeaponProp07")},
			};
			for (const auto& Weapon : Weapons)
			{
				USkeletalMesh* WeaponMesh = Load<USkeletalMesh>(FString(Root) / TEXT("Weapon") / Weapon.Key);
				if (!WeaponMesh) { Test->AddWarning(FString::Printf(TEXT("Missing weapon %s"), Weapon.Key)); continue; }
				USkeletalMeshComponent* Component = NewObject<USkeletalMeshComponent>(Actor);
				Component->SetSkeletalMesh(WeaponMesh);
				Component->SetupAttachment(Body, Weapon.Value);
				Component->RegisterComponent();
			}
			Boss = Actor;
			return true;
		}

		FAutomationTestBase* Test;
		FString Directory;
		TArray<FPose> Poses;
		int32 Index = 0;
		double StartedAt = -1.0;
		double SwitchedAt = 0.0;
		TWeakObjectPtr<ASkeletalMeshActor> Boss;
		TWeakObjectPtr<ACameraActor> Camera;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterFuludelisiScreenshotTest, "CharacterVisual.Fuludelisi",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCharacterFuludelisiScreenshotTest::RunTest(const FString& Parameters)
{
	using namespace CharacterPreviewScreenshots;
	if (GUsingNullRHI)
	{
		AddWarning(TEXT("Needs a real RHI to take screenshots; skipped under NullRHI."));
		return true;
	}
	TArray<FPose> Poses = {
		{TEXT("Stand_Front"), TEXT("Stand2"), 1.0f, FVector(-380, 0, 100), 100.f},
		{TEXT("Stand_Face"), TEXT("Stand2"), 1.0f, FVector(-120, 0, 160), 155.f},
		{TEXT("Stand_Side"), TEXT("Stand2"), 1.0f, FVector(0, -380, 100), 100.f},
		{TEXT("Stand_Back"), TEXT("Stand2"), 1.0f, FVector(380, 0, 100), 100.f},
		{TEXT("Attack01_Slash"), TEXT("Attack01"), 2.3f, FVector(-500, -250, 150), 100.f},
		{TEXT("Attack03_Thrust"), TEXT("Attack03"), 2.3f, FVector(-500, -250, 150), 100.f},
		{TEXT("Attack02"), TEXT("Attack02"), 2.6f, FVector(-600, -300, 200), 100.f},
		{TEXT("Death"), TEXT("Death"), 4.5f, FVector(-500, -250, 200), 50.f},
	};
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Diagnostics/CharacterScreenshots"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	if (!AutomationOpenMap(TEXT("/Game/Environment/JiaoTing/Maps/L_JiaoTing")))
	{
		AddError(TEXT("Could not open L_JiaoTing."));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FCapturePoses(this, Directory, MoveTemp(Poses)));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
