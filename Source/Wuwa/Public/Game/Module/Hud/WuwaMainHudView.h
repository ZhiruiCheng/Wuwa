#pragma once

#include "CoreMinimal.h"
#include "Game/LGUI/View/WuwaUiView.h"
#include "WuwaMainHudView.generated.h"

class UWuwaHudRoleStatePanel;
class UWuwaHudSkillBarPanel;
class UWuwaHudTeamSlotPanel;
class UWuwaBossStatePanel;

/**
 * 主 HUD（DT_UiViews 里 MainHUD 那一行的 ViewClass，预制体 /Game/LGUI/HUD/HUD_Main）。
 * 自己不处理队伍数据，只负责把预制体里嵌着的子组件包成子 Panel；子 Panel 跟着 HUD 一起显示隐藏。
 *
 * HUD_Main 根节点上的 WuwaUiComponentsRegistry：
 *   0~2 三个队伍槽的根节点（从上到下依次是 1、2、3 号）→ UWuwaHudTeamSlotPanel
 *   3   技能栏 PnlSkillFirst → UWuwaHudSkillBarPanel
 *   4   当前角色状态 FightRoleHp → UWuwaHudRoleStatePanel
 * Boss 血条（原作战斗界面里的 BossStatePanel）单独的预制体，生成在 HUD 根节点下，见 UWuwaBossStatePanel。
 */
UCLASS()
class WUWA_API UWuwaMainHudView : public UWuwaUiView
{
	GENERATED_BODY()

public:
	static constexpr int32 TeamSlotCount = 3;
	static constexpr int32 SkillBarIndex = 3;
	static constexpr int32 RoleStateIndex = 4;

	const TArray<TObjectPtr<UWuwaHudTeamSlotPanel>>& GetTeamSlots() const { return TeamSlots; }
	UWuwaHudSkillBarPanel* GetSkillBar() const { return SkillBar; }
	UWuwaHudRoleStatePanel* GetRoleState() const { return RoleState; }
	UWuwaBossStatePanel* GetBossState() const { return BossState; }

protected:
	virtual void OnRegisterComponent() override;
	virtual void OnCreate() override;
	virtual void OnBeforeDestroy() override;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UWuwaHudTeamSlotPanel>> TeamSlots;

	UPROPERTY(Transient)
	TObjectPtr<UWuwaHudSkillBarPanel> SkillBar;

	UPROPERTY(Transient)
	TObjectPtr<UWuwaHudRoleStatePanel> RoleState;

	UPROPERTY(Transient)
	TObjectPtr<UWuwaBossStatePanel> BossState;
};
