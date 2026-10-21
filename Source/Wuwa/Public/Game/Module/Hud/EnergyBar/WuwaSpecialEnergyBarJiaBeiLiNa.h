#pragma once

#include "CoreMinimal.h"
#include "Game/Module/Hud/EnergyBar/WuwaSpecialEnergyBarBase.h"
#include "Game/Module/Hud/EnergyBar/WuwaSpecialEnergyBarSlot.h"
#include "WuwaSpecialEnergyBarJiaBeiLiNa.generated.h"

/**
 * 嘉贝莉娜的两格条（≈ 原作 SpecialEnergyBarJiaBeiLiNaSlot，继承格子条）：
 *   第 0 格（红）= 特殊能量 2 / 上限（原作写死读 Proto_SpecialEnergy2），自己一套平滑；
 *   第 1 格（主）= 表里那一对（SpecialEnergyIndex，原作 AttributeId 62 = 特殊能量 1）。
 *   变身中（弹药 特殊能量 3 > 0）主格改显示弹药 特殊能量 3 / 上限：原作是另一条变身条（配置 120801，读 66 / 65）加一组切换动画，
 *   项目没建那条，简化成同一格换数据源（弹药只在变身中有：强化 E 设成满，解除变身时 1208003317 清空）。
 * 不要底线、不改颜色（原作 ForceHideBottomLine / OverrideColor）。
 */
UCLASS()
class WUWA_API UWuwaSpecialEnergyBarJiaBeiLiNaSlot : public UWuwaSpecialEnergyBarSlot
{
	GENERATED_BODY()

public:
	UWuwaSpecialEnergyBarJiaBeiLiNaSlot();
	float GetRedPercent() const { return RedPercentMachine.GetCurrent(); }

protected:
	virtual void OnMemberData(const FWuwaTeamMemberUiData& Member, bool bInitial) override;
	virtual void TickBar(float DeltaSeconds) override;
	virtual void RefreshBarPercent() override;
	virtual float GetTargetPercent(const FWuwaTeamMemberUiData& Member) const override;

private:
	FWuwaEnergyPercentMachine RedPercentMachine;
};

/**
 * 嘉贝莉娜专属能量条（≈ 原作 SpecialEnergyBarJiaBeiLiNa，表里 ExtraType 120800，预制体 UiItem_BarJiabeilina）。
 * 原作还有：变身状态（标签 332111384）切换强化外观、变身用的另一条（配置 120801）、Buff 剩余时间的进度（配置 120802）、一组补间动画。
 * 这些依赖的标签、Buff 项目里还没有，目前只做两格条；外框保持预制体里的普通状态。
 *
 * 根节点上的 WuwaUiComponentsRegistry：0 两格条的根节点（UiItem_BarSlot2）
 */
UCLASS()
class WUWA_API UWuwaSpecialEnergyBarJiaBeiLiNa : public UWuwaSpecialEnergyBarBase
{
	GENERATED_BODY()

public:
	UWuwaSpecialEnergyBarJiaBeiLiNaSlot* GetBarItem() const { return BarItem; }

	virtual void SetMemberIndex(int32 InMemberIndex) override;

protected:
	virtual void OnRegisterComponent() override;
	virtual void OnCreate() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UWuwaSpecialEnergyBarJiaBeiLiNaSlot> BarItem;
};
