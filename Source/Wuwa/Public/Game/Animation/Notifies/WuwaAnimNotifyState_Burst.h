#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "WuwaAnimNotifyState_Burst.generated.h"

/**
 * 角色放大招（≈ 原作 TsAnimNotifyStateBurst）：窗口内不能切人（State.Formation.SwitchForbid）、无敌（State.Combat.Invincible），
 * 都是松散 Tag 计数，结束时减掉。原作还改移动组件的 HitPriority（推人优先级），项目没有推人优先级，不做。
 */
UCLASS(meta = (DisplayName = "Burst State"))
class WUWA_API UWuwaAnimNotifyState_Burst : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/** 原作 不能切人。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Burst")
	bool bForbidSwitch = true;

	/** 原作 是否无敌。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Burst")
	bool bInvincible = true;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override { return TEXT("Burst State"); }

private:
	void Change(const USkeletalMeshComponent* MeshComp, int32 Delta) const;
};
