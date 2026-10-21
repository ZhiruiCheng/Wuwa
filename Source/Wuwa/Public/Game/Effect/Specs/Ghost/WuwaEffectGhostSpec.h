#pragma once

#include "Tickable.h"
#include "Game/Effect/Specs/Base/WuwaEffectSpec.h"
#include "WuwaEffectGhostSpec.generated.h"

class UPoseableMeshComponent;
class USkeletalMeshComponent;
class UWuwaEffectModelGhost;

/** 同一时刻生成的一批残影（每个选中的骨骼网格一个），共用到期时间。 */
USTRUCT()
struct FWuwaGhostBatch
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPoseableMeshComponent>> Meshes;

	TArray<FName> Names;
	float ExpireTime = 0.f;
};

/** 某个部位闲置的残影组件，复用避免反复创建。 */
USTRUCT()
struct FWuwaGhostIdleList
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPoseableMeshComponent>> Meshes;
};

/**
 * 角色残影，对应原作 EffectModelGhostSpec：
 * - 播放中每隔 1/SpawnRate（或 SpawnInterval）秒，为每个选中的骨骼网格生成一个 PoseableMesh，复制当前姿势与世界变换，所有材质槽换成 Model 的材质；
 *   第一次在播放开始时立即生成。
 * - 每帧把 AlphaCurve(剩余寿命 / GhostLifeTime) 写进残影的 CustomPrimitiveData[0]，到期隐藏并放回闲置列表。
 * - 非立即停止只停止生成，已有残影淡完后结束；立即停止销毁全部组件。
 * 源网格是请求的 AttachTo；生成节奏与寿命来自请求的 Ghost 上下文（原作由 AnimNotifyStateGhost 传入）。
 * 原作对残影设了 LODBias 3，这里没有对应（只影响性能）。
 */
UCLASS()
class WUWA_API UWuwaEffectGhostSpec : public UWuwaEffectSpec, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual bool Play_Implementation(const FWuwaEffectSpawnRequest& Request) override;
	virtual void Stop_Implementation(bool bImmediately) override;

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickableWhenPaused() const override { return false; }

private:
	void CollectSourceMeshes(USkeletalMeshComponent* Source);
	void SpawnBatch();
	void UpdateBatches();
	void ReleaseAll();

	UPROPERTY(Transient)
	TObjectPtr<UWuwaEffectModelGhost> Model;

	/** 源骨骼网格 → 部位名（Body / WeaponCase0 / 自定义组件名）。 */
	TArray<TPair<TWeakObjectPtr<USkeletalMeshComponent>, FName>> Sources;

	UPROPERTY(Transient)
	TArray<FWuwaGhostBatch> Batches;

	UPROPERTY(Transient)
	TMap<FName, FWuwaGhostIdleList> Idle;

	float Interval = -1.f;
	float LifeTime = 0.f;
	float Age = 0.f;
	float LastSpawn = -999.f;
	bool bPlaying = false;
	bool bStopping = false;
};
