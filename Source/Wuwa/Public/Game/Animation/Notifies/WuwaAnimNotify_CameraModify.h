#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Game/Camera/CameraData/WuwaCameraModify.h"
#include "WuwaAnimNotify_CameraModify.generated.h"

/**
 * 技能镜头修改（≈ 原作 TsAnimNotifyCameraModify）：触发时把镜头按 Modify 改掉（AWuwaPlayerCameraManager::PushCameraModify），
 * Duration 秒后退回（BlendOutTime 混回）；Duration ≤ 0 = 蒙太奇结束时退回（原作 持续时间 0）；Modify.bStopOnMontageEnd 时
 * 蒙太奇播完 / 被打断也提前退回。只对本地玩家正在看的角色生效。
 * 例：嘉贝 EX 普攻 3 起跳时 3 秒内把镜头拉远到 10 米、俯 20°、中心抬 130 cm、FOV 85，跳在空中也拍得到。
 * 原作还有：淡入 / 淡出曲线、打断淡出时间（项目被打断也按 BlendOutTime 退回）、生效客户端类型、条件。
 */
UCLASS(meta = (DisplayName = "Camera Modify"))
class WUWA_API UWuwaAnimNotify_CameraModify : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera")
	FWuwaCameraModify Modify;

	/** 原作 持续时间（不含淡出）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera", meta = (Units = "s"))
	float Duration = 0.f;

	/** 原作 淡入时间。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera", meta = (Units = "s"))
	float BlendInTime = 0.f;

	/** 原作 淡出时间。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera", meta = (Units = "s"))
	float BlendOutTime = 0.f;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};

/**
 * 窗口版技能镜头修改（≈ 原作 TsAnimNotifyStateCameraModify）：窗口开始时改，窗口结束时退回（BlendOutTime 混回）。
 */
UCLASS(meta = (DisplayName = "Camera Modify Window"))
class WUWA_API UWuwaAnimNotifyState_CameraModify : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera")
	FWuwaCameraModify Modify;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera", meta = (Units = "s"))
	float BlendInTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera", meta = (Units = "s"))
	float BlendOutTime = 0.f;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

private:
	/** 每个网格各自的相机模式句柄（通知对象是共享的）。 */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, int32> Handles;
};
