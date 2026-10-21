#include "Game/Camera/WuwaPlayerCameraManager.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Game/Camera/WuwaCameraMode.h"
#include "Game/Camera/WuwaCameraRuntimeBridge.h"
#include "Game/Common/WuwaGameTags.h"
#include "Game/Input/WuwaInputTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaCamera, Log, All);

AWuwaPlayerCameraManager::AWuwaPlayerCameraManager()
{
	RuntimeClass = TSoftClassPtr<UWuwaCameraRuntimeBridge>(FSoftObjectPath(TEXT("/Script/UnrealSharp.WuwaCameraRuntime_C")));
}

FWuwaCameraSettings AWuwaPlayerCameraManager::GetBaseSettings() const
{
	return IsValid(DefaultMode) ? DefaultMode->Settings : DefaultSettings;
}

bool AWuwaPlayerCameraManager::EnsureRuntime()
{
	if (IsValid(Runtime)) return true;
	UClass* Class = RuntimeClass.LoadSynchronous();
	if (!Class || Class->HasAnyClassFlags(CLASS_Abstract))
	{
		if (!bReportedMissingRuntime)
		{
			UE_LOG(LogWuwaCamera, Error, TEXT("Camera runtime unavailable: %s. Build ManagedWuwa before playing."), *RuntimeClass.ToString());
			bReportedMissingRuntime = true;
		}
		return false;
	}
	Runtime = NewObject<UWuwaCameraRuntimeBridge>(this, Class);
	return IsValid(Runtime);
}

bool AWuwaPlayerCameraManager::TryGetGameplayContext(APlayerController*& OutPlayer, APawn*& OutPawn)
{
	OutPlayer = GetOwningPlayerController();
	OutPawn = IsValid(OutPlayer) ? OutPlayer->GetPawn() : nullptr;
	if (!IsValid(OutPlayer) || !OutPlayer->IsLocalController() || !IsValid(OutPawn)) return false;
	if (!EnsureRuntime()) return false;
	if (RuntimePawn.Get() != OutPawn)
	{
		NotifyPawnChanged();
		RuntimePawn = OutPawn;
	}
	return true;
}

void AWuwaPlayerCameraManager::NotifyPawnChanged()
{
	ResetCameraInput();
	// 记住新 Pawn，避免下次取上下文时再次重置，并误用已回退的默认模式配置。
	const APlayerController* Player = GetOwningPlayerController();
	RuntimePawn = IsValid(Player) ? Player->GetPawn() : nullptr;
	LastEvaluatedFrame = MAX_uint64;
	CachedView = {};
	if (IsValid(Runtime))
	{
		// 在清除临时模式前读取当前配置；缩放偏好默认随玩家保留。
		const bool bResetUserZoom = Runtime->ResolveSettings(GetBaseSettings()).bResetZoomOnPawnChange;
		Runtime->ResetCamera(bResetUserZoom);
	}
}

void AWuwaPlayerCameraManager::ResetCameraInput()
{
	PendingZoom = 0.f;
}

bool AWuwaPlayerCameraManager::HandleWuwaInput_Implementation(const FWuwaInputEvent& InputEvent)
{
	const auto& InputTags = FWuwaGameTags::Get();
	const bool bLook = InputEvent.InputTag == InputTags.Player_Common_Camera_Rotate;
	const bool bZoom = InputEvent.InputTag == InputTags.Player_Common_Camera_Zoom;
	if (!bLook && !bZoom) return false;
	// Started and Triggered may both arrive in the first frame. Consume the axis only once.
	if (InputEvent.Phase != EWuwaInputPhase::Triggered) return true;
	APlayerController* Player;
	APawn* Pawn;
	if (!TryGetGameplayContext(Player, Pawn)) return false;
	// External CameraActor/Sequencer view targets own their input policy.
	if (Player->GetViewTarget() != Pawn || Player->IsLookInputIgnored()) return true;
	if (bLook && InputEvent.Value.GetValueType() == EInputActionValueType::Axis2D)
	{
		const FVector2D Axis = InputEvent.Value.Get<FVector2D>();
		if (!Axis.ContainsNaN())
		{
			// Preserve the project's existing sensitivity/inversion through UE's input path.
			Player->AddYawInput(Axis.X);
			Player->AddPitchInput(Axis.Y);
		}
	}
	else if (bZoom && InputEvent.Value.GetValueType() == EInputActionValueType::Axis1D)
	{
		const float Delta = InputEvent.Value.Get<float>();
		if (FMath::IsFinite(Delta)) PendingZoom = FMath::Clamp(PendingZoom + Delta, -100.f, 100.f);
	}
	return true;
}

