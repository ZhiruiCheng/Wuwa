#pragma once

#include "CoreMinimal.h"
#include "Game/LGUI/Container/WuwaUiContainer.h"
#include "WuwaUiStackContainer.generated.h"

/**
 * 栈（≈ 鸣潮 UiViewStackContainer）：Normal 层。
 * - 打开：新界面入栈，Create → Start 后，旧栈顶 Hide 和新界面 Show 同时进行（旧的只隐藏不销毁）。
 * - 关闭栈顶：出栈，被关的 Hide → Destroy，同时新栈顶 Show；关闭栈中间的：直接移除销毁，栈顶不变。
 * - 一次操作涉及两个界面，所以容器自己要有锁：进行中再来的打开/关闭进待办列表，解锁后按顺序执行。
 */
UCLASS()
class WUWA_API UWuwaUiStackContainer : public UWuwaUiContainer
{
	GENERATED_BODY()
public:
	virtual void OpenView(UWuwaUiView* View, FWuwaUiDone OnDone) override;
	virtual void CloseView(UWuwaUiView* View, FWuwaUiDone OnDone) override;
	virtual void ClearContainer() override;
	virtual void GetViews(TArray<UWuwaUiView*>& OutViews) const override;

	UWuwaUiView* Peek() const { return Stack.IsEmpty() ? nullptr : Stack.Last().Get(); }
	int32 Num() const { return Stack.Num(); }
	int32 NumPending() const { return Pending.Num(); }

private:
	enum class EOp : uint8 { Open, Close };
	struct FPendingOp
	{
		EOp Op;
		TWeakObjectPtr<UWuwaUiView> View;
		FWuwaUiDone OnDone;
	};

	void RunOpen(UWuwaUiView* View, FWuwaUiDone OnDone);
	void RunClose(UWuwaUiView* View, FWuwaUiDone OnDone);
	/** 进待办列表；"打开 X"还没执行就来了"关闭 X"，两条一起抵消（≈ 鸣潮 UiViewPending.IsPairWith）。 */
	void AddPending(EOp Op, UWuwaUiView* View, FWuwaUiDone OnDone);
	/** 一次操作结束：解锁 → 回调 → 执行下一条待办（和面板 Finish 一样的顺序）。 */
	void FinishOp(uint32 OpGeneration, const FWuwaUiDone& OnDone, bool bSuccess);
	void ProcessPending();

	UPROPERTY(Transient) TArray<TObjectPtr<UWuwaUiView>> Stack;   // 末尾是栈顶
	TArray<FPendingOp> Pending;
	bool bLocked = false;
	/** ClearContainer 时加一：切地图前发出的操作晚到的回调不能再动新世界里的锁。 */
	uint32 Generation = 0;
};
