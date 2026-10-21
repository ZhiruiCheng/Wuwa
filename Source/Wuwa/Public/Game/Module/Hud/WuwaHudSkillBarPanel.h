#pragma once

#include "CoreMinimal.h"
#include "Game/LGUI/View/WuwaUiPanel.h"
#include "WuwaHudSkillBarPanel.generated.h"

class UTexture2D;
class UUISprite;
class UWuwaRoleDataAsset;
struct FStreamableHandle;
struct FWuwaTeamMemberUiData;
enum class EWuwaSkillButtonType : uint8;

/**
 * HUD 右下的技能栏（HUD_Main 里的 PnlSkillFirst）：
 * - 跟着当前角色换 E / R 的图标（≈ 鸣潮按 SkillButton 表刷新按钮）。图标来自当前角色 UWuwaRoleDataAsset::SkillButtons；
 *   角色没配的按钮保留预制体里的图。T（探索工具）、Q（声骸）是公共的，不跟角色。
 * - 按键按下时对应按钮播点击圈（≈ 鸣潮 BattleSkillItem.OnInputAction → ClickEffect，注册表 10 号的 UINiagara NS_Fx_LGUI_FightSkill）：
 *   预制体里是 UWuwaUINiagara（播放按原作重建的 NS_UI_FightSkill）；旧预制体里是烘好的环形贴图，按原作曲线补间（0.35 秒，缩放 0.98→1.2）。
 *   事件 Event.Hud.SkillButtonPressed 由 UWuwaSkillButtonUiController 发。
 *
 * PnlSkillFirst 上的 WuwaUiComponentsRegistry：
 *   0 共鸣技能 E 的图标（UITexture）
 *   1 共鸣解放 R 的图标（UITexture）
 *   2~5 T、E、Q、R 四个按钮的点击圈（UWuwaUINiagara；旧预制体是平时透明的 UITexture）
 *
 * R 按钮外圈的共鸣能量环（≈ 原作 BattleSkillItem.GetUltraComponent → BattleSkillUltraItem）：异步加载 UWuwaUISettings::SkillUltPrefab，
 * 挂到 R 图标的父节点（PnlAdapt）下、图标后面；当前角色的 共鸣能量 / 上限 设成环（预制体注册表 0 号 BarUlt，Radial360 填充）的填充量（原作 SetBarPercent）。
 */
UCLASS()
class WUWA_API UWuwaHudSkillBarPanel : public UWuwaUiPanel
{
	GENERATED_BODY()

public:
	// 测试用
	const UWuwaRoleDataAsset* GetShownRole() const { return ShownRole; }
	UTexture2D* GetShownIcon(int32 Index) const;
	/** 某个按钮的点击圈播放过几次。 */
	int32 GetClickCount(EWuwaSkillButtonType ButtonType) const;
	/** 某个按钮的点击特效节点（UWuwaUINiagara 或旧的 UITexture）。 */
	class UUIItem* GetClickEffect(EWuwaSkillButtonType ButtonType) const;
	/** 能量环当前的填充量（还没加载好是 -1）。 */
	float GetUltPercent() const;
	AActor* GetUltActor() const { return UltActor; }

protected:
	virtual void OnRegisterComponent() override;
	virtual void OnAfterShow() override;
	virtual void OnAddEventListener() override;
	virtual void OnBeforeDestroy() override;

private:
	void Refresh();
	void PlayClick(EWuwaSkillButtonType ButtonType);
	void LoadUltRing();
	void ApplyEnergy(const FWuwaTeamMemberUiData& Member);
	void ApplyCurrentEnergy();

	UPROPERTY(Transient)
	TObjectPtr<const UWuwaRoleDataAsset> ShownRole;

	TMap<EWuwaSkillButtonType, TWeakObjectPtr<class ULTweener>> ClickTweens;
	TMap<EWuwaSkillButtonType, int32> ClickCounts;

	TSharedPtr<FStreamableHandle> UltLoadHandle;
	UPROPERTY(Transient)
	TObjectPtr<AActor> UltActor;
	TWeakObjectPtr<UUISprite> UltBar;
};
