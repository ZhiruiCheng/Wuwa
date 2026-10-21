#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "WuwaAnimNotify_SkillReadyEndBridge.generated.h"

class UWuwaSkillBridgeComponent;

/** 原生层验证通知来自当前技能的实际播放实例；让位规则仍由 C# 实现。 */
UCLASS(Abstract, Blueprintable)
class WUWA_API UWuwaAnimNotify_SkillReadyEndBridge : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual void BranchingPointNotify(FBranchingPointNotifyPayload& Payload) override;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Wuwa|Combat|Skill", meta = (BlueprintProtected))
	void ReachSkillReadyEnd(UWuwaSkillBridgeComponent* Skills, int32 SkillHandle) const;
		
private:
	void DispatchReadyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEvent* NotifyEvent, int32 MontageInstanceId) const;
};