void AWuwaPlayerCameraManager::ProcessViewRotation(float DeltaTime, FRotator& OutViewRotation, FRotator& OutDeltaRot)
{
	APlayerController* Player;
	APawn* Pawn;
	if (TryGetGameplayContext(Player, Pawn))
	{
		const auto Settings = Runtime->ResolveSettings(GetBaseSettings());
		ViewPitchMin = Settings.PitchMin;
		ViewPitchMax = Settings.PitchMax;
		if (!Settings.bAllowRotationInput || Player->GetViewTarget() != Pawn) OutDeltaRot = FRotator::ZeroRotator;
	}
	Super::ProcessViewRotation(DeltaTime, OutViewRotation, OutDeltaRot);
}

void AWuwaPlayerCameraManager::UpdateCamera(float DeltaTime)
{
	Super::UpdateCamera(DeltaTime);
	// An external view must never accumulate wheel events to replay on returning to gameplay.
	PendingZoom = 0.f;
}

void AWuwaPlayerCameraManager::UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime)
{
	APlayerController* Player;
	APawn* Pawn;
	if (!TryGetGameplayContext(Player, Pawn) || OutVT.Target != Pawn)
	{
		Super::UpdateViewTargetInternal(OutVT, DeltaTime);
		return;
	}
	if (LastEvaluatedFrame != GFrameCounter)
	{
		FWuwaCameraFrame Frame;
		Frame.TargetLocation = Pawn->GetActorLocation();
		Frame.ControlRotation = Player->GetControlRotation();
		Frame.DeltaSeconds = DeltaTime;
		Frame.ZoomDelta = PendingZoom;
		PendingZoom = 0.f;
		CachedView = Runtime->EvaluateCamera(Frame, GetBaseSettings());
		LastEvaluatedFrame = GFrameCounter;
	}
	if (!CachedView.bValid || CachedView.Location.ContainsNaN() || CachedView.Rotation.ContainsNaN())
	{
		Super::UpdateViewTargetInternal(OutVT, DeltaTime);
		return;
	}
	OutVT.POV.Location = ResolveCollision(CachedView, Pawn);
	OutVT.POV.Rotation = CachedView.Rotation;
	OutVT.POV.FOV = CachedView.FieldOfView;
}

FVector AWuwaPlayerCameraManager::ResolveCollision(const FWuwaCameraView& View, const APawn* Pawn) const
{
	if (!View.bCollisionTest || !GetWorld()) return View.Location;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WuwaCamera), false, Pawn);
	Query.AddIgnoredActor(this);
	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByChannel(Hit, View.Pivot, View.Location, FQuat::Identity,
		ECC_Camera, FCollisionShape::MakeSphere(FMath::Max(1.f, View.ProbeRadius)), Query);
	return bHit ? (Hit.bStartPenetrating ? View.Pivot : Hit.Location) : View.Location;
}

int32 AWuwaPlayerCameraManager::PushCameraMode(UObject* Source, UWuwaCameraMode* Mode)
{
	APlayerController* Player;
	APawn* Pawn;
	if (!IsValid(Source) || !IsValid(Mode) || !TryGetGameplayContext(Player, Pawn)) return 0;
	return Runtime->PushCameraMode(Source, Mode->Settings, Mode->Priority);
}

