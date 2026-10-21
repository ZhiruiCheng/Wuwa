#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "WuwaAnimNotifyState_AddLooseTag.generated.h"

/**
 * 窗口内给角色 ASC 加一个松散 Tag，窗口结束（或蒙太奇被终止）时移除。对应原作 TsAnimNotifyStateAddTag。
 * 松散 Tag 按计数增减：前一段蒙太奇混出时与下一段的窗口重叠也不会提前丢失。
 * 例：普攻连段窗口 State.Combat.ComboWindow，输入规则用 OwnerTagQuery 判断是否还能接下一段。
 * 共享的通知对象不保存运行状态。
 */
UCLASS(meta = (DisplayName = "Add Loose Tag Window"))
class WUWA_API UWuwaAnimNotifyState_AddLooseTag : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Tag")
	FGameplayTag Tag;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

private:
	void Change(const USkeletalMeshComponent* MeshComp, bool bAdd) const;
};
