#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Game/Camera/WuwaSequenceCameraSubsystem.h"
#include "WuwaAnimNotify_SequenceCamera.generated.h"

/** 切到技能特写镜头（≈ 原作 TsAnimNotifySwitchSequenceCamera）：见 UWuwaSequenceCameraSubsystem。 */
UCLASS(meta = (DisplayName = "Sequence Camera"))
class WUWA_API UWuwaAnimNotify_SequenceCamera : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** 原作 特写镜头配置。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera")
	FWuwaSequenceCameraConfig Config;

	/** 原作 CameraAttachSocket：序列原点挂在角色 Mesh 的这个插槽上。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera")
	FName CameraAttachSocket = TEXT("Root");

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
