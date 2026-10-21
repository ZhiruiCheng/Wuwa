#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WuwaUiSlicedFill.generated.h"

class UUIItem;

/**
 * 九宫格填充（≈ 原作 Kuro LGUI 的 UISpriteType::FilledSliced，LGUI3 没有）：
 * 节点本身是左端固定的裁剪容器（LGUICanvas 裁剪矩形），下面一张满宽的九宫格贴图；填充比例改容器宽度，贴图不缩，露出左边那一截。
 * 由 WuwaKuroHudBuild 按原作 FilledSliced 精灵生成；界面代码用 SetFillAmount（原作 UISprite.SetFillAmount）。
 */
UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaUiSlicedFill : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 满格时的宽度（项目画布单位）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fill")
	float FullWidth = 0.f;

	UFUNCTION(BlueprintCallable, Category = "Wuwa|UI")
	void SetFillAmount(float Amount);

	UFUNCTION(BlueprintPure, Category = "Wuwa|UI")
	float GetFillAmount() const { return FillAmount; }

	/** 节点（UIItem）的填充：带本组件的按容器宽度，UISprite 用自带的 fillAmount。 */
	static void SetFillAmountOn(UUIItem* Item, float Amount);

private:
	float FillAmount = 1.f;
};