int32 AWuwaPlayerCameraManager::PushCameraModify(UObject* Source, const FWuwaCameraModify& Modify, float BlendInTime, float BlendOutTime)
{
	APlayerController* Player;
	APawn* Pawn;
	if (!IsValid(Source) || !TryGetGameplayContext(Player, Pawn)) return 0;
	// 没勾的项沿用当前生效的镜头（原作 Additional 也是叠在当前镜头上）
	FWuwaCameraSettings Settings = Runtime->ResolveSettings(GetBaseSettings());
	if (Modify.bModifyArmLength)
	{
		Settings.ArmLength = Modify.ArmLength;
		Settings.bUseUserZoom = false;
	}
	Settings.ArmLength = FMath::Max(0.f, Settings.ArmLength + Modify.ArmLengthAdditional);
	Settings.MinArmLength = FMath::Min(Settings.MinArmLength, Settings.ArmLength);
	Settings.MaxArmLength = FMath::Max(Settings.MaxArmLength, Settings.ArmLength);
	// 角度：原作目标 = 角色旋转 + ArmRotation（只改勾选的轴），再叠 Additional；项目镜头 = 控制旋转 + RotationOffset，换算成偏移
	const FRotator Control = Player->GetControlRotation();
	FRotator Target = (Control + Settings.RotationOffset).GetNormalized();
	const FRotator Facing = Pawn->GetActorRotation();
	if (Modify.bModifyPitch) Target.Pitch = Facing.Pitch + Modify.ArmRotation.Pitch;
	if (Modify.bModifyYaw) Target.Yaw = Facing.Yaw + Modify.ArmRotation.Yaw;
	Target.Pitch += Modify.ArmRotationAdditional.Pitch;
	Target.Yaw += Modify.ArmRotationAdditional.Yaw;
	const FRotator Delta = (Target - Control).GetNormalized();
	Settings.RotationOffset = FRotator(Delta.Pitch, Delta.Yaw, Settings.RotationOffset.Roll);
	const bool bRotated = Modify.bModifyPitch || Modify.bModifyYaw || !Modify.ArmRotationAdditional.IsNearlyZero();
	// 原作改了角度就锁住对应轴的玩家输入；IsLockInput 全锁
	if (bRotated || Modify.bLockInput) Settings.bAllowRotationInput = false;
	if (Modify.bLockInput || Modify.bModifyArmLength) Settings.bAllowZoomInput = false;
	// 镜头中心偏移按镜头水平朝向的局部空间（X 前 Y 右 Z 上）
	const FRotator Heading(0.f, Target.Yaw, 0.f);
	FVector Local = Heading.UnrotateVector(Settings.PivotOffset);
	if (Modify.bModifyOffsetX) Local.X = Modify.CameraOffset.X;
	if (Modify.bModifyOffsetY) Local.Y = Modify.CameraOffset.Y;
	if (Modify.bModifyOffsetZ) Local.Z = Modify.CameraOffset.Z;
	Settings.PivotOffset = Heading.RotateVector(Local + Modify.CameraOffsetAdditional);
	if (Modify.bModifyFov) Settings.FieldOfView = Modify.Fov;
	Settings.BlendTime = FMath::Max(0.f, BlendInTime);
	Settings.BlendOutTime = BlendOutTime;
	return Runtime->PushCameraMode(Source, Settings, 100 + Modify.Priority);
}

AWuwaPlayerCameraManager* AWuwaPlayerCameraManager::GetForPawn(const APawn* Pawn)
{
	const APlayerController* Player = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!Player || !Player->IsLocalController()) return nullptr;
	AWuwaPlayerCameraManager* Camera = Cast<AWuwaPlayerCameraManager>(Player->PlayerCameraManager);
	return Camera && Player->GetViewTarget() == Pawn ? Camera : nullptr;
}

bool AWuwaPlayerCameraManager::PopCameraMode(int32 Handle)
{
	return IsValid(Runtime) && Handle > 0 && Runtime->PopCameraMode(Handle);
}

void AWuwaPlayerCameraManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	NotifyPawnChanged();
	Runtime = nullptr;
	Super::EndPlay(EndPlayReason);
}
