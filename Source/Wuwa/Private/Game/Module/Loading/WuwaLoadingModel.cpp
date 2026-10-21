#include "Game/Module/Loading/WuwaLoadingModel.h"

void UWuwaLoadingModel::Begin(const FWuwaLoadingShowRow& InShow)
{
	bLoading = true;
	bFinished = false;
	TargetProgress = 0.f;
	Show = InShow;
}

void UWuwaLoadingModel::SetPhaseProgress(EWuwaLoadingPhase Phase, float Alpha)
{
	if (!bLoading)
	{
		return;
	}
	const float Progress = WuwaLoading::PhaseStart(Phase) + WuwaLoading::PhaseWeight(Phase) * FMath::Clamp(Alpha, 0.f, 1.f);
	TargetProgress = FMath::Clamp(FMath::Max(TargetProgress, Progress), 0.f, 1.f);
}

void UWuwaLoadingModel::Finish()
{
	if (bLoading)
	{
		TargetProgress = 1.f;
		bFinished = true;
	}
}

void UWuwaLoadingModel::Reset()
{
	bLoading = false;
	bFinished = false;
	TargetProgress = 0.f;
	Show = FWuwaLoadingShowRow();
}
