#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "WuwaAnimNotifyState_KeepAwayFromGround.generated.h"

class UCurveFloat;

/**
 * 角色和地面保持一定距离（≈ 原作 TsAnimNotifyStateKeepAwayFromGround）。窗口内每帧量脚下离地高度（胶囊半径的球往下扫）：
 * - 低于 MinHeight：往上推 (MinHeight - 高度) × 插值比例（默认三次曲线，窗口结束那帧补满），每帧最多 MaxSpeed × 帧时间；
 * - MaxHeight > 0、已经高于 MaxHeight 且这一帧还在往上走：把这一帧的上升抵消掉。
 * 空中普攻 4 用它保持离地 80~300；心眼·跃（AirAttack05）起手时把角色往上推（离地本身靠根运动，见 UWuwaMovementComponent::PhysWalking）。
 */
UCLASS(meta = (DisplayName = "Keep Away From Ground"))
class WUWA_API UWuwaAnimNotifyState_KeepAwayFromGround : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/** 原作 距离水平面最小高度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Motion", meta = (Units = "cm"))
	float MinHeight = 0.f;

	/** 原作 距离水平面最大高度（≤ 0 = 不限上限；也是向下量高度的距离）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Motion", meta = (Units = "cm"))
	float MaxHeight = 2000.f;

	/** 原作 MaxSpeed：往上推的最大速度。≤ 0 时整个窗口不生效。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Motion", meta = (Units = "cm/s"))
	float MaxSpeed = 2000.f;

	/** 原作 MoveCurve：自定义插值曲线（空 = 默认三次曲线）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Motion")
	TObjectPtr<UCurveFloat> MoveCurve;

	/** 原作 不使用默认插值曲线：没有 MoveCurve 时每帧直接推满（比例 1）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Motion")
	bool bIgnoreInterpolation = false;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	/** 原作 BaseMoveComponent.GetHeightAboveGround：从脚底往下扫 Distance（胶囊半径的球），返回离地高度，没扫到返回 Distance。 */
	static float GetHeightAboveGround(const class ACharacter* Character, float Distance);

private:
	struct FWindow
	{
		float NowTime = 0.f;
		float TotalTime = 0.f;
		double LastZ = 0.0;
	};
	/** 通知对象是蒙太奇共享的，每个角色一份（原作 paramsMap 按实体 ID）。 */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FWindow> Windows;

	float GetRate(const FWindow& Window, float DeltaTime) const;
};
