#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Game/Camera/CameraData/WuwaCameraModify.h"
#include "WuwaAnimNotify_CameraModify.generated.h"

/**
 * 技能镜头修改：触发时让 C# 战斗镜头的镜头修改控制器开始一段修改（AWuwaPlayerCameraManager::ApplyCameraModify）。
 * 之后淡入 BlendInTime → 保持 Duration → 淡出 BlendOutTime 全由镜头修改控制器每帧做；
 * Modify.bStopOnMontageEnd 时蒙太奇播完 / 被顶掉会提前结束，按 BreakBlendOutTime 回去。只对本地玩家控制的角色生效。
 * 例：嘉贝 EX 普攻 3 起跳时 3 秒内把镜头拉远到 10 米、俯 20°、中心抬 130 cm、FOV 85，跳在空中也拍得到。
 */
UCLASS(meta = (DisplayName = "Camera Modify"))
class WUWA_API UWuwaAnimNotify_CameraModify : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera")
	FWuwaCameraModify Modify;

	/** 持续时间：淡入完后保持多久（不含淡出）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera", meta = (Units = "s"))
	float Duration = 0.f;

	/** 淡入时间：从当前样子过渡到目标用的时间。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera", meta = (Units = "s"))
	float BlendInTime = 0.f;

	/** 淡出时间：保持结束后过渡回去用的时间。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera", meta = (Units = "s"))
	float BlendOutTime = 0.f;

	/** 打断淡出时间：被提前结束（蒙太奇停了、换人等）时回去用的时间。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera", meta = (Units = "s"))
	float BreakBlendOutTime = 0.2f;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};

/**
 * 窗口版技能镜头修改：窗口开始时开始修改（持续时间传 100 秒，等于一直保持），
 * 窗口结束时提前结束，按 BreakBlendOutTime 回去。
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

	/** 淡出时间（窗口版只有保持超过 100 秒才会用到）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera", meta = (Units = "s"))
	float BlendOutTime = 0.f;

	/** 打断淡出时间：窗口结束时回去用的时间。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera", meta = (Units = "s"))
	float BreakBlendOutTime = 0.2f;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

private:
	/** 每个网格各自的修改编号（通知对象是共享的）。 */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, int32> Instances;
};
