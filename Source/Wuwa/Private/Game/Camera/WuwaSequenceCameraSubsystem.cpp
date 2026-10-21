#include "Game/Camera/WuwaSequenceCameraSubsystem.h"

#include "Components/SkeletalMeshComponent.h"
#include "DefaultLevelSequenceInstanceData.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Core/ActorComponent/UIItem.h"
#include "Game/LGUI/Core/WuwaUiManager.h"
#include "Game/LGUI/View/WuwaUiView.h"
#include "Game/Module/Hud/WuwaMainHudController.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaSequenceCamera, Log, All);

namespace WuwaSequenceCamera
{
	const FName CameraTag(TEXT("SequenceCamera"));
	const FName RoleTag(TEXT("Role"));
}

UWuwaSequenceCameraSubsystem* UWuwaSequenceCameraSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWuwaSequenceCameraSubsystem>() : nullptr;
}

bool UWuwaSequenceCameraSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UWuwaSequenceCameraSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWuwaSequenceCameraSubsystem, STATGROUP_Tickables);
}

void UWuwaSequenceCameraSubsystem::Deinitialize()
{
	StopSequence(true);
	Super::Deinitialize();
}

bool UWuwaSequenceCameraSubsystem::PlayCameraSequence(const FWuwaSequenceCameraConfig& Config, ACharacter* InCharacter, FName AttachSocket)
{
	UWorld* World = GetWorld();
	ULevelSequence* Asset = Config.CameraSequence.LoadSynchronous();
	if (!World || !IsValid(InCharacter) || !Asset)
	{
		UE_LOG(LogWuwaSequenceCamera, Warning, TEXT("Camera sequence %s not played (character %s)."), *Config.CameraSequence.ToString(), *GetNameSafe(InCharacter));
		return false;
	}
	// 只有玩家正在控制的角色切镜头（原作 CheckCameraSequenceCondition：生效客户端）
	APlayerController* PC = Cast<APlayerController>(InCharacter->GetController());
	if (!PC || !PC->IsLocalController()) return false;
	StopSequence(true);

	// 原点：挂在角色 Mesh 插槽上的 Actor（原作 Oxr.K2_AttachToComponent(Mesh, CameraAttachSocket)）
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* OriginActor = World->SpawnActor<AActor>(AActor::StaticClass(), InCharacter->GetActorTransform(), Params);
	USceneComponent* Root = NewObject<USceneComponent>(OriginActor, TEXT("Root"));
	OriginActor->SetRootComponent(Root);
	Root->RegisterComponent();
	if (USkeletalMeshComponent* Mesh = InCharacter->GetMesh())
	{
		Root->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocket);
	}

	FMovieSceneSequencePlaybackSettings Settings;
	Settings.bDisableCameraCuts = true;   // 视角由这里切（混合时间同原作）
	ALevelSequenceActor* SequenceActor = nullptr;
	ULevelSequencePlayer::CreateLevelSequencePlayer(World, Asset, Settings, SequenceActor);
	if (!SequenceActor)
	{
		OriginActor->Destroy();
		return false;
	}
	SequenceActor->bOverrideInstanceData = true;
	if (UDefaultLevelSequenceInstanceData* Data = Cast<UDefaultLevelSequenceInstanceData>(SequenceActor->DefaultInstanceData))
	{
		Data->TransformOriginActor = OriginActor;
	}
	SequenceActor->SetBindingByTag(WuwaSequenceCamera::RoleTag, { InCharacter }, false);
	SequenceActor->GetSequencePlayer()->Play();

	Sequence = SequenceActor;
	Origin = OriginActor;
	Character = InCharacter;
	Current = Config;
	PlayTime = 0.0;
	const double Length = SequenceActor->GetSequencePlayer()->GetDuration().AsSeconds();
	EndTime = Config.SpecificSequenceTime > 0.f ? FMath::Min(Length, static_cast<double>(Config.SpecificSequenceTime)) : Length;
	BlendOutUntil = -1.0;

	// 序列第一次求值时生成相机；切视角
	for (const FMovieSceneObjectBindingID& Binding : SequenceActor->FindNamedBindings(WuwaSequenceCamera::CameraTag))
	{
		for (UObject* Bound : SequenceActor->GetSequencePlayer()->GetBoundObjects(Binding))
		{
			if (AActor* Camera = Cast<AActor>(Bound)) CameraActor = Camera;
		}
	}
	if (AActor* Camera = CameraActor.Get()) PC->SetViewTargetWithBlend(Camera, Current.BlendInTime);
	else UE_LOG(LogWuwaSequenceCamera, Warning, TEXT("%s has no SequenceCamera binding."), *Asset->GetName());

	if (Config.bHideHud && Config.HideHudTime > 0.f)
	{
		SetHudHidden(true);
		HudHiddenUntil = Config.HideHudTime;
	}
	return true;
}

