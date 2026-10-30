#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Game/Camera/CameraData/WuwaSequenceCameraConfig.h"
#include "WuwaAnimNotify_SequenceCamera.generated.h"

/**
 * 切到技能特写镜头：交给 C# 序列镜头的播放组件
 * （AWuwaPlayerCameraManager::PlayCameraSequence → ZRCSequenceCameraPlayerComponent）。只对本地玩家控制的角色生效。
 */
UCLASS(meta = (DisplayName = "Sequence Camera"))
class WUWA_API UWuwaAnimNotify_SequenceCamera : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** 特写镜头配置。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera")
	FWuwaSequenceCameraConfig Config;

	/** 序列的播放原点挂在角色网格的这个插槽上，跟着它走（填 None 就用开始那一刻 Root 骨骼的位置，之后不动）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera")
	FName CameraAttachSocket = TEXT("Root");

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
