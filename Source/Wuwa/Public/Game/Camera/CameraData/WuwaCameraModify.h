#pragma once

#include "CoreMinimal.h"
#include "WuwaCameraModify.generated.h"

/**
 * 一次镜头修改的配置（技能动画通知里填）。
 * C# 的镜头修改控制器按它把镜头从当前样子过渡到目标、保持、再过渡回去；只改勾选的项。
 * 角度按角色朝向算（目标 = 修改开始时的角色旋转 + ArmRotation）；Additional 叠在目标上。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCameraModify
{
	GENERATED_BODY()

	/** 优先级：正在修改时，优先级更低的新修改不生效。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera") 
	int32 Priority = 0;

	/** 改距离：镜头距离改成 ArmLength。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|ArmLength") 
	bool bModifyArmLength = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|ArmLength", meta=(EditCondition="bModifyArmLength", Units="cm")) 
	float ArmLength = 0.f;
	/** 在目标距离上再加这么多。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|ArmLength", meta=(Units="cm")) float ArmLengthAdditional = 0.f;

	/** 改俯仰 / 偏航：镜头转到 角色旋转 + ArmRotation。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Rotation") bool bModifyPitch = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Rotation") bool bModifyYaw = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Rotation") FRotator ArmRotation = FRotator::ZeroRotator;
	/** 在目标角度上再加（没勾改俯仰 / 偏航时，加在修改开始时的镜头角度上）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Rotation") FRotator ArmRotationAdditional = FRotator::ZeroRotator;

	/** 改镜头偏移：勾了的轴（镜头空间：X 前 Y 右 Z 上）改成 CameraOffset。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Offset") bool bModifyOffsetX = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Offset") bool bModifyOffsetY = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Offset") bool bModifyOffsetZ = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Offset") FVector CameraOffset = FVector::ZeroVector;
	/** 在镜头偏移上再加。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Offset") FVector CameraOffsetAdditional = FVector::ZeroVector;

	/** 改视野：视野改成 Fov。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Fov") bool bModifyFov = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Fov", meta=(EditCondition="bModifyFov")) float Fov = 0.f;

	/** 锁输入：修改期间整个输入控制器关掉（不能转镜头、不能缩放）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Input") bool bLockInput = false;
	/** 覆盖输入：只锁住被修改的那几项对应的玩家输入（改了俯仰锁俯仰，改了距离锁缩放）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Input") bool bOverrideCameraInput = false;

	/** 蒙太奇停了就结束：触发它的蒙太奇播完、或被别的动画顶掉时，提前结束（按打断淡出时间回去）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera") bool bStopOnMontageEnd = true;
	/** 换人不结束（默认换人就结束）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera") bool bSwitchModifier = false;
	/** 强制播放：镜头修改被配置关掉时也照样生效。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera") bool bForcePlayModify = false;

	
	/** 结束时角度留在修改后的样子（关掉则转回修改前）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Reset") bool bResetFinalArmRotation = true;
	/**
	 * 角度留下时，俯仰改成 ResetFinalSpecificPitch。
	 * （数值字段不能叫 ResetFinalArmRotationToSpecificPitch：UnrealSharp 去掉 bool 的 b 前缀后会和它重名）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Reset") bool bResetFinalArmRotationToSpecificPitch = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Reset", meta=(EditCondition="bResetFinalArmRotationToSpecificPitch")) float ResetFinalSpecificPitch = -7.f;
	/** 角度留下时，偏航改成 角色朝向 + ResetFinalSpecificYaw。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Reset") bool bResetFinalArmRotationToSpecificYaw = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Reset", meta=(EditCondition="bResetFinalArmRotationToSpecificYaw")) float ResetFinalSpecificYaw = 0.f;
	/** 结束时距离留在修改后的值（关掉则回到修改前）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Reset") bool bResetFinalArmLength = false;
	/** 距离留下时，改成 ResetFinalSpecificArmLength。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Reset") bool bResetFinalArmLengthToSpecificValue = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|Reset", meta=(EditCondition="bResetFinalArmLengthToSpecificValue", Units="cm")) float ResetFinalSpecificArmLength = 0.f;
};
