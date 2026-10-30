#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Game/Camera/CameraData/WuwaCameraModify.h"
#include "Game/Camera/CameraData/WuwaSequenceCameraConfig.h"
#include "WuwaCameraRuntimeBridge.generated.h"

class AActor;
class ACharacter;
class APawn;
class APlayerCameraManager;
class UAnimSequenceBase;

/**
 * C++ 和 C# 相机系统之间的门：PCM 和动画通知只在固定时机调用这些函数，C# 子类（UZrCameraRuntime）重写它们并转给 ZrCameraController。
 * 镜头的拥有和计算都在 C#，这个类自己不做事。
 */
UCLASS(Abstract, Blueprintable)
class WUWA_API UWuwaCameraRuntimeBridge : public UObject
{
	GENERATED_BODY()
public:
	/** 创建 C# 相机系统：主屏数据 + 战斗镜头 + 序列镜头（战斗镜头从对象池取出相机，并让屏幕看它）。 */
	UFUNCTION(BlueprintNativeEvent, Category="Camera") void InitCamera(APlayerCameraManager* CameraManager);
	/** 清理 C# 相机系统：相机还回对象池，静态数据清空。 */
	UFUNCTION(BlueprintNativeEvent, Category="Camera") void ClearCamera();
	/** 换了控制的角色：战斗镜头和序列镜头都换跟随的角色。 */
	UFUNCTION(BlueprintNativeEvent, Category="Camera") void OnPossess(APawn* Pawn);
	/** 每帧在引擎出画面之前调用。LookInput / ZoomInput = 这一帧攒下的转镜头 / 滚轮输入（增强输入给的原始值）。 */
	UFUNCTION(BlueprintNativeEvent, Category="Camera") void TickCamera(float DeltaSeconds, FVector2D LookInput, float ZoomInput);
	/** 战斗镜头的相机 Actor：屏幕在看它，就说明正在用战斗镜头。 */
	UFUNCTION(BlueprintNativeEvent, Category="Camera") AActor* GetFightCameraActor() const;

	/**
	 * 开始一段镜头修改（技能动画通知调用）。
	 * Duration / BlendInTime / BlendOutTime = 淡入之后保持多久 / 淡入时间 / 淡出时间；BreakBlendOutTime = 被提前结束时回去用的时间；
	 * Animation = 触发它的动画（蒙太奇停了就提前结束、StopCameraModify 对号用）。返回这次修改的编号，没生效返回 -1。
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Camera") int32 ApplyCameraModify(const FWuwaCameraModify& Settings, float Duration, float BlendInTime,
		float BlendOutTime, float BreakBlendOutTime, UAnimSequenceBase* Animation, AActor* Owner);
	/** 提前结束一段镜头修改：动画和编号都对得上才结束。 */
	UFUNCTION(BlueprintNativeEvent, Category="Camera") void StopCameraModify(UAnimSequenceBase* Animation, int32 ModifyInstance);

	/** 播技能特写镜头。 */
	UFUNCTION(BlueprintNativeEvent, Category="Camera") bool PlayCameraSequence(const FWuwaSequenceCameraConfig& Config, ACharacter* Character, FName AttachSocket);
	/** 特写镜头是否正在播。 */
	UFUNCTION(BlueprintNativeEvent, Category="Camera") bool IsPlayingCameraSequence() const;
	/** 正在播的特写镜头用的相机（测试 / 调试用）。 */
	UFUNCTION(BlueprintNativeEvent, Category="Camera") AActor* GetSequenceCameraActor() const;
};
