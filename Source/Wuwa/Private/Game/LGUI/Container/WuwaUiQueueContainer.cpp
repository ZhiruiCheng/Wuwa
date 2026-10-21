#include "Game/LGUI/Container/WuwaUiQueueContainer.h"

#include "Game/LGUI/View/WuwaUiView.h"

void UWuwaUiQueueContainer::OpenView(UWuwaUiView* View, FWuwaUiDone OnDone)
{
	if (!Current)
	{
		ShowNow(View, MoveTemp(OnDone));
		return;
	}

	// 插到第一个优先级比它低的前面：高的先显示，同优先级保持先来后到。
	const int32 Priority = View->GetPriority();
	const int32 InsertAt = Waiting.IndexOfByPredicate([Priority](const FWaiting& Item)
	{
		return Item.View.IsValid() && Item.View->GetPriority() < Priority;
	});
	FWaiting Item{View, MoveTemp(OnDone)};
	if (InsertAt == INDEX_NONE)
	{
		Waiting.Add(MoveTemp(Item));
	}
	else
	{
		Waiting.Insert(MoveTemp(Item), InsertAt);
	}
}

void UWuwaUiQueueContainer::CloseView(UWuwaUiView* View, FWuwaUiDone OnDone)
{
	if (View == Current)
	{
		// 先关完当前这个，再放出下一个（和鸣潮一样，不叠在一起显示）。
		Current = nullptr;
		const uint32 OpGeneration = Generation;
		TWeakObjectPtr<UWuwaUiQueueContainer> WeakThis(this);
		CloseImplement(View, [WeakThis, OpGeneration, OnDone](bool bSuccess)
		{
			if (OnDone) { OnDone(bSuccess); }
			if (WeakThis.IsValid() && WeakThis->Generation == OpGeneration)
			{
				WeakThis->ShowNext();
			}
		});
		return;
	}

	const int32 Index = Waiting.IndexOfByPredicate([View](const FWaiting& Item) { return Item.View.Get() == View; });
	if (Index != INDEX_NONE)
	{
		// 还在排队：没创建过，告诉打开方"没打开成功"，再把对象销毁掉（从 None 开始，不触发钩子）。
		const FWuwaUiDone OpenDone = MoveTemp(Waiting[Index].OnDone);
		Waiting.RemoveAt(Index);
		if (OpenDone) { OpenDone(false); }
	}
	CloseImplement(View, MoveTemp(OnDone));
}

void UWuwaUiQueueContainer::ClearContainer()
{
	Current = nullptr;
	Waiting.Empty();
	++Generation;
}

void UWuwaUiQueueContainer::GetViews(TArray<UWuwaUiView*>& OutViews) const
{
	if (Current)
	{
		OutViews.Add(Current);
	}
	for (const FWaiting& Item : Waiting)
	{
		if (Item.View.IsValid())
		{
			OutViews.Add(Item.View.Get());
		}
	}
}

void UWuwaUiQueueContainer::ShowNow(UWuwaUiView* View, FWuwaUiDone OnDone)
{
	Current = View;
	const uint32 OpGeneration = Generation;
	TWeakObjectPtr<UWuwaUiQueueContainer> WeakThis(this);
	TWeakObjectPtr<UWuwaUiView> WeakView(View);
	OpenImplement(View, [WeakThis, WeakView, OpGeneration, OnDone](bool bSuccess)
	{
		if (OnDone) { OnDone(bSuccess); }
		// 没打开成功就让出位置给下一个。
		UWuwaUiQueueContainer* Self = WeakThis.Get();
		if (!bSuccess && Self && Self->Generation == OpGeneration && Self->Current == WeakView.Get())
		{
			Self->Current = nullptr;
			Self->ShowNext();
		}
	});
}

void UWuwaUiQueueContainer::ShowNext()
{
	while (!Current && Waiting.Num() > 0)
	{
		FWaiting Next = MoveTemp(Waiting[0]);
		Waiting.RemoveAt(0);
		if (UWuwaUiView* View = Next.View.Get())
		{
			ShowNow(View, MoveTemp(Next.OnDone));
		}
		else if (Next.OnDone)
		{
			Next.OnDone(false);
		}
	}
}
