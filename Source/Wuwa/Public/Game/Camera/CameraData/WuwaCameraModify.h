#pragma once

#include "CoreMinimal.h"
#include "WuwaCameraModify.generated.h"

/**
 * 一次镜头修改（≈ 原作 TsAnimNotifyCameraModify 的 相机修改配置，CameraModifyController）：只改勾选的项，其余沿用当前镜头。
 * 旋转按角色朝向算（原作 起始角度 = 角色旋转，目标 = 起始 + ArmRotation）；Additional 叠加在当前镜头上。
 * 没做：曲线（ArmLength / ArmRotation / Fov / Offset 的 FloatCurve）、条件、按缩放输入中止、ArmOffset、镜头 Lens。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCameraModify
{
	GENERATED_BODY()

	/** 原作 Priority：同时有多个修改时高的生效（项目压入相机模式栈时再加 100，高于普通模式）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera") int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|ArmLength") bool bModifyArmLength = false;
	/** 原作 ArmLength：镜头距离（不跟玩家缩放）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|ArmLength", meta=(EditCondition="bModifyArmLength", Units="cm")) float ArmLength = 400.f;
	/** 原作 ArmLengthAdditional：在当前距离上加。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|ArmLength", meta=(Units="cm")) float ArmLengthAdditional = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Rotation") bool bModifyPitch = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Rotation") bool bModifyYaw = false;
	/** 原作 ArmRotation：相对角色朝向的镜头俯仰 / 偏航（勾了 bModifyPitch / bModifyYaw 才用）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Rotation") FRotator ArmRotation = FRotator::ZeroRotator;
	/** 原作 ArmRotationAdditional：在当前镜头角度上加。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Rotation") FRotator ArmRotationAdditional = FRotator::ZeroRotator;

	/** 原作 IsModifiedCameraOffsetX/Y/Z + CameraOffset：把镜头中心偏移的这几个轴设成 CameraOffset（镜头空间：X 前 Y 右 Z 上）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Offset") bool bModifyOffsetX = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Offset") bool bModifyOffsetY = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Offset") bool bModifyOffsetZ = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Offset") FVector CameraOffset = FVector::ZeroVector;
	/** 原作 CameraOffsetAdditional：在当前镜头中心偏移上加（镜头空间）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Offset") FVector CameraOffsetAdditional = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Fov") bool bModifyFov = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Fov", meta=(EditCondition="bModifyFov")) float Fov = 90.f;

	/** 原作 IsLockInput：修改期间不响应玩家转镜头 / 缩放（改了角度时也会锁转镜头）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Input") bool bLockInput = false;

	/** 原作 StopModifyOnMontageEnd（默认开）：蒙太奇播完 / 被打断时提前退回。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera") bool bStopOnMontageEnd = true;
};
