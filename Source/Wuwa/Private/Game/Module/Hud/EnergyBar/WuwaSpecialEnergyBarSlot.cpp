#include "Game/Module/Hud/EnergyBar/WuwaSpecialEnergyBarSlot.h"

#include "Core/ActorComponent/UIBaseRenderable.h"
#include "Core/ActorComponent/UITexture.h"

namespace WuwaSpecialEnergySlotItem
{
	constexpr int32 Bg = 0;
	constexpr int32 BarPart = 1;
	constexpr int32 Graduate = 2;
	constexpr int32 GraduateFront = 3;

	/** 把拉伸的节点换成固定锚点、轴心挪到左端（大小位置不变），返回满宽度；之后改宽度只从右边收。 */
	float PinLeft(UUIItem* Item)
	{
		if (!Item) return 0.f;
		Item->SetHorizontalAnchorMinMax(FVector2D(Item->GetAnchorMin().X), true, true);
		const float Width = Item->GetWidth();
		const FVector2D Pivot = Item->GetPivot();
		FVector2D Position = Item->GetAnchoredPosition();
		Position.X -= Width * Pivot.X;
		Item->SetPivot(FVector2D(0.f, Pivot.Y));
		Item->SetAnchoredPosition(Position);
		return Width;
	}
}

void UWuwaSpecialEnergyBarSlotItem::OnRegisterComponent()
{
	using namespace WuwaSpecialEnergySlotItem;
	ComponentRegisterInfos = {
		{Bg, UUITexture::StaticClass()},
		{BarPart, UUITexture::StaticClass()},
		{Graduate, UUITexture::StaticClass()},
		{GraduateFront, UUITexture::StaticClass()},
	};
}

void UWuwaSpecialEnergyBarSlotItem::OnCreate()
{
	using namespace WuwaSpecialEnergySlotItem;
	BarFullWidth = PinLeft(GetComponentAt<UUITexture>(BarPart));
	FrontFullWidth = PinLeft(GetComponentAt<UUITexture>(GraduateFront));
}

void UWuwaSpecialEnergyBarSlotItem::SetBarColor(const FColor& Color)
{
	if (UUITexture* Bar = GetComponentAt<UUITexture>(WuwaSpecialEnergySlotItem::BarPart)) Bar->SetColor(Color);
}

void UWuwaSpecialEnergyBarSlotItem::SetPointColor(const FColor& Color)
{
	if (UUIBaseRenderable* Front = GetComponentAt<UUIBaseRenderable>(WuwaSpecialEnergySlotItem::GraduateFront)) Front->SetColor(Color);
}

void UWuwaSpecialEnergyBarSlotItem::SetFill(float Percent)
{
	using namespace WuwaSpecialEnergySlotItem;
	ShownFill = Percent;
	if (UUITexture* Bar = GetComponentAt<UUITexture>(BarPart); Bar && BarFullWidth > 0.f)
	{
		Bar->SetWidth(BarFullWidth * Percent);
	}
	// 前景刻度：露出左边 Percent 那么宽，UV 也只取左边那部分（原作改右边距 + 平铺数，刻度不会被压扁；
	// 导出时刻度已经按原作格宽排成一条，裁 UV 等于少画几格）
	if (UUITexture* Front = GetComponentAt<UUITexture>(GraduateFront); Front && FrontFullWidth > 0.f)
	{
		Front->SetWidth(FrontFullWidth * Percent);
		Front->SetUVRect(FVector4(0.f, 0.f, FMath::Max(Percent, KINDA_SMALL_NUMBER), 1.f));
	}
}

void UWuwaSpecialEnergyBarSlotItem::UpdatePercent(float Percent, bool bKeyEnable)
{
	using namespace WuwaSpecialEnergySlotItem;
	int32 NewState = 0;
	if (Percent <= KINDA_SMALL_NUMBER) NewState = -1;
	else if (Percent >= 1.f - KINDA_SMALL_NUMBER && bKeyEnable) NewState = 1;

	if (NewState != State)
	{
		State = NewState;
		auto SetActive = [this](int32 Index, bool bActive)
		{
			if (UUIItem* Item = GetComponentAt<UUIItem>(Index)) Item->SetIsUIActive(bActive);
		};
		// 和原作 UpdatePercent 的三种状态一致；满的时候原作是 Niagara，这里用满的填充层代替
		SetActive(Bg, NewState != 1);
		SetActive(Graduate, NewState != 1);
		SetActive(GraduateFront, NewState == 0);
		SetActive(BarPart, NewState != -1);
	}
	SetFill(NewState == 1 ? 1.f : FMath::Clamp(Percent, 0.f, 1.f));
}

bool UWuwaSpecialEnergyBarSlot::HasBottomLine() const
{
	return !bForceHideBottomLine;
}

void UWuwaSpecialEnergyBarSlot::OnRegisterComponent()
{
	for (int32 Index = 0; Index < Config.SlotNum; ++Index)
	{
		ComponentRegisterInfos.Add({Index, UUIItem::StaticClass()});
	}
	if (HasBottomLine())
	{
		ComponentRegisterInfos.Add({Config.SlotNum, UUIItem::StaticClass()});
	}
}

void UWuwaSpecialEnergyBarSlot::OnCreate()
{
	SlotItems.Reset();
	for (int32 Index = 0; Index < Config.SlotNum; ++Index)
	{
		if (UWuwaSpecialEnergyBarSlotItem* Item = AddChildByIndex<UWuwaSpecialEnergyBarSlotItem>(Index))
		{
			SlotItems.Add(Item);
		}
	}
}

void UWuwaSpecialEnergyBarSlot::OnStart()
{
	if (!bOverrideColor)
	{
		// 表里的颜色是 RRGGBB，FColor::FromHex 补 FF 透明度（原作 UE.Color.FromHex 同样）
		for (UWuwaSpecialEnergyBarSlotItem* Item : SlotItems)
		{
			Item->SetBarColor(Config.EffectColor);
			Item->SetPointColor(Config.PointColor);
		}
	}
	// 原作：(ForceHideBottomLine || IsMorph && 有外框贴图) 时隐藏底线
	if (HasBottomLine())
	{
		if (UUIItem* Line = GetItem(Config.SlotNum))
		{
			Line->SetIsUIActive(!(bIsMorph && !Config.Texture.IsNull()));
		}
	}
}

void UWuwaSpecialEnergyBarSlot::RefreshBarPercent()
{
	const float Percent = PercentMachine.GetCurrent();
	const bool bKeyEnable = GetKeyEnable();
	for (int32 Index = 0; Index < SlotItems.Num(); ++Index)
	{
		SlotItems[Index]->UpdatePercent(Percent * Config.SlotNum - Index, bKeyEnable);
	}
}
