#include "Game/LGUI/Component/WuwaUiSlicedFill.h"

#include "Core/ActorComponent/UIItem.h"
#include "Core/ActorComponent/UISprite.h"
#include "GameFramework/Actor.h"

void UWuwaUiSlicedFill::SetFillAmount(float Amount)
{
	FillAmount = FMath::Clamp(Amount, 0.f, 1.f);
	if (UUIItem* Item = GetOwner() ? Cast<UUIItem>(GetOwner()->GetRootComponent()) : nullptr)
	{
		Item->SetWidth(FullWidth * FillAmount);
	}
}

void UWuwaUiSlicedFill::SetFillAmountOn(UUIItem* Item, float Amount)
{
	if (!Item) return;
	if (UWuwaUiSlicedFill* Fill = Item->GetOwner()->FindComponentByClass<UWuwaUiSlicedFill>())
	{
		Fill->SetFillAmount(Amount);
	}
	else if (UUISprite* Sprite = Cast<UUISprite>(Item))
	{
		Sprite->SetFillAmount(FMath::Clamp(Amount, 0.f, 1.f));
	}
}
