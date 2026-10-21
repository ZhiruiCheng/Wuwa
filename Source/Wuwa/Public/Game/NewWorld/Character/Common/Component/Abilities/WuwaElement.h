#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "WuwaElement.generated.h"

/**
 * 元素；数值和原作 ElementInfo 的 Id 一致，存在属性 UWuwaAttributeSet::ElementType 里（≈ 原作 Proto_ElementPropertyType）。
 * 原作伤害公式 CalculateElementMatchUpRate(伤害元素, 目标元素) 按这个 Id 判断克制。
 */
UENUM(BlueprintType)
enum class EWuwaElement : uint8
{
	None = 0,
	Glacio = 1 UMETA(DisplayName = "冷凝"),
	Fusion = 2 UMETA(DisplayName = "热熔"),
	Electro = 3 UMETA(DisplayName = "导电"),
	Aero = 4 UMETA(DisplayName = "气动"),
	Spectro = 5 UMETA(DisplayName = "衍射"),
	Havoc = 6 UMETA(DisplayName = "湮灭"),
};

namespace WuwaElement
{
	/** 属性里存的浮点数 → 元素；超出范围是 None。 */
	inline EWuwaElement FromAttributeValue(float Value)
	{
		const int32 Id = FMath::RoundToInt(Value);
		return Id >= 0 && Id <= static_cast<int32>(EWuwaElement::Havoc) ? static_cast<EWuwaElement>(Id) : EWuwaElement::None;
	}

	/** 元素 → 标签 Element.<名字>（DT_ElementInfo 的行名）；None 返回空标签。 */
	inline FGameplayTag ToTag(EWuwaElement Element)
	{
		if (Element == EWuwaElement::None) return FGameplayTag();
		const FString Name = StaticEnum<EWuwaElement>()->GetNameStringByValue(static_cast<int64>(Element));
		return FGameplayTag::RequestGameplayTag(*(TEXT("Element.") + Name), false);
	}
}
