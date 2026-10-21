#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Game/LGUI/Mvc/WuwaControllerBase.h"
#include "Game/Module/Loading/WuwaLoadingTypes.h"
#include "WuwaLoadingController.generated.h"

class APlayerController;

/**
 * 加载界面的业务（≈ 鸣潮 LoadingController：OpenLoadingView / SetProgress / AddProgress / CloseLoadingView），
 * 以及"进入游戏地图"的加载流程（≈ 鸣潮 GameModeController.Load：GameModeOpenLoading → 各阶段推进进度 → OpenBattleViewFromLoading → GameModeCloseLoading）。
 *
 * 进入有队伍的游戏地图时：
 *   1. 打开 LoadingView，进度归零，随机一条展示内容（背景、标题、描述）；
 *   2. Formation 阶段：队伍组件异步加载角色资源，进度 = 加载句柄的真实进度（按已加载的包数算）；
 *   3. SpawnRoles 阶段：队员生成好、控制了第一个角色（客户端：队员复制过来）；
 *   4. MainHud 阶段：打开主 HUD，等它显示出来；
 *   5. 进度设为 100%，LoadingView 把显示进度走满后自己通知这里关闭。
 * 主 HUD 在加载界面下面打开（加载层盖在 HUD 层上），所以关掉加载界面时 HUD 已经就绪。
 */
UCLASS()
class WUWA_API UWuwaLoadingController : public UWuwaControllerBase
{
	GENERATED_BODY()

public:
	static const FName LoadingViewName;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 打开加载界面、进度归零、随机展示内容。已经在加载中则什么都不做。 */
	void BeginLoading();
	/** 某一阶段的段内进度 0~1；总进度只增不减。 */
	void SetPhaseProgress(EWuwaLoadingPhase Phase, float Alpha);
	/** 加载完成：进度设为 100%；界面显示走满后自己关闭。 */
	void FinishLoading();
	/** LoadingView 显示到 100% 后调用：关闭界面、清空状态。 */
	void HandleLoadingViewFinished();
	bool IsLoading() const;

	/**
	 * 加载卡住多久（秒）算超时：这段时间里异步加载队列没有任何变化、流程也没往前走，才关闭加载界面。
	 * 只是慢（编辑器冷启动加载角色要几十秒）不算超时，否则界面关掉时角色还没生成，画面只剩摄像机。
	 */
	float WorldLoadTimeoutSeconds = 60.f;

protected:
	virtual void OnInit() override;
	virtual void OnClear() override;
	virtual void OnLeaveLevel() override;

private:
	void HandlePlayerControllerChanged(APlayerController* NewController);
	void HandleWorldBeginPlay();
	void StartWorldLoad(APlayerController* PlayerController);
	bool TickWorldLoad(float DeltaSeconds);
	void StopWorldLoad();
	void UnbindWorldBeginPlay();
	/**
	 * 加载界面期间让引擎多给异步加载时间（WorldSettings::bHighPriorityLoadingLocal），结束时还原。
	 * 编辑器里同时禁止"切到后台降到 3 帧"（编辑器偏好 Use Less CPU when in Background）：每帧的加载时间是固定的，
	 * 3 帧下冷启动加载角色要好几分钟，加载界面超时关掉后画面里没有角色。
	 */
	void SetHighPriorityLoading(UWorld* World, bool bEnable);
	FWuwaLoadingShowRow PickShow() const;

	FDelegateHandle PlayerControllerChangedHandle;
	TWeakObjectPtr<UWorld> PendingWorld;
	TWeakObjectPtr<APlayerController> PendingController;
	FDelegateHandle WorldBeginPlayHandle;

	TWeakObjectPtr<APlayerController> LoadingPlayer;
	FTSTicker::FDelegateHandle WorldLoadTicker;
	double WorldLoadStartTime = 0.0;
	/** 最近一次看到进展（异步加载包数变化、流程推进）的时间，用来判断卡住。 */
	double LastProgressTime = 0.0;
	int32 LastAsyncPackages = -1;
	/** 这次加载期间异步队列里最多有多少个包，用来把包数换算成进度。 */
	int32 PeakAsyncPackages = 0;
	float LastPhaseAlpha = -1.f;
	TWeakObjectPtr<UWorld> HighPriorityWorld;
	bool bPreviousHighPriorityLoading = false;
#if WITH_EDITOR
	FDelegateHandle EditorThrottleHandle;
#endif
};
