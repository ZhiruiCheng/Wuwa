#include "Game/LGUI/Container/WuwaUiStackContainer.h"

#include "Game/LGUI/View/WuwaUiView.h"

void UWuwaUiStackContainer::OpenView(UWuwaUiView* View, FWuwaUiDone OnDone)
{
	if (bLocked)
	{
		AddPending(EOp::Open, View, MoveTemp(OnDone));
		return;
	}
	RunOpen(View, MoveTemp(OnDone));
}

void UWuwaUiStackContainer::CloseView(UWuwaUiView* View, FWuwaUiDone OnDone)
{
	if (bLocked)
	{
		AddPending(EOp::Close, View, MoveTemp(OnDone));
		return;
	}
	RunClose(View, MoveTemp(OnDone));
}

void UWuwaUiStackContainer::ClearContainer()
{
	Stack.Empty();
	Pending.Empty();
	bLocked = false;
	++Generation;
}

void UWuwaUiStackContainer::GetViews(TArray<UWuwaUiView*>& OutViews) const
{
	for (const TObjectPtr<UWuwaUiView>& View : Stack)
	{
		OutViews.Add(View);
	}
	for (const FPendingOp& Op : Pending)
	{
		if (Op.Op == EOp::Open && Op.View.IsValid())
		{
			OutViews.Add(Op.View.Get());
		}
	}
}

void UWuwaUiStackContainer::RunOpen(UWuwaUiView* View, FWuwaUiDone OnDone)
{
	bLocked = true;
	const uint32 OpGeneration = Generation;
	UWuwaUiView* Previous = Peek();
	Stack.Add(View);

	TWeakObjectPtr<UWuwaUiStackContainer> WeakThis(this);
	TWeakObjectPtr<UWuwaUiView> WeakView(View);
	TWeakObjectPtr<UWuwaUiView> WeakPrevious(Previous);
	View->Prepare([WeakThis, WeakView, WeakPrevious, OpGeneration, OnDone](bool bPrepared)
	{
		UWuwaUiStackContainer* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}

		UWuwaUiView* NewView = WeakView.Get();
		if (!bPrepared || !NewView)
		{
			// 失败：从栈里拿掉，旧栈顶还没动过，保持显示；一定要解锁，不能卡住整层。
			Self->Stack.Remove(NewView);
			if (!NewView)
			{
				Self->FinishOp(OpGeneration, OnDone, false);
				return;
			}
			NewView->Destroy([WeakThis, OpGeneration, OnDone](bool)
			{
				if (WeakThis.IsValid()) { WeakThis->FinishOp(OpGeneration, OnDone, false); }
			});
			return;
		}

		// 旧栈顶 Hide 和新界面 Show 同时进行，都完成才算这次打开结束。
		UWuwaUiView* PreviousView = WeakPrevious.Get();
		const bool bHidePrevious = PreviousView && Self->Stack.Contains(PreviousView);
		TSharedRef<bool> bShown = MakeShared<bool>(false);
		const FWuwaUiDone Join = MakeJoin(bHidePrevious ? 2 : 1, [WeakThis, OpGeneration, OnDone, bShown](bool)
		{
			if (WeakThis.IsValid()) { WeakThis->FinishOp(OpGeneration, OnDone, *bShown); }
		});
		if (bHidePrevious)
		{
			PreviousView->Hide(Join);
		}
		NewView->Show([Join, bShown](bool bSuccess)
		{
			*bShown = bSuccess;
			Join(bSuccess);
		});
	});
}

void UWuwaUiStackContainer::RunClose(UWuwaUiView* View, FWuwaUiDone OnDone)
{
	const int32 Index = Stack.Find(View);
	if (Index == INDEX_NONE)
	{
		// 不在栈里（打开失败已经被移走等）：不影响栈，只保证它被销毁。
		CloseImplement(View, MoveTemp(OnDone));
		return;
	}

	bLocked = true;
	const uint32 OpGeneration = Generation;
	TWeakObjectPtr<UWuwaUiStackContainer> WeakThis(this);
	auto Finish = [WeakThis, OpGeneration, OnDone](bool)
	{
		if (WeakThis.IsValid()) { WeakThis->FinishOp(OpGeneration, OnDone, true); }
	};

	if (Index != Stack.Num() - 1)
	{
		// 栈中间的：直接移除销毁，栈顶不变。
		Stack.RemoveAt(Index);
		CloseImplement(View, Finish);
		return;
	}

	// 栈顶：出栈，被关的 Hide → Destroy，同时新栈顶 Show。
	Stack.Pop();
	UWuwaUiView* NewTop = Peek();
	const FWuwaUiDone Join = MakeJoin(NewTop ? 2 : 1, Finish);
	CloseImplement(View, Join);
	if (NewTop)
	{
		NewTop->Show(Join);
	}
}

void UWuwaUiStackContainer::AddPending(EOp Op, UWuwaUiView* View, FWuwaUiDone OnDone)
{
	if (Op == EOp::Close)
	{
		const int32 OpenIndex = Pending.IndexOfByPredicate([View](const FPendingOp& Pend)
		{
			return Pend.Op == EOp::Open && Pend.View.Get() == View;
		});
		if (OpenIndex != INDEX_NONE)
		{
			// 打开还没执行就要关：两条一起抵消，界面根本不会被创建（Destroy 从 None 开始，不触发任何钩子）。
			const FWuwaUiDone OpenDone = MoveTemp(Pending[OpenIndex].OnDone);
			Pending.RemoveAt(OpenIndex);
			if (OpenDone) { OpenDone(false); }
			View->Destroy([OnDone](bool)
			{
				if (OnDone) { OnDone(true); }
			});
			return;
		}
	}
	Pending.Add({Op, View, MoveTemp(OnDone)});
}

void UWuwaUiStackContainer::FinishOp(uint32 OpGeneration, const FWuwaUiDone& OnDone, bool bSuccess)
{
	// 切地图前发出的操作：锁和待办已经被 ClearContainer 重置，只回调，不碰新的状态。
	const bool bCurrent = OpGeneration == Generation;
	if (bCurrent)
	{
		bLocked = false;
	}
	if (OnDone)
	{
		OnDone(bSuccess);
	}
	if (bCurrent)
	{
		ProcessPending();
	}
}

void UWuwaUiStackContainer::ProcessPending()
{
	while (!bLocked && Pending.Num() > 0)
	{
		FPendingOp Next = MoveTemp(Pending[0]);
		Pending.RemoveAt(0);

		UWuwaUiView* View = Next.View.Get();
		if (!View)
		{
			if (Next.OnDone) { Next.OnDone(false); }
			continue;
		}
		if (Next.Op == EOp::Open)
		{
			RunOpen(View, MoveTemp(Next.OnDone));
		}
		else
		{
			RunClose(View, MoveTemp(Next.OnDone));
		}
	}
}
