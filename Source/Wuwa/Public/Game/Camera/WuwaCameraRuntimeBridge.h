#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Game/Camera/CameraData/WuwaCameraSettings.h"
#include "Game/Camera/CameraData/WuwaCameraFrame.h"
#include "WuwaCameraRuntimeBridge.generated.h"

/** Reflection boundary only. C# owns policy; the manager owns this UObject. */
UCLASS(Abstract, Blueprintable)
class WUWA_API UWuwaCameraRuntimeBridge : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, Category="Camera") FWuwaCameraView EvaluateCamera(FWuwaCameraFrame Frame, FWuwaCameraSettings BaseSettings);
	UFUNCTION(BlueprintNativeEvent, Category="Camera") FWuwaCameraSettings ResolveSettings(FWuwaCameraSettings BaseSettings);
	UFUNCTION(BlueprintNativeEvent, Category="Camera") int32 PushCameraMode(UObject* Source, FWuwaCameraSettings Settings, int32 Priority);
	UFUNCTION(BlueprintNativeEvent, Category="Camera") bool PopCameraMode(int32 Handle);
	UFUNCTION(BlueprintNativeEvent, Category="Camera") void ResetCamera(bool bResetUserZoom = true);
};
