#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaSkillMotion.h"
#include "WuwaAnimNotifyState_MoveToTarget.generated.h"

/**
 * 位移吸附到目标（≈ 原作 TsAnimNotifyStatePositionBranchTarget，"位移吸附到目标位置"）。
 * 窗口开始时取技能目标（UWuwaSkillBridgeComponent::GetSkillTarget，技能开始时由锁定挑的）；没有目标整段不生效。
 * 窗口内每帧：离目标远就往目标推，够近就抵消这一帧根运动的水平位移，见 FWuwaMoveToTargetConfig。
 * 实际位移在移动组件里、根运动生效之前修正（UWuwaMovementComponent::BeginMoveToTarget）。共享的通知对象不保存运行状态。
 */
UCLASS(meta = (DisplayName = "Move To Target Window"))
class WUWA_API UWuwaAnimNotifyState_MoveToTarget : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Motion", meta = (ShowOnlyInnerProperties))
	FWuwaMoveToTargetConfig Config;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
