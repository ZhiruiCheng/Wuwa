#pragma once

#include "CoreMinimal.h"
#include "Game/LGUI/View/WuwaUiPanel.h"
#include "WuwaHudTeamSlotPanel.generated.h"

class UTexture2D;
class UUITexture;
struct FWuwaTeamMemberUiData;

/**
 * HUD 右侧的一个队伍槽（HUD_TeamSlot 子预制体）：头像、血条、数字键、当前角色高亮；点头像切到这个队员。
 * 由 UWuwaMainHudView 用 AddChildByIndex 创建，跟着 HUD 一起显示隐藏。数据只来自 UWuwaFormationModel 和 Event.Formation.* 事件。
 *
 * HUD_TeamSlot 根节点上的 WuwaUiComponentsRegistry：
 *   0 根节点自己（UIButtonComponent，点击切人）
 *   1 Head   头像（UITexture）
 *   2 HpFill 血条填充（UITexture，按血量比例改宽度，左端固定）
 *   3 Number 数字键（UIText；父节点是键帽，当前角色隐藏）
 *   4 Frame  选中圆环（UITexture；父节点是整组选中效果，只在当前角色上显示）
 */
UCLASS()
class WUWA_API UWuwaHudTeamSlotPanel : public UWuwaUiPanel
{
	GENERATED_BODY()

public:
	void SetSlotIndex(int32 InSlotIndex) { SlotIndex = InSlotIndex; }
	int32 GetSlotIndex() const { return SlotIndex; }

	// 测试用
	bool IsSlotVisible() const { return bSlotVisible; }
	bool IsHighlighted() const { return bHighlighted; }
	float GetDisplayedHealthPercent() const { return DisplayedHealthPercent; }

protected:
	virtual void OnRegisterComponent() override;
	virtual void OnCreate() override;
	virtual void OnAfterShow() override;
	virtual void OnAddEventListener() override;

private:
	void Refresh();
	void ApplyMember(const FWuwaTeamMemberUiData* Member);
	void ApplyHealthPercent(float Percent);

	int32 SlotIndex = INDEX_NONE;
	float HpFullWidth = 0.f;
	bool bSlotVisible = true;
	bool bHighlighted = false;
	float DisplayedHealthPercent = -1.f;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ShownHeadIcon;
};
