#include "Game/Effect/Specs/Niagara/WuwaEffectNiagaraSpec.h"

#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Game/Effect/Types/WuwaEffectSpawnRequest.h"
#include "Game/Effect/Types/WuwaEffectTransform.h"
#include "Game/EffectModel/Models/WuwaEffectModelNiagara.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaEffectNiagara, Log, All);

bool UWuwaEffectNiagaraSpec::Play_Implementation(const FWuwaEffectSpawnRequest& Request)
{
	Model = Cast<UWuwaEffectModelNiagara>(Request.Model);
	UWorld* World = GetWorld();
	if (!Model || !World)
	{
		return false;
	}
	if (!Model->NiagaraRef)
	{
		UE_LOG(LogWuwaEffectNiagara, Warning, TEXT("%s: no Niagara system is configured."), *Model->GetName());
		return false;
	}

	if (!IsValid(NiagaraComponent))
	{
		NiagaraComponent = NewObject<UNiagaraComponent>(this);
		NiagaraComponent->SetAutoActivate(false);
		NiagaraComponent->SetAutoDestroy(false);
		NiagaraComponent->RegisterComponentWithWorld(World);
	}
	UNiagaraComponent* Component = NiagaraComponent;
	Component->SetAsset(Model->NiagaraRef);
	Component->SetCastShadow(Model->bCastShadow);
	Component->SetReceivesDecals(Model->ReceiveDecal);
	Component->SetTranslucentSortPriority(Model->TranslucencySortPriority);

	BaseTransform = Request.Transform;
	bAttached = false;
	if (USceneComponent* AttachTo = Request.AttachTo.Get())
	{
		Component->AttachToComponent(AttachTo, FAttachmentTransformRules::KeepRelativeTransform, Request.SocketName);
		bAttached = true;
	}
	else
	{
		Component->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}

	Age = 0.f;
	bStopping = false;
	bAnimated = WuwaEffectTransform::IsAnimated(Model->Location, Model->Rotation, Model->Scale);
	for (const auto& Parameter : Model->FloatParameters) bAnimated |= Parameter.Value.bUseCurve;
	for (const auto& Parameter : Model->VectorParameters) bAnimated |= Parameter.Value.bUseCurve;
	for (const auto& Parameter : Model->ColorParameters) bAnimated |= Parameter.Value.bUseCurve;
	ApplyStateAtAge();

	// 先绑定再激活：系统可能在激活过程中同步结束。
	Component->OnSystemFinished.AddUniqueDynamic(this, &ThisClass::HandleSystemFinished);
	bPlaying = true;
	Component->Activate(true);
	if (IsFinished())
	{
		return false;
	}
	if (!Component->IsActive())
	{
		Stop_Implementation(true);
		return false;
	}
	return true;
}

void UWuwaEffectNiagaraSpec::Stop_Implementation(bool bImmediately)
{
	if (IsFinished()) return;
	UNiagaraComponent* Component = NiagaraComponent;
	if (!IsValid(Component))
	{
		NiagaraComponent = nullptr;
		bPlaying = false;
		Finish();
		return;
	}

	// 原作 OnPreStop：Deactivate 只停止发射，已有粒子自然播完后才算结束。
	if (!bImmediately && Model && Model->DeactivateOnStop && Component->IsActive())
	{
		if (!bStopping)
		{
			bStopping = true;
			Component->Deactivate();
		}
		return;
	}

	// DeactivateImmediate 会同步广播结束；先解绑，Finish 之后由池负责清理。
	Component->OnSystemFinished.RemoveAll(this);
	Component->DeactivateImmediate();
	bPlaying = false;
	Finish();
}

void UWuwaEffectNiagaraSpec::HandleSystemFinished(UNiagaraComponent* Component)
{
	if (Component != NiagaraComponent) return;
	NiagaraComponent->OnSystemFinished.RemoveAll(this);
	bPlaying = false;
	Finish();
}

void UWuwaEffectNiagaraSpec::ApplyStateAtAge()
{
	UNiagaraComponent* Component = NiagaraComponent;
	if (!Model || !IsValid(Component)) return;

	const FTransform Local = WuwaEffectTransform::Evaluate(Model->Location, Model->Rotation, Model->Scale, Age);
	if (bAttached)
	{
		Component->SetRelativeTransform(Local * BaseTransform);
	}
	else
	{
		Component->SetWorldTransform(Local * BaseTransform);
	}
	for (const auto& Parameter : Model->FloatParameters)
	{
		Component->SetVariableFloat(Parameter.Key, Parameter.Value.Evaluate(Age));
	}
	for (const auto& Parameter : Model->VectorParameters)
	{
		Component->SetVariableVec3(Parameter.Key, Parameter.Value.Evaluate(Age));
	}
	for (const auto& Parameter : Model->ColorParameters)
	{
		Component->SetVariableLinearColor(Parameter.Key, Parameter.Value.Evaluate(Age));
	}
}

bool UWuwaEffectNiagaraSpec::ShouldAutoStop() const
{
	// 起始段结束且没有循环段：进入结束段，停止发射。负数 StartTime 保留原作的持续播放语义，需外部停止。
	return Model && !bStopping && Model->LoopTime <= 0.f && Model->StartTime > 0.f;
}

void UWuwaEffectNiagaraSpec::Tick(float DeltaTime)
{
	Age += FMath::Max(0.f, DeltaTime);
	if (bAnimated) ApplyStateAtAge();
	if (ShouldAutoStop() && Age >= Model->StartTime)
	{
		Stop_Implementation(false);
	}
}

bool UWuwaEffectNiagaraSpec::IsTickable() const
{
	return bPlaying && !IsFinished() && IsValid(NiagaraComponent) && (bAnimated || ShouldAutoStop());
}

ETickableTickType UWuwaEffectNiagaraSpec::GetTickableTickType() const
{
	// 类默认对象不参与 Tick；实例按 IsTickable 决定。
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

TStatId UWuwaEffectNiagaraSpec::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWuwaEffectNiagaraSpec, STATGROUP_Tickables);
}

void UWuwaEffectNiagaraSpec::ResetForPool()
{
	if (IsValid(NiagaraComponent))
	{
		NiagaraComponent->OnSystemFinished.RemoveAll(this);
		NiagaraComponent->DeactivateImmediate();
		NiagaraComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		NiagaraComponent->SetWorldTransform(FTransform::Identity);
	}
	Model = nullptr;
	BaseTransform = FTransform::Identity;
	bAttached = false;
	bAnimated = false;
	bPlaying = false;
	bStopping = false;
	Age = 0.f;
}

void UWuwaEffectNiagaraSpec::ReleaseNiagaraComponent()
{
	if (IsValid(NiagaraComponent))
	{
		NiagaraComponent->OnSystemFinished.RemoveAll(this);
		NiagaraComponent->DeactivateImmediate();
		NiagaraComponent->DestroyComponent();
	}
	NiagaraComponent = nullptr;
}
