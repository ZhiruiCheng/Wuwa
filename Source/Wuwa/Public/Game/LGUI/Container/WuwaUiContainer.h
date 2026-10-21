#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Game/LGUI/View/WuwaUiState.h"
#include "WuwaUiContainer.generated.h"

class UWuwaUiView;

/**
 * 一层界面的进出规则（≈ 鸣潮 UiViewContainer）。UiManager 只负责创建界面对象，打开/关闭都交给所在层的容器。
 *
 * 回调约定：
 * - OpenView 的 OnDone(true)：界面已经显示；OnDone(false)：没打开成功，界面已经从容器移除并销毁完毕。
 * - CloseView 的 OnDone：界面已经销毁完毕。
 */
UCLASS(Abstract)
class WUWA_API UWuwaUiContainer : public UObject
{
	GENERATED_BODY() 
public:
	virtual void OpenView(UWuwaUiView* View, FWuwaUiDone OnDone) PURE_VIRTUAL(UWuwaUiContainer::OpenView, );
	virtual void CloseView(UWuwaUiView* View, FWuwaUiDone OnDone) PURE_VIRTUAL(UWuwaUiContainer::CloseView, );
	/** 切地图时调用：Actor 已经随旧世界销毁了，这里只丢掉记录，不走关闭流程。 */
	virtual void ClearContainer() PURE_VIRTUAL(UWuwaUiContainer::ClearContainer, );
	/** 容器里现有的界面（含排队等待的），调试和测试用。 */
	virtual void GetViews(TArray<UWuwaUiView*>& OutViews) const PURE_VIRTUAL(UWuwaUiContainer::GetViews, );

protected:
	/** Create → Start → Show（≈ OpenViewImplementAsync）；失败时先把界面销毁完再回调 false。 */
	static void OpenImplement(UWuwaUiView* View, FWuwaUiDone OnDone);
	/** 还没创建就直接 Destroy，否则 Hide → Destroy（≈ CloseViewImplementAsync）。 */
	static void CloseImplement(UWuwaUiView* View, FWuwaUiDone OnDone);
	/** 返回一个要被调用 Count 次的回调，全部调用完后执行 OnAll（参数：是否全部成功）。Count 必须大于 0。 */
	static FWuwaUiDone MakeJoin(int32 Count, TFunction<void(bool bAllSucceeded)> OnAll);
};
