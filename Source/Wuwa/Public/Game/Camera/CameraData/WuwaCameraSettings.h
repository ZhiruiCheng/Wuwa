#pragma once

#include "CoreMinimal.h"
#include "WuwaCameraSettings.generated.h"

/** 弹簧臂：镜头到焦点的距离和碰撞检测。距离单位厘米。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCameraArmSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arm") float ArmLength = 400.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arm", meta=(ClampMin="0")) float MinArmLength = 150.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arm", meta=(ClampMin="0")) float MaxArmLength = 800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arm") bool bCollisionTest = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arm", meta=(ClampMin="1")) float ProbeRadius = 12.f;
};

/** 偏移与视野：焦点偏移、镜头角度偏移、FOV。角度单位度。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCameraOffsetSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Offset") FVector PivotOffset = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Offset") FRotator RotationOffset = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Offset", meta=(ClampMin="5", ClampMax="170")) float FieldOfView = 90.f;
};

/** 旋转限制：玩家转镜头的俯仰范围和开关。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCameraRotationSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation") float PitchMin = -89.9f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation") float PitchMax = 89.9f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rotation") bool bAllowRotationInput = true;
};

/** 缩放：玩家滚轮缩放的步长、平滑和开关。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCameraZoomSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zoom", meta=(ClampMin="0")) float ZoomStep = 50.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zoom", meta=(ClampMin="0")) float ZoomInterpSpeed = 12.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zoom") bool bAllowZoomInput = true;
	/** Skill profiles can ignore the user's exploration zoom without overwriting it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zoom") bool bUseUserZoom = true;
	/** 切换 Pawn 时是否重置玩家缩放偏好；按切换前生效的相机模式判断。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zoom") bool bResetZoomOnPawnChange = false;
};

/** 过渡：切入 / 切出这个模式的混合时长（秒）。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCameraBlendSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blend", meta=(ClampMin="0")) float BlendTime = .2f;
	/** 这个模式被移除时混回下一个模式用的时长；< 0 = 用下一个模式的 BlendTime（原作镜头修改的 淡出时间）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Blend") float BlendOutTime = -1.f;
};

/** A complete gameplay camera profile, grouped by concern. */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCameraSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera") FWuwaCameraArmSettings Arm;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera") FWuwaCameraOffsetSettings Offset;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera") FWuwaCameraRotationSettings Rotation;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera") FWuwaCameraZoomSettings Zoom;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera") FWuwaCameraBlendSettings Blend;
};
