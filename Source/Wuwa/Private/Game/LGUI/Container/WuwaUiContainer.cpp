#include "Game/LGUI/Container/WuwaUiContainer.h"

#include "Game/LGUI/View/WuwaUiView.h"

void UWuwaUiContainer::OpenImplement(UWuwaUiView* View, FWuwaUiDone OnDone)
{
	TWeakObjectPtr<UWuwaUiView> WeakView(View);
	View->Open([WeakView, OnDone](bool bSuccess)
	{
		if (bSuccess || !WeakView.IsValid())
		{
			if (OnDone) { OnDone(bSuccess); }
			return;
		}
		// 没打开成功：把已经生成的部分清理掉，再告诉调用方。
		WeakView->Destroy([OnDone](bool)
		{
			if (OnDone) { OnDone(false); }
		});
	});
}

void UWuwaUiContainer::CloseImplement(UWuwaUiView* View, FWuwaUiDone OnDone)
{
	View->Close(MoveTemp(OnDone));
}

FWuwaUiDone UWuwaUiContainer::MakeJoin(int32 Count, TFunction<void(bool bAllSucceeded)> OnAll)
{
	check(Count > 0);
	TSharedRef<int32> Remaining = MakeShared<int32>(Count);
	TSharedRef<bool> bAllSucceeded = MakeShared<bool>(true);
	TSharedRef<TFunction<void(bool)>> SharedOnAll = MakeShared<TFunction<void(bool)>>(MoveTemp(OnAll));
	return [Remaining, bAllSucceeded, SharedOnAll](bool bSuccess)
	{
		*bAllSucceeded &= bSuccess;
		if (--(*Remaining) == 0)
		{
			(*SharedOnAll)(*bAllSucceeded);
		}
	};
}
