#include "Game/Camera/WuwaPlayerCameraManager.h"

#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Game/Camera/WuwaCameraRuntimeBridge.h"
#include "Core/Tag/WuwaGameTags.h"
#include "Game/Input/WuwaInputTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaCamera, Log, All);

AWuwaPlayerCameraManager::AWuwaPlayerCameraManager()
{
	RuntimeClass = TSoftClassPtr<UWuwaCameraRuntimeBridge>(FSoftObjectPath(TEXT("/Script/UnrealSharp.ZrCameraRuntime_C")));
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
	if (!IsValid(Runtime)) return false;
	// 创建 C# 相机系统：主屏数据 → 序列镜头 + 战斗镜头 → 战斗镜头从对象池取出相机，屏幕改看它
	Runtime->InitCamera(this);
	// 刚建好的相机系统还不知道要跟哪个角色：清掉记录，TryGetGameplayContext 接着会发现"角色变了"，补发一次 OnPossess
	RuntimePawn = nullptr;
	return true;
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
	// 记住新 Pawn，避免下次取上下文时又通知一遍
	const APlayerController* Player = GetOwningPlayerController();
	RuntimePawn = IsValid(Player) ? Player->GetPawn() : nullptr;
	// 告诉战斗镜头和序列镜头新的角色
	if (IsValid(Runtime)) Runtime->OnPossess(RuntimePawn.Get());
}

void AWuwaPlayerCameraManager::ResetCameraInput()
{
	PendingLook = FVector2D::ZeroVector;
	PendingZoom = 0.f;
}

bool AWuwaPlayerCameraManager::IsViewingFightCamera() const
{
	if (!IsValid(Runtime)) return false;
	const AActor* FightCameraActor = Runtime->GetFightCameraActor();
	// GetViewTarget = 屏幕在看的 Actor；正在混合时返回的是要混过去的那个
	return IsValid(FightCameraActor) && GetViewTarget() == FightCameraActor;
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
	// 屏幕没在看战斗镜头（特写镜头等）时，转镜头和滚轮输入都不归战斗镜头，直接吃掉
	if (!IsViewingFightCamera() || Player->IsLookInputIgnored()) return true;
	if (bLook && InputEvent.Value.GetValueType() == EInputActionValueType::Axis2D)
	{
		// 只攒起来，不再 AddYawInput：镜头角度由 C# 输入控制器改，ControlRotation 再从镜头抄过去
		const FVector2D Axis = InputEvent.Value.Get<FVector2D>();
		if (!Axis.ContainsNaN()) PendingLook += Axis;
	}
	else if (bZoom && InputEvent.Value.GetValueType() == EInputActionValueType::Axis1D)
	{
		const float Delta = InputEvent.Value.Get<float>();
		if (FMath::IsFinite(Delta)) PendingZoom = FMath::Clamp(PendingZoom + Delta, -100.f, 100.f);
	}
	return true;
}

void AWuwaPlayerCameraManager::UpdateCamera(float DeltaTime)
{
	APlayerController* Player;
	APawn* Pawn;
	// 先让 C# 把镜头算好、摆好，引擎接着在 Super 里去"看"它，同一帧就用上新位置
	if (TryGetGameplayContext(Player, Pawn)) Runtime->TickCamera(DeltaTime, PendingLook, PendingZoom);
	Super::UpdateCamera(DeltaTime);
	// 这一帧的输入已经交出去了；没交出去的（比如没有角色时）也丢掉，免得以后一次性补上，镜头猛地一跳
	ResetCameraInput();
}

int32 AWuwaPlayerCameraManager::ApplyCameraModify(const FWuwaCameraModify& Modify, float Duration, float BlendInTime, float BlendOutTime,
	float BreakBlendOutTime, UAnimSequenceBase* Animation, AActor* ModifyOwner)
{
	APlayerController* Player;
	APawn* Pawn;
	if (!TryGetGameplayContext(Player, Pawn)) return -1;
	return Runtime->ApplyCameraModify(Modify, Duration, BlendInTime, BlendOutTime, BreakBlendOutTime, Animation, ModifyOwner);
}

void AWuwaPlayerCameraManager::StopCameraModify(UAnimSequenceBase* Animation, int32 ModifyInstance)
{
	if (IsValid(Runtime)) Runtime->StopCameraModify(Animation, ModifyInstance);
}

bool AWuwaPlayerCameraManager::PlayCameraSequence(const FWuwaSequenceCameraConfig& Config, ACharacter* Character, FName AttachSocket)
{
	APlayerController* Player;
	APawn* Pawn;
	if (!TryGetGameplayContext(Player, Pawn)) return false;
	return Runtime->PlayCameraSequence(Config, Character, AttachSocket);
}

bool AWuwaPlayerCameraManager::IsPlayingCameraSequence() const
{
	return IsValid(Runtime) && Runtime->IsPlayingCameraSequence();
}

AActor* AWuwaPlayerCameraManager::GetSequenceCameraActor() const
{
	return IsValid(Runtime) ? Runtime->GetSequenceCameraActor() : nullptr;
}

AWuwaPlayerCameraManager* AWuwaPlayerCameraManager::GetForPawn(const APawn* Pawn)
{
	const APlayerController* Player = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!Player || !Player->IsLocalController()) return nullptr;
	// 正在放特写镜头时也给：镜头修改 / 新的特写镜头要不要生效，由 C# 相机系统自己按镜头模式决定
	return Cast<AWuwaPlayerCameraManager>(Player->PlayerCameraManager);
}

void AWuwaPlayerCameraManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 清理 C# 相机系统：相机还回对象池、静态数据清空。
	// C# 的静态数据在两次 PIE 之间不会自己消失，这里不清，下一局会拿到上一局的旧相机
	if (IsValid(Runtime)) Runtime->ClearCamera();
	Runtime = nullptr;
	RuntimePawn = nullptr;
	ResetCameraInput();
	Super::EndPlay(EndPlayReason);
}
