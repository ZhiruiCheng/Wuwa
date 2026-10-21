#include "Game/Module/Hud/EnergyBar/WuwaSpecialEnergyBarMorph.h"

#include "Core/ActorComponent/UITexture.h"
#include "Engine/Texture2D.h"
#include "Game/Module/Hud/EnergyBar/WuwaSpecialEnergyBarSlot.h"

namespace WuwaSpecialEnergyMorph
{
	constexpr int32 Frame = 0;
	constexpr int32 BarSlot = 1;
}

void UWuwaSpecialEnergyBarMorph::OnRegisterComponent()
{
	using namespace WuwaSpecialEnergyMorph;
	ComponentRegisterInfos = {
		{Frame, UUITexture::StaticClass()},
		{BarSlot, UUIItem::StaticClass()},
	};
}

void UWuwaSpecialEnergyBarMorph::OnCreate()
{
	// 里面的格子条用同一行配置（同一对属性、同样的格数），标成 IsMorph（原作 InitBarItem）
	AActor* BarActor = GetRegisteredActor(WuwaSpecialEnergyMorph::BarSlot);
	if (!BarActor || Config.Type != EWuwaSpecialEnergyBarType::Morph)
	{
		return;
	}
	BarItem = NewObject<UWuwaSpecialEnergyBarSlot>(this);
	BarItem->bIsMorph = true;
	BarItem->InitData(MemberIndex, Config);
	BarItem->CreateByActor(BarActor);
	AddChild(BarItem);
}

void UWuwaSpecialEnergyBarMorph::RefreshIcon()
{
	const bool bEnable = GetKeyEnable() && !Config.EnableTexture.IsNull();
	if (bIconShown && bEnable == bShowingEnable)
	{
		return;
	}
	UUITexture* Frame = GetComponentAt<UUITexture>(WuwaSpecialEnergyMorph::Frame);
	UTexture2D* Texture = (bEnable ? Config.EnableTexture : Config.Texture).LoadSynchronous();
	if (Frame && Texture)
	{
		Frame->SetTexture(Texture);
	}
	bShowingEnable = bEnable;
	bIconShown = true;
}

void UWuwaSpecialEnergyBarMorph::SetMemberIndex(int32 InMemberIndex)
{
	Super::SetMemberIndex(InMemberIndex);
	if (BarItem) BarItem->SetMemberIndex(InMemberIndex);
}
