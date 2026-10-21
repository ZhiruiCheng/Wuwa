#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "WuwaAnimNotify_SpawnBullet.generated.h"

/**
 * 添加子弹（≈ 原作 TsAnimNotifyReSkillEvent，"添加子弹"）：动画走到这一帧时，按列表里的子弹 ID 各生成一颗子弹。
 * 瞬时通知，触发一次；子弹存在多久、何时判定由子弹表决定（持续时间 / 碰撞判定延迟 / 碰撞判定时长）。
 * 原作的 子弹数据名（单颗）和 子弹id数组（多颗）在这里统一成 BulletIds。原作还有出生 / 初速度偏移数组、随机权重、
 * 召唤者子弹、骨骼等配置，长离 / 珂莱塔的 101 个通知都没用到，先不做。
 */
/** 原作 TsAnimNotifySkillBehavior 条件「是否有技能目标」。 */
UENUM(BlueprintType)
enum class EWuwaBulletSkillTargetCondition : uint8
{
	/** 不检查。 */
	Any,
	/** 有技能目标才生成。 */
	HasTarget,
	/** 没有技能目标才生成。 */
	NoTarget
};

UCLASS(meta = (DisplayName = "Spawn Bullet"))
class WUWA_API UWuwaAnimNotify_SpawnBullet : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** 子弹 ID（子弹表行名，例如 1205001000）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullet")
	TArray<FName> BulletIds;

	/**
	 * 生成条件（原作 TsAnimNotifySkillBehavior「创建子弹」行为的条件组，全部满足才生成；普通 添加子弹 通知没有条件）。
	 * 技能目标：技能开始时锁定组件挑的目标（UWuwaSkillBridgeComponent::GetSkillTarget）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullet|Condition")
	EWuwaBulletSkillTargetCondition SkillTargetCondition = EWuwaBulletSkillTargetCondition::Any;

	/** 施法者身上要有这些 Tag（原作 施法者标签检测）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullet|Condition")
	FGameplayTagContainer RequiredOwnerTags;

	/** 施法者身上不能有这些 Tag（原作 施法者标签检测 + 取反）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullet|Condition")
	FGameplayTagContainer BlockedOwnerTags;

	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	/** 发射者当前所属的技能 ID：正在播这段蒙太奇的 GA 的技能 ID；没有时为 0。 */
	static int32 ResolveSkillId(const AActor* Attacker);
};
