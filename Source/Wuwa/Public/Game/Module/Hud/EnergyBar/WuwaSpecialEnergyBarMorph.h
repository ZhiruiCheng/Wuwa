#pragma once

#include "CoreMinimal.h"
#include "Game/Module/Hud/EnergyBar/WuwaSpecialEnergyBarBase.h"
#include "WuwaSpecialEnergyBarMorph.generated.h"

class UWuwaSpecialEnergyBarSlot;

/**
 * 变身外框 + 里面的格子条（≈ 原作 SpecialEnergyBarMorph，预制体 UiItem_BarPointMorp，长离用）：
 * - 外框贴图：平时用表里的 Texture（TexturePath），按键可用（满）时换成 EnableTexture（EnableTexturePath）——原作 RefreshIcon；
 * - 里面的条：原作按 PrefabType 选（3 点、4 格子、9 点+刻度），项目实现了格子（Type = Morph 时用 UWuwaSpecialEnergyBarSlot，IsMorph）。
 * 原作还有开始特效、可用时的额外特效（Niagara）和 Buff 剩余时间的闪烁，项目还没有。
 *
 * 根节点上的 WuwaUiComponentsRegistry：0 TexMorp 外框（UITexture），1 里面格子条的根节点（UiItem_BarSlotN）
 */
UCLASS()
class WUWA_API UWuwaSpecialEnergyBarMorph : public UWuwaSpecialEnergyBarBase
{
	GENERATED_BODY()

public:
	UWuwaSpecialEnergyBarSlot* GetBarItem() const { return BarItem; }
	/** 测试用：现在外框显示的贴图是不是"可用"那张。 */
	bool IsShowingEnableTexture() const { return bShowingEnable; }

	virtual void SetMemberIndex(int32 InMemberIndex) override;

protected:
	virtual void OnRegisterComponent() override;
	virtual void OnCreate() override;
	virtual void OnBarPercentChanged() override { RefreshIcon(); }
	virtual void OnKeyEnableChanged() override { RefreshIcon(); }

private:
	void RefreshIcon();

	UPROPERTY(Transient)
	TObjectPtr<UWuwaSpecialEnergyBarSlot> BarItem;

	bool bShowingEnable = false;
	bool bIconShown = false;
};
