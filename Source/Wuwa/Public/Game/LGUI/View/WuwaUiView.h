#pragma once

#include "CoreMinimal.h"
#include "Game/LGUI/Core/WuwaUISettings.h"
#include "Game/LGUI/Layer/WuwaUiLayer.h"
#include "Game/LGUI/View/WuwaUiPanel.h"
#include "WuwaUiView.generated.h"

class AActor;
class UAudioComponent;
class UWuwaUiManager;

/**
 * 顶层界面（≈ 鸣潮 UiViewBase）：由 UiManager 创建，交给所在层的容器打开和关闭。
 * 配置表里的打开效果在这里统一处理（业务子类不用管）：
 * - 音效：第一次显示时播 OpenSound，关闭时播 CloseSound；
 * - 时间缩放、UI 相机：从第一次显示到销毁期间生效；
 * - 背景模糊：显示期间生效（被栈上层盖住时撤掉）；
 * - 全屏：完全显示后隐藏下面各层，开始关闭时恢复。
 */
UCLASS(BlueprintType)
class WUWA_API UWuwaUiView : public UWuwaUiPanel
{
	GENERATED_BODY()
public:
	/** 记下打开需要的一切，容器拿到界面对象就能直接打开它。 */
	void Init(FName InViewName, int32 InViewId, const FWuwaUiViewConfig& InConfig, UUIItem* InLayerItem);
	/** Create → Start → Show 一起排进队列；任一步失败就回调 false（后面的步骤会被丢弃）。 */
	void Open(FWuwaUiDone OnDone = nullptr);
	/** 只 Create → Start，不 Show：Stack 容器要自己决定什么时候 Show（和旧栈顶 Hide 同时进行）。 */
	void Prepare(FWuwaUiDone OnDone = nullptr);
	/** 还没创建（None）就直接 Destroy；否则 Hide → Destroy。 */
	void Close(FWuwaUiDone OnDone = nullptr);
	/** 业务代码里关自己：交给 UiManager，由容器关闭（Stack 关掉栈顶后要把下面的界面显示回来）。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|UI")
	void CloseMe();

	UFUNCTION(BlueprintPure, Category = "Wuwa|UI")
	FName GetViewName() const { return ViewName; }
	UFUNCTION(BlueprintPure, Category = "Wuwa|UI")
	int32 GetViewId() const { return ViewId; }
	const FWuwaUiViewConfig& GetConfig() const { return Config; }
	EWuwaUiLayer GetLayer() const { return Config.Layer; }
	int32 GetPriority() const { return Config.Priority; }
	bool IsPermanent() const { return Config.bIsPermanent; }

	/** UiManager 收到关闭请求时标记；之后 IsViewOpen 不再算它，也不会被重复关闭。 */
	void MarkCloseRequested() { bCloseRequested = true; }
	bool IsCloseRequested() const { return bCloseRequested; }

	// 调试、测试用
	AActor* GetBlurActor() const { return BlurActor.Get(); }
	UAudioComponent* GetLastPlayedSound() const { return LastPlayedSound.Get(); }

protected:
	virtual void OnShowStartImplement() override;
	virtual void OnAfterShowImplement() override;
	virtual void OnHideStartImplement() override;
	virtual void OnAfterHideImplement() override;
	virtual void OnBeforeDestroyImplement() override;

private:
	UWuwaUiManager* GetManager() const;
	void PlayUiSound(const TSoftObjectPtr<USoundBase>& Sound);
	void ShowBlur();
	void HideBlur();
	void SetFullScreenActive(bool bActive);

	FName ViewName;
	int32 ViewId = 0;
	FWuwaUiViewConfig Config;
	TWeakObjectPtr<UUIItem> LayerItem;
	bool bCloseRequested = false;
	/** 第一次显示时生效、销毁时撤销的效果（打开音效、时间缩放、UI 相机）是否已经生效。 */
	bool bOpenEffectsApplied = false;
	bool bFullScreenActive = false;
	TWeakObjectPtr<AActor> BlurActor;
	TWeakObjectPtr<UAudioComponent> LastPlayedSound;
};