void UWuwaSequenceCameraSubsystem::Tick(float DeltaTime)
{
	if (!Sequence.IsValid()) return;
	PlayTime += DeltaTime;
	// 相机生成物在序列第一次求值时才出现：Play 时还没有就在这里补切视角
	if (!CameraActor.IsValid() && BlendOutUntil < 0.0)
	{
		for (const FMovieSceneObjectBindingID& Binding : Sequence->FindNamedBindings(WuwaSequenceCamera::CameraTag))
			for (UObject* Bound : Sequence->GetSequencePlayer()->GetBoundObjects(Binding))
				if (AActor* Camera = Cast<AActor>(Bound)) CameraActor = Camera;
		ACharacter* Owner = Character.Get();
		if (APlayerController* PC = Owner ? Cast<APlayerController>(Owner->GetController()) : nullptr; PC && CameraActor.IsValid())
			PC->SetViewTargetWithBlend(CameraActor.Get(), Current.BlendInTime);
	}
	if (bHudHidden && PlayTime >= HudHiddenUntil) SetHudHidden(false);
	if (BlendOutUntil < 0.0 && PlayTime >= EndTime) StopSequence(false);
	else if (BlendOutUntil >= 0.0 && PlayTime >= BlendOutUntil) Cleanup();
}

void UWuwaSequenceCameraSubsystem::StopSequence(bool bImmediate)
{
	if (!Sequence.IsValid())
	{
		Cleanup();
		return;
	}
	// 停在当前帧，视角混回角色，混完再停序列（相机是序列的生成物，先停会让混合的起点消失）
	if (ULevelSequencePlayer* Player = Sequence->GetSequencePlayer()) Player->Pause();
	ACharacter* Owner = Character.Get();
	APlayerController* PC = Owner ? Cast<APlayerController>(Owner->GetController()) : nullptr;
	if (PC && PC->GetViewTarget() == CameraActor.Get()) PC->SetViewTargetWithBlend(Owner, bImmediate ? 0.f : Current.BlendOutTime);
	if (bImmediate || Current.BlendOutTime <= 0.f) Cleanup();
	else BlendOutUntil = PlayTime + Current.BlendOutTime;
}

void UWuwaSequenceCameraSubsystem::Cleanup()
{
	if (bHudHidden) SetHudHidden(false);
	if (ALevelSequenceActor* Actor = Sequence.Get())
	{
		if (ULevelSequencePlayer* Player = Actor->GetSequencePlayer()) Player->Stop();
		Actor->Destroy();
	}
	if (AActor* Actor = Origin.Get()) Actor->Destroy();
	Sequence.Reset();
	Origin.Reset();
	CameraActor.Reset();
	Character.Reset();
	BlendOutUntil = -1.0;
}

void UWuwaSequenceCameraSubsystem::SetHudHidden(bool bHidden)
{
	bHudHidden = bHidden;
	const ACharacter* Owner = Character.Get();
	const APlayerController* PC = Owner ? Cast<APlayerController>(Owner->GetController()) : nullptr;
	UWuwaUiManager* Ui = PC ? UWuwaUiManager::Get(PC) : nullptr;
	UWuwaUiView* Hud = Ui ? Ui->GetView(UWuwaMainHudController::MainHudViewName) : nullptr;
	if (UUIItem* Root = Hud ? Hud->GetRootItem() : nullptr) Root->SetIsUIActive(!bHidden);
}
