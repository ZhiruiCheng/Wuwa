#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Game/Camera/CameraData/WuwaCameraSettings.h"
#include "WuwaCameraMode.generated.h"

/** Immutable shared configuration; runtime requests and zoom never live in this asset. */
UCLASS(BlueprintType)
class WUWA_API UWuwaCameraMode : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera") FWuwaCameraSettings Settings;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera") int32 Priority = 0;
};
