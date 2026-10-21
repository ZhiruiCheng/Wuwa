#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Animation/AnimMontage.h"
#include "Engine/Engine.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RHI.h"
#include "UnrealClient.h"
#include "ShaderCompiler.h"
#include "AssetCompilingManager.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_PlayEffect.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/Formation/WuwaFormationComponent.h"

// 只用于人工看画面：在真实地图里播放攻击蒙太奇并截图，名字不含 "Wuwa"，不随常规测试套件运行。
namespace EffectVisualScreenshots
{
	struct FShot
	{
		FString View;
		float SecondsAfterEffect = 0.f;
		/** 只播放不截图：第一次播放时特效资源可能还没加载完，画面里看不到特效。 */
		bool bWarmup = false;
	};

	UWorld* FindPlayWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) return Context.World();
		}
		return nullptr;
	}

	/** 直接读 PIE 视口的像素。FScreenshotRequest 会被编辑器视口抢走，截到的不是游戏画面。 */
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

	class FCaptureAttackShots final : public IAutomationLatentCommand
	{
	public:
		FCaptureAttackShots(FAutomationTestBase* InTest, FString InAttack, TArray<FShot> InShots, FString InDirectory, float InPitch)
			: Test(InTest), Attack(MoveTemp(InAttack)), Shots(MoveTemp(InShots)), Directory(MoveTemp(InDirectory)), Pitch(InPitch) {}

		virtual bool Update() override
		{
			UWorld* World = FindPlayWorld();
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			AWuwaCharacter* Character = Player ? Cast<AWuwaCharacter>(Player->GetPawn()) : nullptr;
			const double Now = FPlatformTime::Seconds();
			if (!Character)
			{
				// 刚重建过特效时首次进 PIE 要编译着色器，编队异步加载常超过 20 秒
				if (Now - StartedAt > 90.0) { Test->AddError(TEXT("No possessed Wuwa character in PIE.")); return true; }
				return false;
			}
			// -EffectRole=jiabei：先切到类名含这个词的编队成员，再播它的蒙太奇
			if (!RoleName.IsEmpty() && !Character->GetClass()->GetName().Contains(RoleName))
			{
				if (Now - StartedAt > 120.0) { Test->AddError(FString::Printf(TEXT("Could not switch to role %s."), *RoleName)); return true; }
				if (UWuwaFormationComponent* Formation = Player->FindComponentByClass<UWuwaFormationComponent>())
				{
					const TArray<TObjectPtr<AWuwaCharacter>>& Members = Formation->GetMembers();
					for (int32 Index = 0; Index < Members.Num(); ++Index)
					{
						if (Members[Index] && Members[Index]->GetClass()->GetName().Contains(RoleName) && Formation->CanSwitchTo(Index))
						{
							Formation->RequestSwitch(Index);
							break;
						}
					}
				}
				return false;
			}
			if (!Montage)
			{
				const FString MontagePath = !MontagePathOverride.IsEmpty() ? MontagePathOverride
					: FString::Printf(TEXT("/Game/Characters/Role/changli/AnimMontage/AM_%s.AM_%s"), *Attack, *Attack);
				Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
				if (!Montage) { Test->AddError(FString::Printf(TEXT("AM_%s missing."), *Attack)); return true; }
				EffectTime = Montage->GetPlayLength();
				for (const FAnimNotifyEvent& Event : Montage->Notifies)
				{
					if (Cast<UWuwaAnimNotify_PlayEffect>(Event.Notify.Get())) EffectTime = FMath::Min(EffectTime, Event.GetTime());
				}
				BaseYaw = Character->GetActorRotation().Yaw;
				BaseLocation = Character->GetActorLocation();
				if (bDarkFloor)
				{
					SpawnDarkFloor(World, Character);
					// 画面几乎全黑时自动曝光会大幅提亮，暗色半透明层被放大成鲜艳色块；对照时关掉曝光自适应
					Player->ConsoleCommand(TEXT("ShowFlag.EyeAdaptation 0"));
				}
			}
			if (ShotIndex >= Shots.Num()) return true;
			const FShot& Shot = Shots[ShotIndex];
			const double GameTime = World->GetTimeSeconds();

			switch (Stage)
			{
			case EStage::Aim:
			{
				// 上一次播放触发的按需 shader 编完再截图，否则网格粒子会用默认材质（没有 WPO 缩放）原尺寸显示
				FAssetCompilingManager::Get().FinishAllCompilation();
				if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
				// 每张都从同一位置起播（镜头也按这个位置摆）：带根运动的段（如第四段后退）连续重播会把角色推进墙里
				Character->SetActorLocationAndRotation(BaseLocation, FRotator(0.f, BaseYaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
				// 背后：镜头朝向与角色一致；正面：反过来看角色；Left / Right：从角色左 / 右侧看侧面。俯角与参考截图的视角相近。
				const float ViewOffset = Shot.View == TEXT("Front") ? 180.f : Shot.View == TEXT("Left") ? 90.f : Shot.View == TEXT("Right") ? -90.f : 0.f;
				// -EffectRotateCharacter：转角色、镜头始终在正面（太阳不动，用来看不同光照角度下的脸）
				const float CharacterYaw = bRotateCharacter ? BaseYaw - ViewOffset + 180.f : BaseYaw;
				const float Yaw = bRotateCharacter ? CharacterYaw + 180.f : BaseYaw + ViewOffset;
				Player->SetControlRotation(FRotator(Pitch, Yaw, 0.f));
				if (CameraDistance > 0.f)
				{
					// 固定机位：沿视线方向退到给定距离，便于和远景的参考视频同框对照
					const FRotator View(Pitch, Yaw, 0.f);
					const FVector Location = Character->GetActorLocation() + FVector(0.f, 0.f, CameraHeight) - View.Vector() * CameraDistance;
					if (!ShotCamera) ShotCamera = World->SpawnActor<ACameraActor>(Location, View);
					ShotCamera->SetActorLocationAndRotation(Location, View);
					Player->SetViewTargetWithBlend(ShotCamera, 0.f);
				}
				Character->SetActorRotation(FRotator(0.f, CharacterYaw, 0.f));
				StageStartedGameTime = GameTime;
				Stage = EStage::Settle;
				return false;
			}
			case EStage::Settle:
				// 第一张多等一会儿：PIE 刚开始时镜头还没稳定到角色身上。
				if (GameTime - StageStartedGameTime < (ShotIndex == 0 ? 2.0 : 0.6)) return false;
				Character->StopAnimMontage();
				// -EffectNoPlay：不播蒙太奇，只看站立的角色（核对角色材质用）
				if (!bNoPlay) Character->PlayAnimMontage(Montage);
				PlayStartLocation = Character->GetActorLocation();
				StageStartedGameTime = GameTime;
				Stage = EStage::WaitForShot;
				return false;
			case EStage::WaitForShot:
				if (GameTime - StageStartedGameTime < EffectTime + Shot.SecondsAfterEffect) return false;
				if (!Shot.bWarmup)
				{
					const FString ShotPath = Directory / FString::Printf(TEXT("%s_%s_%04dms.png"), *Attack, *Shot.View, FMath::RoundToInt(Shot.SecondsAfterEffect * 1000.f));
					IFileManager::Get().Delete(*ShotPath);
					if (!SavePlayViewport(World, ShotPath)) { Test->AddError(FString::Printf(TEXT("Screenshot was not written: %s"), *ShotPath)); return true; }
					Test->AddInfo(FString::Printf(TEXT("Saved %s"), *ShotPath));
					// 根运动位移（水平），用来核对滑铲 / 冲刺距离
					Test->AddInfo(FString::Printf(TEXT("Displacement %s %s %.0fms: %.1f cm"), *Attack, *Shot.View,
						Shot.SecondsAfterEffect * 1000.f, FVector::Dist2D(Character->GetActorLocation(), PlayStartLocation)));
				}
				Character->StopAnimMontage();
				++ShotIndex;
				Stage = EStage::Aim;
				return false;
			}
			return true;
		}

	private:
		enum class EStage : uint8 { Aim, Settle, WaitForShot };

		/** 参考视频是夜景深色地面；半透明特效叠在测试图的浅色地板上会发白，对照颜色时在脚下铺一块深色平面。 */
		static void SpawnDarkFloor(UWorld* World, AWuwaCharacter* Character)
		{
			UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
			UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/BlackUnlitMaterial.BlackUnlitMaterial"));
			if (!Plane || !Material) return;
			const FVector Feet = Character->GetActorLocation() - FVector(0.f, 0.f, Character->GetSimpleCollisionHalfHeight() - 2.f);
			AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(Feet, FRotator::ZeroRotator);
			if (!Floor) return;
			UStaticMeshComponent* Mesh = Floor->GetStaticMeshComponent();
			Mesh->SetMobility(EComponentMobility::Movable);
			Mesh->SetStaticMesh(Plane);
			Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mesh->SetMaterial(0, Material);  // 无光照纯黑：日光下的受光暗色材质仍是灰色
			Floor->SetActorScale3D(FVector(200.f, 200.f, 1.f));
		}

	public:
		/** -EffectRole：切到类名含这个词的编队成员；-EffectMontagePath：直接指定蒙太奇对象路径。 */
		FString RoleName;
		FString MontagePathOverride;
		/** -EffectDarkFloor：脚下铺深色地面（见 SpawnDarkFloor）。 */
		bool bDarkFloor = false;
		bool bNoPlay = false;
		bool bRotateCharacter = false;
		/** -EffectCameraHeight：固定机位对准角色中心以上多少厘米（默认 50）。 */
		float CameraHeight = 50.f;

		/** -EffectCameraDistance=700：用固定相机（>0 时），不跟随游戏镜头。 */
		float CameraDistance = 0.f;
		ACameraActor* ShotCamera = nullptr;

	private:
		FAutomationTestBase* Test;
		FString Attack;
		TArray<FShot> Shots;
		FString Directory;
		float Pitch = -28.f;
		UAnimMontage* Montage = nullptr;
		float EffectTime = 0.f;
		float BaseYaw = 0.f;
		FVector BaseLocation = FVector::ZeroVector;
		FVector PlayStartLocation = FVector::ZeroVector;
		int32 ShotIndex = 0;
		EStage Stage = EStage::Aim;
		double StartedAt = FPlatformTime::Seconds();
		double StageStartedGameTime = 0.0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEffectVisualAttackScreenshotTest, "EffectVisual.ChangliAttackScreenshots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEffectVisualAttackScreenshotTest::RunTest(const FString& Parameters)
{
	if (GUsingNullRHI)
	{
		AddWarning(TEXT("Needs a real RHI to take screenshots; skipped under NullRHI."));
		return true;
	}
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Diagnostics/EffectScreenshots"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	// 默认第一段、默认时间点；-EffectMontage=Attack02 换攻击段，-EffectShots=0.2,0.3 改时间点（秒，相对第一个特效通知），-EffectViews=Back 只截一个方向。
	FString Attack = TEXT("Attack01");
	FParse::Value(FCommandLine::Get(), TEXT("EffectMontage="), Attack);
	// 俯角默认与参考截图相近；-EffectPitch=-70 可从高处俯看整体布局。
	float Pitch = -28.f;
	FParse::Value(FCommandLine::Get(), TEXT("EffectPitch="), Pitch);
	TArray<float> Offsets = {0.06f, 0.15f, 0.3f, 0.5f};
	FString OffsetText;
	if (FParse::Value(FCommandLine::Get(), TEXT("EffectShots="), OffsetText, /*bShouldStopOnSeparator*/ false))
	{
		TArray<FString> Parts;
		OffsetText.ParseIntoArray(Parts, TEXT(","));
		Offsets.Reset();
		for (const FString& Part : Parts) Offsets.Add(FCString::Atof(*Part));
	}
	TArray<FString> Views = {TEXT("Back"), TEXT("Front")};
	FString ViewText;
	if (FParse::Value(FCommandLine::Get(), TEXT("EffectViews="), ViewText, /*bShouldStopOnSeparator*/ false)) ViewText.ParseIntoArray(Views, TEXT(","));
	TArray<EffectVisualScreenshots::FShot> Shots;
	Shots.Add({TEXT("Back"), 0.5f, true});
	for (const FString& View : Views)
	{
		for (float Seconds : Offsets) Shots.Add({View, Seconds});
	}
	if (!AutomationOpenMap(TEXT("/Game/Map/Wuwa")))
	{
		AddError(TEXT("Could not open /Game/Map/Wuwa."));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	// -EffectRole=jiabei -EffectMontagePath=/Game/.../AM_Jiabei_Attack01.AM_Jiabei_Attack01：截其他角色的蒙太奇
	auto* Capture = new EffectVisualScreenshots::FCaptureAttackShots(this, Attack, Shots, Directory, Pitch);
	FParse::Value(FCommandLine::Get(), TEXT("EffectRole="), Capture->RoleName);
	FParse::Value(FCommandLine::Get(), TEXT("EffectMontagePath="), Capture->MontagePathOverride);
	Capture->bDarkFloor = FParse::Param(FCommandLine::Get(), TEXT("EffectDarkFloor"));
	Capture->bNoPlay = FParse::Param(FCommandLine::Get(), TEXT("EffectNoPlay"));
	Capture->bRotateCharacter = FParse::Param(FCommandLine::Get(), TEXT("EffectRotateCharacter"));
	FParse::Value(FCommandLine::Get(), TEXT("EffectCameraHeight="), Capture->CameraHeight);
	FParse::Value(FCommandLine::Get(), TEXT("EffectCameraDistance="), Capture->CameraDistance);
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShareable(Capture));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
