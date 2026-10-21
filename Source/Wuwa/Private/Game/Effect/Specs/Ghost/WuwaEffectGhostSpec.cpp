#include "Game/Effect/Specs/Ghost/WuwaEffectGhostSpec.h"

#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Game/Effect/Types/WuwaEffectSpawnRequest.h"
#include "Game/EffectModel/Models/WuwaEffectModelGhost.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaEffectGhost, Log, All);

namespace WuwaEffectGhost
{
	/** 原作 EffectModelGhostSpec.GetComponentName；Body 指残影源网格本身。 */
	FName ComponentName(EWuwaEffectGhostMesh Mesh)
	{
		switch (Mesh)
		{
		case EWuwaEffectGhostMesh::Body: return TEXT("Body");
		case EWuwaEffectGhostMesh::WeaponCase0: return TEXT("WeaponCase0");
		case EWuwaEffectGhostMesh::WeaponCase1: return TEXT("WeaponCase1");
		case EWuwaEffectGhostMesh::WeaponCase2: return TEXT("WeaponCase2");
		case EWuwaEffectGhostMesh::WeaponCase3: return TEXT("WeaponCase3");
		case EWuwaEffectGhostMesh::WeaponCase4: return TEXT("WeaponCase4");
		case EWuwaEffectGhostMesh::HuluCase: return TEXT("HuluCase");
		case EWuwaEffectGhostMesh::OtherCase0: return TEXT("OtherCase0");
		case EWuwaEffectGhostMesh::OtherCase1: return TEXT("OtherCase1");
		case EWuwaEffectGhostMesh::OtherCase2: return TEXT("OtherCase2");
		case EWuwaEffectGhostMesh::OtherCase3: return TEXT("OtherCase3");
		case EWuwaEffectGhostMesh::OtherCase4: return TEXT("OtherCase4");
		}
		return NAME_None;
	}
}

bool UWuwaEffectGhostSpec::Play_Implementation(const FWuwaEffectSpawnRequest& Request)
{
	Model = Cast<UWuwaEffectModelGhost>(Request.Model);
	USkeletalMeshComponent* Source = Cast<USkeletalMeshComponent>(Request.AttachTo.Get());
	if (!Model || !GetWorld())
	{
		return false;
	}
	if (!Model->MaterialRef)
	{
		// 没有材质时残影会以默认灰格材质出现，宁可不播。
		UE_LOG(LogWuwaEffectGhost, Warning, TEXT("%s: no ghost material is configured."), *Model->GetName());
		return false;
	}
	if (!Source)
	{
		UE_LOG(LogWuwaEffectGhost, Warning, TEXT("%s: ghost needs a skeletal mesh component as AttachTo."), *Model->GetName());
		return false;
	}

	ReleaseAll();
	const FWuwaEffectGhostContext& Context = Request.Ghost;
	Interval = Context.bUseSpawnRate ? 1.f / FMath::Max(Context.SpawnRate, 1e-6f) : Context.SpawnInterval;
	LifeTime = Context.GhostLifeTime;
	CollectSourceMeshes(Source);
	Age = 0.f;
	LastSpawn = -999.f;
	bPlaying = true;
	bStopping = false;
	// 原作第一次 Tick 就生成（上次生成时间初始化为 -999）；这里在开始时立即生成，效果相同。
	Tick(0.f);
	return true;
}

void UWuwaEffectGhostSpec::CollectSourceMeshes(USkeletalMeshComponent* Source)
{
	Sources.Reset();
	TMap<FName, USkeletalMeshComponent*> ByName;
	if (const AActor* Owner = Source->GetOwner())
	{
		TArray<USkeletalMeshComponent*> Components;
		Owner->GetComponents(Components);
		for (USkeletalMeshComponent* Component : Components)
		{
			if (Component && Component != Source) ByName.Add(Component->GetFName(), Component);
		}
	}
	auto Add = [this, &ByName](FName Name)
	{
		if (USkeletalMeshComponent** Found = ByName.Find(Name)) Sources.Emplace(*Found, Name);
		else UE_LOG(LogWuwaEffectGhost, Warning, TEXT("%s: no skeletal mesh component named %s for the ghost."), *Model->GetName(), *Name.ToString());
	};
	for (const EWuwaEffectGhostMesh Mesh : Model->MeshComponentsToUse)
	{
		if (Mesh == EWuwaEffectGhostMesh::Body) Sources.Emplace(Source, WuwaEffectGhost::ComponentName(Mesh));
		else Add(WuwaEffectGhost::ComponentName(Mesh));
	}
	for (const FName Name : Model->CustomComponentNames)
	{
		Add(Name);
	}
}

