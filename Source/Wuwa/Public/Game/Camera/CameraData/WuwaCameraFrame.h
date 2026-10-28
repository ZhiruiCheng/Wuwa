#pragma once

#include "CoreMinimal.h"
#include "WuwaCameraFrame.generated.h"

//层间互通,C++层打包发给脚本层的数据
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCameraFrame
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category="Camera") FVector TargetLocation = FVector::ZeroVector;//跟踪目标的位置
	UPROPERTY(BlueprintReadWrite, Category="Camera") FRotator ControlRotation = FRotator::ZeroRotator;//Controller当前的朝向
	UPROPERTY(BlueprintReadWrite, Category="Camera") float DeltaSeconds = 0.f;//过度时间
	UPROPERTY(BlueprintReadWrite, Category="Camera") float ZoomDelta = 0.f;//Zoom的距离参数
};

/** Script output, before world collision and UE camera modifiers/shakes. */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCameraView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, Category="Camera") bool bValid = false;
	UPROPERTY(BlueprintReadWrite, Category="Camera") FVector Pivot = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite, Category="Camera") FVector Location = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite, Category="Camera") FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY(BlueprintReadWrite, Category="Camera") float FieldOfView = 90.f;
	UPROPERTY(BlueprintReadWrite, Category="Camera") float ArmLength = 400.f;
	UPROPERTY(BlueprintReadWrite, Category="Camera") bool bCollisionTest = true;
	UPROPERTY(BlueprintReadWrite, Category="Camera") float ProbeRadius = 12.f;
};
