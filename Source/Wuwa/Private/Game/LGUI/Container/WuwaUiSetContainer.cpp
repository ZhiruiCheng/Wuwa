#include "Game/LGUI/Container/WuwaUiSetContainer.h"

#include "Game/LGUI/Core/WuwaUiLog.h"
#include "Game/LGUI/View/WuwaUiView.h"

void UWuwaUiSetContainer::OpenView(UWuwaUiView* View, FWuwaUiDone OnDone)
{
	const FName Name = View->GetViewName();
	if (Views.Contains(Name))
	{
		UE_LOG(LogWuwaUI, Error, TEXT("Set container: view '%s' is already open (duplicate)."), *Name.ToString());
		View->Destroy([OnDone](bool)
		{
			if (OnDone) { OnDone(false); }
		});
		return;
	}

	Views.Add(Name, View);
	TWeakObjectPtr<UWuwaUiSetContainer> WeakThis(this);
	TWeakObjectPtr<UWuwaUiView> WeakView(View);
	OpenImplement(View, [WeakThis, WeakView, OnDone](bool bSuccess)
	{
		if (!bSuccess && WeakThis.IsValid())
		{
			WeakThis->RemoveIfSame(WeakView.Get());
		}
		if (OnDone) { OnDone(bSuccess); }
	});
}

void UWuwaUiSetContainer::CloseView(UWuwaUiView* View, FWuwaUiDone OnDone)
{
	RemoveIfSame(View);
	CloseImplement(View, MoveTemp(OnDone));
}

void UWuwaUiSetContainer::ClearContainer()
{
	Views.Empty();
}

void UWuwaUiSetContainer::GetViews(TArray<UWuwaUiView*>& OutViews) const
{
	for (const TPair<FName, TObjectPtr<UWuwaUiView>>& Pair : Views)
	{
		OutViews.Add(Pair.Value);
	}
}

void UWuwaUiSetContainer::RemoveIfSame(UWuwaUiView* View)
{
	if (View && Views.FindRef(View->GetViewName()) == View)
	{
		Views.Remove(View->GetViewName());
	}
}
