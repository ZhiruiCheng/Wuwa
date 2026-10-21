#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "WuwaAnimNotifyState_RoleRotate.generated.h"

/**
 * 角色转向输入窗口（≈ 原作 TsAnimNotifyStateRoleRotate）。
 * 窗口内有移动输入时，角色直接转向输入方向：转速 = 离输入方向的角度 / 180 × 转速（偏得越多转得越快）。
 * 勾了自动朝向目标则不转向输入，只设置技能转向参数（转向技能目标、转速），要另有技能转向窗口打开才会转——同原作。
 * 窗口结束时关掉技能转向（原作 SetSkillCanRotate(false)）。原作的屏蔽标签、横板模式、骑乘判断还没做。
 */
UCLASS(meta = (DisplayName = "Role Rotate Window"))
class WUWA_API UWuwaAnimNotifyState_RoleRotate : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/** 转速（度 / 秒，偏离 180 度时的转速）。原作 旋转速度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Rotate")
	float RotateSpeed = 100.f;

	/** 原作 是否自动朝向目标。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Rotate")
	bool bAutoFaceTarget = false;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
