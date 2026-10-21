#pragma once

#include "CoreMinimal.h"
#include "Game/LGUI/Mvc/WuwaModelBase.h"
#include "Game/Module/Loading/WuwaLoadingTypes.h"
#include "WuwaLoadingModel.generated.h"

/**
 * 加载数据（≈ 鸣潮 LoadingModel：CurrentProgress / NextProgress）。
 * Controller 写"实际加载到哪了"（TargetProgress，只增不减）；LoadingView 每帧把显示进度平滑地追过去，
 * 所以进度条不会因为某一段瞬间完成就一下跳过去。
 */
UCLASS()
class WUWA_API UWuwaLoadingModel : public UWuwaModelBase
{
	GENERATED_BODY()

public:
	bool IsLoading() const { return bLoading; }
	bool IsFinished() const { return bFinished; }
	/** 实际加载进度 0~1。 */
	float GetTargetProgress() const { return TargetProgress; }
	const FWuwaLoadingShowRow& GetShow() const { return Show; }

	// —— 只给 Controller 用 ——
	void Begin(const FWuwaLoadingShowRow& InShow);
	/** 某一阶段的段内进度 0~1；总进度只增不减。 */
	void SetPhaseProgress(EWuwaLoadingPhase Phase, float Alpha);
	void Finish();
	void Reset();

protected:
	virtual void OnClear() override { Reset(); }
	virtual void OnLeaveLevel() override { Reset(); }

private:
	bool bLoading = false;
	bool bFinished = false;
	float TargetProgress = 0.f;
	FWuwaLoadingShowRow Show;
};
