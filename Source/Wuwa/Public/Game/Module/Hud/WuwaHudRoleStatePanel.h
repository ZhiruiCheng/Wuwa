#pragma once

#include "CoreMinimal.h"
#include "Game/LGUI/View/WuwaUiPanel.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaElement.h"
#include "WuwaHudRoleStatePanel.generated.h"

class AActor;
class UWuwaRoleDataAsset;
class UWuwaSpecialEnergyBarBase;
class UWuwaHudBuffBar;
struct FWuwaBuffUiData;
struct FWuwaSpecialEnergyBarInfo;
struct FStreamableHandle;
struct FWuwaTeamMemberUiData;

/**
 * HUD 下方中间的当前角色状态（HUD_Main 里的 FightRoleHp，≈ 鸣潮 RoleStateView + ConcertoResponseItem）：元素和协奏圈、特殊能量条、等级、
 * 血条和血量数字，全部跟着当前在场角色走。数值都来自角色 ASC 的属性（经 UWuwaFormationModel / Event.Formation.*）：
 * - 元素：属性 ElementType → UWuwaUISettings::ElementTable 的图标和颜色；图标和协奏圈都染成元素色（原作 ConcertoResponseItem 的做法）；
 * - 协奏圈：ConcertoEnergy / MaxConcertoEnergy → 填充比例；
 * - 特殊能量条：角色 SpecialEnergyBarId → UWuwaUISettings::SpecialEnergyBarTable 的一行：预制体生成在 PnlEnergyBar 下面，
 *   再按行里的 BarClass 包成能量条界面（UWuwaSpecialEnergyBarBase 的子类，读特殊能量属性）；切人时整个换掉；
 * - 等级：属性 Level，前缀（"Lv." 和它的字号）沿用预制体里的富文本；
 * - 血条：按血量比例改宽度（左端固定），数字是"当前/上限"。
 *
 * FightRoleHp 上的 WuwaUiComponentsRegistry：
 *   0 ElementIcon  元素图标（UITexture）
 *   1 EnergyBar       特殊能量条的父节点（UIItem）
 *   2 Level        等级（UIText，富文本）
 *   3 HpFill       血条填充（UITexture）
 *   4 HpText       血量数字（UIText）
 *   5 HpTextMask   血条上面那层同样的数字（UIText，原作用来做压在血条上的另一种颜色）
 *   6 ConcertoBar  协奏圈（UISprite，Filled Radial360）
 *   7 BuffTop      角色专属 buff 栏（原作 PnlBuffTop / RoleUniqueBuffView：CueType 24，比如长离心眼）
 *   8 BuffList     普通 buff 栏（原作 PnlBuffList/PnlBuff / RoleBuffView：CueType 2 / 14 / 37）
 * buff 栏（UWuwaHudBuffBar）：切人时按新角色身上现有的 buff 整体刷新（不播动画），之后跟着 Event.Formation.MemberBuffs 增删（播动画）。
 */
UCLASS()
class WUWA_API UWuwaHudRoleStatePanel : public UWuwaUiPanel
{
	GENERATED_BODY()

public:
	// 测试用
	const UWuwaRoleDataAsset* GetShownRole() const { return ShownRole; }
	int32 GetShownEnergyBarId() const { return ShownEnergyBarId; }
	AActor* GetEnergyBarActor() const { return EnergyBarActor; }
	UWuwaSpecialEnergyBarBase* GetEnergyBar() const { return EnergyBarPanel; }
	float GetDisplayedHealthPercent() const { return DisplayedHealthPercent; }
	float GetDisplayedConcertoPercent() const { return DisplayedConcertoPercent; }
	int32 GetDisplayedLevel() const { return DisplayedLevel; }
	EWuwaElement GetDisplayedElement() const { return DisplayedElement; }
	UWuwaHudBuffBar* GetUniqueBuffBar() const { return UniqueBuffBar; }
	UWuwaHudBuffBar* GetBuffBar() const { return BuffBar; }

protected:
	virtual void OnRegisterComponent() override;
	virtual void OnCreate() override;
	virtual void OnAfterShow() override;
	virtual void OnBeforeHide() override;
	virtual void OnAddEventListener() override;
	virtual void OnBeforeDestroy() override;

private:
	void Refresh();
	void ApplyRole(const UWuwaRoleDataAsset* Role);
	void ApplyAttributes(const FWuwaTeamMemberUiData& Member);
	void ApplyElement(EWuwaElement Element);
	void ApplyEnergyBar(int32 EnergyBarId);
	void ClearEnergyBar();
	void CreateEnergyBar(AActor* Actor, const FWuwaSpecialEnergyBarInfo& Info);
	void ApplyHealth(float Health, float MaxHealth);
	void ApplyBuffs(const TArray<FWuwaBuffUiData>* Buffs, bool bAnimate);

	float HpFullWidth = 0.f;
	UPROPERTY() TObjectPtr<UWuwaHudBuffBar> UniqueBuffBar;
	UPROPERTY() TObjectPtr<UWuwaHudBuffBar> BuffBar;
	/** 等级富文本里数字前面的部分，比如 "<size=12>Lv.</size>"。 */
	FString LevelPrefix;
	float DisplayedHealthPercent = -1.f;
	float DisplayedConcertoPercent = -1.f;
	int32 DisplayedLevel = 0;
	EWuwaElement DisplayedElement = EWuwaElement::None;
	bool bElementShown = false;
	int32 ShownEnergyBarId = 0;
	/** 每次换能量条加一；异步加载回来时对不上说明又切人了，丢掉。 */
	int32 EnergyBarRequest = 0;
	TSharedPtr<FStreamableHandle> EnergyBarLoadHandle;

	UPROPERTY(Transient)
	TObjectPtr<const UWuwaRoleDataAsset> ShownRole;

	UPROPERTY(Transient)
	TObjectPtr<AActor> EnergyBarActor;

	UPROPERTY(Transient)
	TObjectPtr<UWuwaSpecialEnergyBarBase> EnergyBarPanel;
};
