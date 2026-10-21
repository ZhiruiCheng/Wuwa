#include "Game/LGUI/Container/WuwaUiListContainer.h"

#include "Game/LGUI/View/WuwaUiView.h"

void UWuwaUiListContainer::OpenView(UWuwaUiView* View, FWuwaUiDone OnDone)
{
	Views.Add(View);
	TWeakObjectPtr<UWuwaUiListContainer> WeakThis(this);
	TWeakObjectPtr<UWuwaUiView> WeakView(View);
	OpenImplement(View, [WeakThis, WeakView, OnDone](bool bSuccess)
	{
		if (!bSuccess && WeakThis.IsValid())
		{
			WeakThis->Views.Remove(WeakView.Get());
		}
		if (OnDone) { OnDone(bSuccess); }
	});
}

void UWuwaUiListContainer::CloseView(UWuwaUiView* View, FWuwaUiDone OnDone)
{
	Views.Remove(View);
	CloseImplement(View, MoveTemp(OnDone));
}

void UWuwaUiListContainer::ClearContainer()
{
	Views.Empty();
}

void UWuwaUiListContainer::GetViews(TArray<UWuwaUiView*>& OutViews) const
{
	OutViews.Append(Views);
}
