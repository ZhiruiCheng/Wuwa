#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "GameplayEffectComponent.h"
#include "WuwaAttributeEventComponent.generated.h"

class UGameplayEffect;

/**
 * 原作 buff 额外效果 6（AttributeEventEffects + AttributeIntervalCheck）：buff 生效期间监听持有者的一个属性，
 * 属性值进入区间 (下界, 上界] 时给持有者加 EffectsOnEnter；离开区间时 bRemoveOnLeave 就把这次加的撤掉。
 * 万分比模式下比较的是 属性 / 上限属性 × 10000。buff 移除时只停止监听，加过的不撤（同原作）。
 * 例：嘉贝变身 buff 1208001001 监听 SpecialEnergy3 进入 (-1, 0]（弹药打光）→ 加 1208001002（0.1 秒后解除变身）；
 *     长离被动 1205104040 监听 SpecialEnergy1 进入 (3, 50]（心火 4 层）→ 加 1205104041（授予 心火4层 Tag），离开就撤。
 * 只在有权威的一端运行（加 buff 本来就只在权威端）。
 */
UCLASS(DisplayName = "Wuwa Attribute Event", CollapseCategories)
class WUWA_API UWuwaAttributeEventComponent : public UGameplayEffectComponent
{
	GENERATED_BODY()

public:
	/** 监听的属性（原作 额外效果参数 0 = 属性 ID）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Attribute Event")
	FGameplayAttribute Attribute;

	/** 万分比模式（原作参数 1 = 1）：比较 属性 / 上限 × 10000，上限属性见 MaxAttribute。 */
	UPROPERTY(EditDefaultsOnly, Category = "Attribute Event")
	bool bPerTenThousand = false;

	/** 万分比模式的上限属性（原作 attributeIdsWithMax）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Attribute Event", meta = (EditCondition = "bPerTenThousand"))
	FGameplayAttribute MaxAttribute;

	/** 区间下界（不含；原作 额外效果成长参数 1）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Attribute Event")
	float LowerBound = -1.f;

	/** 区间上界（含；原作 额外效果成长参数 2）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Attribute Event")
	float UpperBound = -1.f;

	/** 进入区间时给持有者加的 buff（原作参数 3，目标类型 0 = 加 buff）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Attribute Event")
	TArray<TSubclassOf<UGameplayEffect>> EffectsOnEnter;

	/** 离开区间时撤掉进入时加的 buff（原作参数 4 = 1）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Attribute Event")
	bool bRemoveOnLeave = false;

	virtual bool OnActiveGameplayEffectAdded(FActiveGameplayEffectsContainer& ActiveGEContainer, FActiveGameplayEffect& ActiveGE) const override;

	/** 当前值是否在区间里（万分比模式按上限换算；上限为 0 时不在）。 */
	bool IsInInterval(const class UAbilitySystemComponent& ASC) const;
};
