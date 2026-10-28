#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "Game/Input/IWuwaInputRouteHandler.h"
#include "Game/Camera/CameraData/WuwaCameraModify.h"
#include "Game/Camera/CameraData/WuwaCameraSettings.h"
#include "Game/Camera/CameraData/WuwaCameraFrame.h"
#include "WuwaPlayerCameraManager.generated.h"

class UWuwaCameraMode;
class UWuwaCameraRuntimeBridge;

/** Per-player engine adapter: input routing, lifetime, collision and final UE POV. */
UCLASS()
class WUWA_API AWuwaPlayerCameraManager : public APlayerCameraManager, public IIWuwaInputRouteHandler
{
	GENERATED_BODY()
public:
	AWuwaPlayerCameraManager();
	virtual bool HandleWuwaInput_Implementation(const FWuwaInputEvent& InputEvent) override;
	virtual void ProcessViewRotation(float DeltaTime, FRotator& OutViewRotation, FRotator& OutDeltaRot) override;
	virtual void UpdateCamera(float DeltaTime) override;
	void NotifyPawnChanged();
	void ResetCameraInput();

	UFUNCTION(BlueprintCallable, Category="Wuwa|Camera") int32 PushCameraMode(UObject* Source, UWuwaCameraMode* Mode);
	UFUNCTION(BlueprintCallable, Category="Wuwa|Camera") bool PopCameraMode(int32 Handle);
	/**
	 * 原作镜头修改（技能镜头）：以当前生效的镜头为起点，按 Modify 改出一组设置，压成优先级 100 + Modify.Priority 的相机模式。
	 * BlendInTime / BlendOutTime = 原作 淡入时间 / 淡出时间。返回句柄，结束时 PopCameraMode。只对本地玩家正在看的角色生效。
	 */
	UFUNCTION(BlueprintCallable, Category="Wuwa|Camera") int32 PushCameraModify(UObject* Source, const FWuwaCameraModify& Modify, float BlendInTime, float BlendOutTime);
	/** Pawn 当前是不是这个相机在看的本地玩家角色（镜头修改只给它）。 */
	static AWuwaPlayerCameraManager* GetForPawn(const APawn* Pawn);
	UFUNCTION(BlueprintPure, Category="Wuwa|Camera") UWuwaCameraRuntimeBridge* GetCameraRuntime() const { return Runtime; }
	
	
	//默认相机数据
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wuwa|Camera") FWuwaCameraSettings DefaultSettings;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wuwa|Camera") TObjectPtr<UWuwaCameraMode> DefaultMode;
	
	UPROPERTY(EditDefaultsOnly, Category="Wuwa|Camera") TSoftClassPtr<UWuwaCameraRuntimeBridge> RuntimeClass;

	
protected:
	virtual void UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool EnsureRuntime();
	bool TryGetGameplayContext(APlayerController*& OutPlayer, APawn*& OutPawn);
	FWuwaCameraSettings GetBaseSettings() const;
	FVector ResolveCollision(const FWuwaCameraView& View, const APawn* Pawn) const;
	UPROPERTY(Transient) TObjectPtr<UWuwaCameraRuntimeBridge> Runtime;
	TWeakObjectPtr<APawn> RuntimePawn;
	float PendingZoom = 0.f;
	uint64 LastEvaluatedFrame = MAX_uint64;
	FWuwaCameraView CachedView;
	
	
	//debug
	bool bReportedMissingRuntime = false;
};
