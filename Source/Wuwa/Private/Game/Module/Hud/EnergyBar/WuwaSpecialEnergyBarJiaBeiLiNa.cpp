#include "Game/Module/Hud/EnergyBar/WuwaSpecialEnergyBarJiaBeiLiNa.h"

#include "Core/ActorComponent/UIItem.h"
#include "Game/Module/Formation/WuwaFormationEvents.h"

namespace WuwaSpecialEnergyJiaBeiLiNa
{
	constexpr int32 RedSlot = 0;
	constexpr int32 MainSlot = 1;
	// 原作 GetRedTargetAttributePercent 写死读特殊能量 2
	constexpr int32 RedSpecialEnergy = 2;
	// 变身弹药（原作变身条 120801 的 AttributeId 66）
	constexpr int32 AmmoSpecialEnergy = 3;
}

UWuwaSpecialEnergyBarJiaBeiLiNaSlot::UWuwaSpecialEnergyBarJiaBeiLiNaSlot()
{
	bForceHideBottomLine = true;
	bOverrideColor = true;
}

void UWuwaSpecialEnergyBarJiaBeiLiNaSlot::OnMemberData(const FWuwaTeamMemberUiData& Member, bool bInitial)
{
	const float Red = Member.GetSpecialEnergyPercent(WuwaSpecialEnergyJiaBeiLiNa::RedSpecialEnergy);
	if (bInitial)
	{
		RedPercentMachine.Init(Red);
	}
	else
	{
		RedPercentMachine.SetTarget(Red);
	}
	Super::OnMemberData(Member, bInitial);
}

float UWuwaSpecialEnergyBarJiaBeiLiNaSlot::GetTargetPercent(const FWuwaTeamMemberUiData& Member) const
{
	const float Ammo = Member.GetSpecialEnergyPercent(WuwaSpecialEnergyJiaBeiLiNa::AmmoSpecialEnergy);
	return Ammo > 0.f ? Ammo : Super::GetTargetPercent(Member);
}

void UWuwaSpecialEnergyBarJiaBeiLiNaSlot::TickBar(float DeltaSeconds)
{
	Super::TickBar(DeltaSeconds);
	if (RedPercentMachine.Update(DeltaSeconds))
	{
		RefreshBarPercent();
	}
}

void UWuwaSpecialEnergyBarJiaBeiLiNaSlot::RefreshBarPercent()
{
	using namespace WuwaSpecialEnergyJiaBeiLiNa;
	const bool bKeyEnable = GetKeyEnable();
	if (SlotItems.IsValidIndex(RedSlot)) SlotItems[RedSlot]->UpdatePercent(RedPercentMachine.GetCurrent(), bKeyEnable);
	if (SlotItems.IsValidIndex(MainSlot)) SlotItems[MainSlot]->UpdatePercent(PercentMachine.GetCurrent(), bKeyEnable);
}

void UWuwaSpecialEnergyBarJiaBeiLiNa::OnRegisterComponent()
{
	ComponentRegisterInfos = {{0, UUIItem::StaticClass()}};
}

void UWuwaSpecialEnergyBarJiaBeiLiNa::OnCreate()
{
	AActor* BarActor = GetRegisteredActor(0);
	if (!BarActor)
	{
		return;
	}
	BarItem = NewObject<UWuwaSpecialEnergyBarJiaBeiLiNaSlot>(this);
	BarItem->InitData(MemberIndex, Config);
	BarItem->CreateByActor(BarActor);
	AddChild(BarItem);
}

void UWuwaSpecialEnergyBarJiaBeiLiNa::SetMemberIndex(int32 InMemberIndex)
{
	Super::SetMemberIndex(InMemberIndex);
	if (BarItem) BarItem->SetMemberIndex(InMemberIndex);
}
