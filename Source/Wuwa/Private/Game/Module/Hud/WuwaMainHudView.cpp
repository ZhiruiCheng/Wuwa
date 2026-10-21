#include "Game/Module/Hud/WuwaMainHudView.h"

#include "Core/ActorComponent/UIItem.h"
#include "Game/Module/BattleUi/WuwaBossStatePanel.h"
#include "Game/Module/Hud/WuwaHudRoleStatePanel.h"
#include "Game/Module/Hud/WuwaHudSkillBarPanel.h"
#include "Game/Module/Hud/WuwaHudTeamSlotPanel.h"

void UWuwaMainHudView::OnRegisterComponent()
{
	for (int32 Index = 0; Index < TeamSlotCount; ++Index)
	{
		ComponentRegisterInfos.Add({Index, UUIItem::StaticClass()});
	}
	ComponentRegisterInfos.Add({SkillBarIndex, UUIItem::StaticClass()});
	ComponentRegisterInfos.Add({RoleStateIndex, UUIItem::StaticClass()});
}

void UWuwaMainHudView::OnCreate()
{
	TeamSlots.Reset();
	for (int32 Index = 0; Index < TeamSlotCount; ++Index)
	{
		if (UWuwaHudTeamSlotPanel* TeamSlot = AddChildByIndex<UWuwaHudTeamSlotPanel>(Index))
		{
			TeamSlot->SetSlotIndex(Index);
			TeamSlots.Add(TeamSlot);
		}
	}
	SkillBar = AddChildByIndex<UWuwaHudSkillBarPanel>(SkillBarIndex);
	RoleState = AddChildByIndex<UWuwaHudRoleStatePanel>(RoleStateIndex);
	BossState = NewObject<UWuwaBossStatePanel>(this);
	BossState->Initialize(GetRootItem());
}

void UWuwaMainHudView::OnBeforeDestroy()
{
	if (BossState)
	{
		BossState->Shutdown();
		BossState = nullptr;
	}
	Super::OnBeforeDestroy();
}
