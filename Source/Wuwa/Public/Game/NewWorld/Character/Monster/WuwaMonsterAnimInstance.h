#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "WuwaMonsterAnimInstance.generated.h"

class UAnimSequenceBase;

/** UWuwaMonsterAnimInstance 的代理：待机循环 → DefaultSlot（和动画蓝图里 Slot 节点同一套：注册插槽、按蒙太奇权重混合）。 */
USTRUCT()
struct WUWA_API FWuwaMonsterAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FWuwaMonsterAnimInstanceProxy() = default;
	explicit FWuwaMonsterAnimInstanceProxy(UAnimInstance* InAnimInstance) : FAnimInstanceProxy(InAnimInstance) {}

	static const FName SlotName;

protected:
	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void Update(float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	void EvaluateIdle(FPoseContext& Output) const;

	TObjectPtr<UAnimSequenceBase> Idle = nullptr;
	float IdleTime = 0.f;
	float SlotNodeWeight = 0.f;
	float SourceWeight = 1.f;
	float TotalNodeWeight = 0.f;
};

/**
 * 怪物的临时动画实例（原作怪物用 ABP_MonsterCommon 的子蓝图，项目还没有）：循环播所属角色的 IdleAnimation，DefaultSlot 上播蒙太奇（受击、技能）。
 * 蒙太奇的根运动照常交给 CMC（RootMotionFromMontagesOnly）。
 */
UCLASS(Transient, NotBlueprintable)
class WUWA_API UWuwaMonsterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

	friend struct FWuwaMonsterAnimInstanceProxy;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FWuwaMonsterAnimInstanceProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete static_cast<FWuwaMonsterAnimInstanceProxy*>(InProxy); }
};
