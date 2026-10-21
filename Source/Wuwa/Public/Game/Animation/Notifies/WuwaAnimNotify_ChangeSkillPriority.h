#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "WuwaAnimNotify_ChangeSkillPriority.generated.h"

/**
 * 原作 TsAnimNotifyChangeSkillPriority：技能播到这里把当前技能的打断等级改成 Priority（例：长离 E 起手改成 7、空中下落攻击落地改成 2）。
 * 只改这个蒙太奇所属的当前主技能（按播放实例核对），迟到的通知不会改到新技能上。
 */
UCLASS(meta = (DisplayName = "Change Skill Priority"))
class WUWA_API UWuwaAnimNotify_ChangeSkillPriority : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (ClampMin = "0", ClampMax = "255"))
	int32 Priority = 0;

	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual void BranchingPointNotify(FBranchingPointNotifyPayload& Payload) override;

private:
	void Apply(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEvent* NotifyEvent, int32 MontageInstanceId) const;
};
