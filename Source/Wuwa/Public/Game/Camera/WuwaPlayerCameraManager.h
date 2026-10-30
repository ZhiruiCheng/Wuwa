#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "Game/Input/IWuwaInputRouteHandler.h"
#include "Game/Camera/CameraData/WuwaCameraModify.h"
#include "Game/Camera/CameraData/WuwaSequenceCameraConfig.h"
#include "WuwaPlayerCameraManager.generated.h"

class ACharacter;
class UAnimSequenceBase;
class UWuwaCameraRuntimeBridge;

/**
 * 引擎这一侧的钩子：接相机输入、管 C# 相机系统的创建和清理、每帧在出画面之前调一次 C# 的 Tick，
 * 再把动画通知的"镜头修改 / 特写镜头"请求转给 C#。
 * 镜头怎么算、相机 Actor 归谁、镜头角度，都在 C#（ZrCameraController → ZRCameraModel）；
 * ControlRotation 由 C# 战斗镜头每帧从镜头角度抄过来，PCM 不再把鼠标量加到 ControlRotation 上。
 */
UCLASS()
class WUWA_API AWuwaPlayerCameraManager : public APlayerCameraManager, public IIWuwaInputRouteHandler
{
	GENERATED_BODY()
public:
	AWuwaPlayerCameraManager();
	virtual bool HandleWuwaInput_Implementation(const FWuwaInputEvent& InputEvent) override;
	virtual void UpdateCamera(float DeltaTime) override;
	void NotifyPawnChanged();
	void ResetCameraInput();
	/** 屏幕现在看的是不是战斗镜头的相机。不是的话（比如在放特写镜头），玩家的转镜头 / 滚轮输入直接丢掉。 */
	bool IsViewingFightCamera() const;

	/** 镜头修改（技能动画通知调用）：见 UWuwaCameraRuntimeBridge::ApplyCameraModify。 */
	UFUNCTION(BlueprintCallable, Category="Wuwa|Camera") int32 ApplyCameraModify(const FWuwaCameraModify& Modify, float Duration, float BlendInTime,
		float BlendOutTime, float BreakBlendOutTime, UAnimSequenceBase* Animation, AActor* ModifyOwner);
	UFUNCTION(BlueprintCallable, Category="Wuwa|Camera") void StopCameraModify(UAnimSequenceBase* Animation, int32 ModifyInstance);
	/** 特写镜头（技能动画通知调用）：见 UWuwaCameraRuntimeBridge::PlayCameraSequence。 */
	UFUNCTION(BlueprintCallable, Category="Wuwa|Camera") bool PlayCameraSequence(const FWuwaSequenceCameraConfig& Config, ACharacter* Character, FName AttachSocket);
	UFUNCTION(BlueprintPure, Category="Wuwa|Camera") bool IsPlayingCameraSequence() const;
	UFUNCTION(BlueprintPure, Category="Wuwa|Camera") AActor* GetSequenceCameraActor() const;

	/** 控制这个 Pawn 的本地玩家的相机（镜头修改 / 特写镜头只给本地玩家控制的角色）。 */
	static AWuwaPlayerCameraManager* GetForPawn(const APawn* Pawn);
	UFUNCTION(BlueprintPure, Category="Wuwa|Camera") UWuwaCameraRuntimeBridge* GetCameraRuntime() const { return Runtime; }

	UPROPERTY(EditDefaultsOnly, Category="Wuwa|Camera") TSoftClassPtr<UWuwaCameraRuntimeBridge> RuntimeClass;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool EnsureRuntime();
	bool TryGetGameplayContext(APlayerController*& OutPlayer, APawn*& OutPawn);
	UPROPERTY(Transient) TObjectPtr<UWuwaCameraRuntimeBridge> Runtime;
	TWeakObjectPtr<APawn> RuntimePawn;
	// 这一帧攒下的转镜头 / 滚轮输入，UpdateCamera 时交给 C# 的输入控制器，交完清零
	FVector2D PendingLook = FVector2D::ZeroVector;
	float PendingZoom = 0.f;

	//debug
	bool bReportedMissingRuntime = false;
};
