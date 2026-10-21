#include "Game/Effect/Specs/Group/WuwaEffectGroupSpec.h"

#include "Game/Effect/System/WuwaEffectSystem.h"
#include "Game/Effect/Types/WuwaEffectTransform.h"
#include "Game/EffectModel/Models/WuwaEffectModelGroup.h"

bool UWuwaEffectGroupSpec::Play_Implementation(const FWuwaEffectSpawnRequest& Request)
{
	Model = Cast<UWuwaEffectModelGroup>(Request.Model);
	if (!Model || !UWuwaEffectSystem::GetEffectSystem(this)) return false;

	// 子项共用整组的挂接目标；变换 = 组的局部变换叠加在请求变换上（挂接时两者都是相对插槽）。
	ChildRequest = Request;
	ChildRequest.Transform = WuwaEffectTransform::Evaluate(Model->Location, Model->Rotation, Model->Scale, 0.f) * Request.Transform;
	PendingChildren.Reset();
	ActiveChildren.Reset();
	Age = 0.f;
	bPlaying = true;

	for (const auto& Entry : Model->EffectData)
	{
		UWuwaEffectModelBase* Child = Entry.Key;
		// 原作不允许 Group 嵌套 Group；这里至少拒绝引用自己，避免无限生成。
		if (!Child || Child == Model) continue;
		if (Entry.Value <= 0.f) SpawnChild(Child);
		else PendingChildren.Add({Child, Entry.Value});
	}
	if (IsFinished()) return false;
	if (PendingChildren.IsEmpty() && ActiveChildren.IsEmpty())
	{
		// 没有任何子项能播放（全部为空或播放失败）。
		bPlaying = false;
		Finish();
		return false;
	}
	return true;
}

void UWuwaEffectGroupSpec::SpawnChild(UWuwaEffectModelBase* ChildModel)
{
	UWuwaEffectSystem* Effects = UWuwaEffectSystem::GetEffectSystem(this);
	if (!Effects) return;
	FWuwaEffectSpawnRequest Request = ChildRequest;
	Request.Model = ChildModel;
	const FWuwaEffectHandle Handle = Effects->SpawnEffect(Request);
	if (Handle.Id.IsValid()) ActiveChildren.Add(Handle);
}

void UWuwaEffectGroupSpec::Tick(float DeltaTime)
{
	Age += FMath::Max(0.f, DeltaTime);
	for (int32 Index = 0; Index < PendingChildren.Num();)
	{
		if (Age < PendingChildren[Index].Delay) { ++Index; continue; }
		UWuwaEffectModelBase* Child = PendingChildren[Index].Model.Get();
		PendingChildren.RemoveAtSwap(Index);
		if (Child) SpawnChild(Child);
	}
	FinishIfDone();
}

void UWuwaEffectGroupSpec::FinishIfDone()
{
	if (const UWuwaEffectSystem* Effects = UWuwaEffectSystem::GetEffectSystem(this))
	{
		ActiveChildren.RemoveAll([Effects](const FWuwaEffectHandle& Handle) { return !Effects->IsEffectActive(Handle); });
	}
	else
	{
		ActiveChildren.Reset();
	}
	if (PendingChildren.IsEmpty() && ActiveChildren.IsEmpty())
	{
		bPlaying = false;
		Finish();
	}
}

void UWuwaEffectGroupSpec::Stop_Implementation(bool bImmediately)
{
	if (IsFinished()) return;
	// 还没生成的子项不再生成；已生成的子项各自按自己的方式收尾。
	PendingChildren.Reset();
	if (UWuwaEffectSystem* Effects = UWuwaEffectSystem::GetEffectSystem(this))
	{
		const TArray<FWuwaEffectHandle> Children = ActiveChildren;
		for (const FWuwaEffectHandle& Handle : Children) Effects->StopEffect(Handle, bImmediately);
	}
	if (bImmediately) ActiveChildren.Reset();
	FinishIfDone();
}

bool UWuwaEffectGroupSpec::IsTickable() const
{
	return bPlaying && !IsFinished();
}

ETickableTickType UWuwaEffectGroupSpec::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

TStatId UWuwaEffectGroupSpec::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWuwaEffectGroupSpec, STATGROUP_Tickables);
}