void UWuwaEffectGhostSpec::SpawnBatch()
{
	FWuwaGhostBatch& Batch = Batches.AddDefaulted_GetRef();
	for (const auto& Pair : Sources)
	{
		USkeletalMeshComponent* Source = Pair.Key.Get();
		if (!Source || !Source->IsVisible() || Source->bHiddenInGame || !Source->GetSkeletalMeshAsset()) continue;

		UPoseableMeshComponent* Ghost = nullptr;
		FWuwaGhostIdleList& IdleList = Idle.FindOrAdd(Pair.Value);
		if (!IdleList.Meshes.IsEmpty())
		{
			Ghost = IdleList.Meshes.Pop();
			Ghost->SetVisibility(true);
		}
		else
		{
			Ghost = NewObject<UPoseableMeshComponent>(this);
			Ghost->SetSkinnedAssetAndUpdate(Source->GetSkeletalMeshAsset(), false);
			Ghost->SetCastShadow(false);
			Ghost->RegisterComponentWithWorld(GetWorld());
			for (int32 Slot = 0; Slot < Ghost->GetNumMaterials(); ++Slot)
			{
				Ghost->SetMaterial(Slot, Model->MaterialRef);
			}
		}
		Ghost->SetWorldTransform(Source->GetComponentTransform());
		Ghost->CopyPoseFromSkeletalComponent(Source);
		Ghost->SetCustomPrimitiveDataFloat(0, 1.f);
		Batch.Meshes.Add(Ghost);
		Batch.Names.Add(Pair.Value);
	}
	Batch.ExpireTime = Age + LifeTime;
	LastSpawn = Age;
}

void UWuwaEffectGhostSpec::UpdateBatches()
{
	for (int32 Index = 0; Index < Batches.Num();)
	{
		FWuwaGhostBatch& Batch = Batches[Index];
		if (Batch.ExpireTime >= Age)
		{
			// 曲线横轴是剩余寿命比例：刚生成为 1，消失前为 0。
			const float Remaining = LifeTime > 0.f ? (Batch.ExpireTime - Age) / LifeTime : 0.f;
			const float Alpha = Model->AlphaCurve.Evaluate(Remaining);
			for (UPoseableMeshComponent* Ghost : Batch.Meshes)
			{
				if (IsValid(Ghost)) Ghost->SetCustomPrimitiveDataFloat(0, Alpha);
			}
			++Index;
			continue;
		}
		for (int32 Mesh = 0; Mesh < Batch.Meshes.Num(); ++Mesh)
		{
			UPoseableMeshComponent* Ghost = Batch.Meshes[Mesh];
			if (!IsValid(Ghost)) continue;
			Ghost->SetVisibility(false);
			Idle.FindOrAdd(Batch.Names[Mesh]).Meshes.Add(Ghost);
		}
		Batches.RemoveAt(Index);
	}
}

void UWuwaEffectGhostSpec::Tick(float DeltaTime)
{
	Age += FMath::Max(0.f, DeltaTime);
	UpdateBatches();
	if (!bStopping && Interval >= 0.f && Age - LastSpawn >= Interval)
	{
		SpawnBatch();
	}
	// 停止生成后，最后一批残影淡完才结束（原作 OnCanStop）。
	if (bStopping && Batches.IsEmpty())
	{
		bPlaying = false;
		ReleaseAll();
		Finish();
	}
}

void UWuwaEffectGhostSpec::Stop_Implementation(bool bImmediately)
{
	if (IsFinished()) return;
	bStopping = true;
	if (bImmediately || Batches.IsEmpty())
	{
		bPlaying = false;
		ReleaseAll();
		Finish();
	}
}

bool UWuwaEffectGhostSpec::IsTickable() const
{
	return bPlaying && !IsFinished();
}

ETickableTickType UWuwaEffectGhostSpec::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

TStatId UWuwaEffectGhostSpec::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWuwaEffectGhostSpec, STATGROUP_Tickables);
}

void UWuwaEffectGhostSpec::ReleaseAll()
{
	for (const FWuwaGhostBatch& Batch : Batches)
	{
		for (UPoseableMeshComponent* Ghost : Batch.Meshes)
		{
			if (IsValid(Ghost)) Ghost->DestroyComponent();
		}
	}
	for (const auto& Pair : Idle)
	{
		for (UPoseableMeshComponent* Ghost : Pair.Value.Meshes)
		{
			if (IsValid(Ghost)) Ghost->DestroyComponent();
		}
	}
	Batches.Reset();
	Idle.Reset();
	Sources.Reset();
}
