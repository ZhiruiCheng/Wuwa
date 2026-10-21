#pragma once

#include "CoreMinimal.h"
#include "Game/LGUI/View/WuwaUiPanel.h"
#include "Game/Module/Hud/EnergyBar/WuwaSpecialEnergyBarBase.h"
#include "WuwaSpecialEnergyBarSlot.generated.h"

/**
 * 格子能量条里的一格（≈ 原作 SpecialEnergyBarSlotItem，预制体 PnlSlot）。三种状态：
 *   空（≤0）    底板 + 刻度
 *   部分（0~1）  底板 + 刻度 + 填充（BarPart 按比例改宽度，左端固定）+ 前景刻度（同样露出比例那么宽）
 *   满（≥1 且按键可用）原作只显示满格的 Niagara（NiaFull / NiaFullBg）；LGUI3 放不了界面 Niagara，这里显示满的填充层
 * 原作 BarPart 是 FilledSliced（LGUI3 没有这种类型），所以改宽度；刻度原作是平铺的，导出时已按原作格宽排成一条贴图；前景刻度改宽度并裁 UV 露出一部分。
 *
 * PnlSlot 上的 WuwaUiComponentsRegistry（原作还有 PnlClip 和 4 个 Niagara，项目里没有）：
 *   0 SprBg 底板  1 BarPart 填充  2 SprGraduate 刻度  3 SprGraduateFront 前景刻度
 */
UCLASS()
class WUWA_API UWuwaSpecialEnergyBarSlotItem : public UWuwaUiPanel
{
	GENERATED_BODY()

public:
	/** ≈ 原作 UpdatePercent(t, keyEnable)。 */
	void UpdatePercent(float Percent, bool bKeyEnable);
	void SetBarColor(const FColor& Color);
	void SetPointColor(const FColor& Color);

	// 测试用
	int32 GetVisualState() const { return State; }
	float GetShownFill() const { return ShownFill; }

protected:
	virtual void OnRegisterComponent() override;
	virtual void OnCreate() override;

private:
	void SetFill(float Percent);

	/** -1 空、0 部分、1 满（和原作一样）。 */
	int32 State = INDEX_NONE - 1;
	float BarFullWidth = 0.f;
	float FrontFullWidth = 0.f;
	float ShownFill = 0.f;
};

/**
 * 格子能量条（≈ 原作 SpecialEnergyBarSlot，预制体 UiItem_BarSlotN）：总比例 × 格数，第 i 格显示 (比例 × 格数 − i)。
 * 颜色：填充用表里的 EffectColor、前景刻度用 PointColor（OverrideColor 的子类不改）。
 * 底线：变身外框里的格子条（IsMorph 且有外框贴图）或 ForceHideBottomLine 时隐藏（原作同样的规则）。
 *
 * 根节点上的 WuwaUiComponentsRegistry：0 ~ SlotNum-1 每一格（PnlSlot），SlotNum 底线 SprDesc（bHasBottomLine 时才有）
 */
UCLASS()
class WUWA_API UWuwaSpecialEnergyBarSlot : public UWuwaSpecialEnergyBarBase
{
	GENERATED_BODY()

public:
	/** 嵌在变身外框里（≈ 原作 IsMorph）。 */
	bool bIsMorph = false;
	/** 不要底线（嘉贝莉娜的条）；在 CreateByActor 之前设，决定注册表里有没有底线那一项。 */
	bool bForceHideBottomLine = false;
	/** 子类自己管颜色（≈ 原作 OverrideColor）。 */
	bool bOverrideColor = false;

	const TArray<TObjectPtr<UWuwaSpecialEnergyBarSlotItem>>& GetSlotItems() const { return SlotItems; }

protected:
	virtual void OnRegisterComponent() override;
	virtual void OnCreate() override;
	virtual void OnStart() override;
	virtual void OnBarPercentChanged() override { RefreshBarPercent(); }
	virtual void OnKeyEnableChanged() override { RefreshBarPercent(); }

	/** ≈ 原作 RefreshBarPercent：每一格按自己那段比例刷新。子类（嘉贝莉娜）重写成按格读不同属性。 */
	virtual void RefreshBarPercent();
	bool HasBottomLine() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWuwaSpecialEnergyBarSlotItem>> SlotItems;
};
