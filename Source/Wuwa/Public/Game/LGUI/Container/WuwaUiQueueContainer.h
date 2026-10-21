#pragma once

#include "CoreMinimal.h"
#include "Game/LGUI/Container/WuwaUiContainer.h"
#include "WuwaUiQueueContainer.generated.h"

/**
 * 优先级队列（≈ 鸣潮 UiViewFloatContainer + FloatViewQueue 的简化版）：Float 层。
 * 同一时刻只显示一个；其余按 Priority 从高到低排队（同优先级先来先显示），当前这个关掉后放出下一个。
 * 鸣潮还按 Area 分区、各区各显示一个，这里整层只有一个区。
 */
UCLASS()
class WUWA_API UWuwaUiQueueContainer : public UWuwaUiContainer
{
	GENERATED_BODY()
public:
	virtual void OpenView(UWuwaUiView* View, FWuwaUiDone OnDone) override;
	virtual void CloseView(UWuwaUiView* View, FWuwaUiDone OnDone) override;
	virtual void ClearContainer() override;
	virtual void GetViews(TArray<UWuwaUiView*>& OutViews) const override;

	UWuwaUiView* GetCurrent() const { return Current; }
	int32 NumWaiting() const { return Waiting.Num(); }

private:
	struct FWaiting
	{
		TWeakObjectPtr<UWuwaUiView> View;
		/** 轮到它真正打开时才回调。 */
		FWuwaUiDone OnDone;
	};

	void ShowNow(UWuwaUiView* View, FWuwaUiDone OnDone);
	void ShowNext();

	UPROPERTY(Transient) TObjectPtr<UWuwaUiView> Current;
	/** 已按优先级排好序；界面对象由 UiManager 持有。 */
	TArray<FWaiting> Waiting;
	/** 同 Stack：切地图前的回调晚到时，不能再放出新世界里的界面。 */
	uint32 Generation = 0;
};
