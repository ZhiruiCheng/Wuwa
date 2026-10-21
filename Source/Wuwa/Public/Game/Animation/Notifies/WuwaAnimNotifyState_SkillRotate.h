#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "WuwaAnimNotifyState_SkillRotate.generated.h"

class UCurveFloat;

/**
 * 技能转向窗口（≈ 原作 TsAnimNotifyStateRotate，"旋转到黑板目标或技能目标或输入方向"）。
 * 窗口内允许技能转向：勾了自动朝向目标就转向技能目标（没有技能目标不转），否则转向移动输入方向；每帧最多转 转速 × 帧时间。
 * 勾了平滑旋转时转速按剩余角度和曲线进度算（不超过转速），窗口结束时刚好转到位。
 * 转向在移动组件里执行（UWuwaMovementComponent::SetSkillCanRotate 等）。原作的黑板目标、定向旋转、横板模式、屏蔽标签还没做。
 */
UCLASS(meta = (DisplayName = "Skill Rotate Window"))
class WUWA_API UWuwaAnimNotifyState_SkillRotate : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/** 转速（度 / 秒）。原作 旋转速度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Rotate")
	float RotateSpeed = 100.f;

	/** 转向技能目标；否则转向移动输入方向。原作 是否自动朝向目标。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Rotate")
	bool bAutoFaceTarget = false;

	/** 转速按剩余角度 × 曲线进度算。原作 是否平滑旋转。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Rotate")
	bool bSmooth = false;

	/** 平滑旋转的进度曲线（输入 0~1）；不填按线性。原作 Curve。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Rotate", meta = (EditCondition = "bSmooth"))
	TObjectPtr<UCurveFloat> Curve;

	/** 目标方向再偏转一个角度。原作 是否应用旋转偏移 / 旋转偏移。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Rotate")
	bool bApplyOffset = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Rotate", meta = (EditCondition = "bApplyOffset"))
	float Offset = 0.f;

	/** 离目标方向小于这个角度时停转，停转后大于继续阈值才再转（> 0 才生效）。原作 停止旋转阈值 / 继续旋转阈值。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Rotate")
	float PauseThreshold = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Rotate")
	float ResumeThreshold = 0.f;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
