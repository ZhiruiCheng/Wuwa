#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Game/LGUI/View/WuwaUiView.h"
#include "WuwaLoadingView.generated.h"

struct FStreamableHandle;

/**
 * 加载界面（≈ 鸣潮 LoadingView）：全屏背景图、左下角标题和描述、底部进度条和右侧百分比。
 * 每帧把显示进度平滑地追向 Model 里的实际进度（≈ 鸣潮 LoadingViewBase 的 CurrentProgress += Speed*dt，不超过 NextProgress），
 * 加载完成且显示走到 100% 后，停一下再通知 Controller 关闭。
 *
 * Loading_Prefab 根节点上的 WuwaUiComponentsRegistry：
 *   0 Background  背景图（UITexture，铺满）
 *   1 Title       标题（UIText）
 *   2 Description 描述（UIText）
 *   3 BarFill     进度条填充（UITexture，挂在 BarTrack 下、左端对齐，按进度改宽度）
 *   4 Percent     百分比（UIText）
 *   5 BarTrack    进度条底槽（UIItem，横向铺满，它的宽度就是 100%）
 */
UCLASS()
class WUWA_API UWuwaLoadingView : public UWuwaUiView
{
	GENERATED_BODY()

public:
	// 测试用
	float GetDisplayedProgress() const { return DisplayedProgress; }

	/** 显示进度追赶实际进度：每秒至少走这么多（实际进度停住时也不会一下跳过去）。 */
	float MinSpeedPerSecond = 0.35f;
	/** 落后越多走得越快：每秒追上差距的这个倍数。 */
	float CatchUpRate = 6.f;
	/** 走到 100% 后停留多久再关闭（秒）。 */
	float HoldAtFullSeconds = 0.25f;

protected:
	virtual void OnRegisterComponent() override;
	virtual void OnBeforeShow() override;
	virtual void OnBeforeHide() override;
	virtual void OnBeforeDestroy() override;

private:
	void ApplyShow();
	void ApplyProgress(float Progress);
	bool Tick(float DeltaSeconds);
	void StopTicking();

	float DisplayedProgress = 0.f;
	float HoldTimer = 0.f;
	FTSTicker::FDelegateHandle TickHandle;
	TSharedPtr<FStreamableHandle> BackgroundHandle;
};
