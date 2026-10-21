#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "WuwaAnimNotifyState_AbsoluteTimeStop.generated.h"

/** 动画和子弹冻结（≈ 原作 TsAnimNotifyStateAbsoluteTimeStop）：窗口内除了自己以外的单位暂停，见 UWuwaTimeStopSubsystem。 */
UCLASS(meta = (DisplayName = "Absolute Time Stop"))
class WUWA_API UWuwaAnimNotifyState_AbsoluteTimeStop : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/** 原作 是否冻结移动效果：真 = 动画和移动一起停；假 = 只停动画。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|TimeStop")
	bool bStopMove = true;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override { return TEXT("Absolute Time Stop"); }
};
