#pragma once

#include "Tickable.h"
#include "Game/Effect/Specs/Base/WuwaEffectSpec.h"
#include "WuwaEffectMaterialControllerSpec.generated.h"

class UMaterialInstanceDynamic;
class UWuwaCharacterMaterialControllerData;

/** 一个被控制的材质实例，以及它被写入前的参数值（用于还原）。 */
USTRUCT()
struct FWuwaControlledMaterial
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	TMap<FName, float> OriginalScalars;
	TMap<FName, FLinearColor> OriginalVectors;
};

/**
 * 角色材质控制器的一次播放，对应原作 CharMaterialControlRuntimeData + CharMaterialContainer（效果类型）：
 * - 时间轴：开始 / 循环 / 结束三段，每段内进度 = 段内计时 / 段长（0~1），各参数组按所在段与进度取样；
 *   按原作逻辑先判段再累加时间，播到总时长前一帧标记结束，结束时还原参数。
 * - 作用范围：播放请求 AttachTo 所属 Actor 的全部骨骼网格的全部材质槽（原作未指定部位时选全部部位），
 *   必要时为材质槽创建动态材质实例。
 * - 写入的参数名与原作 RenderConfig 一致（E_Rim_*），角色材质需要有这些参数（WuwaCharacterRimSetup 命令行加入）。
 * 原作多个控制器同时播放时按 MaskOriginEffect 互相屏蔽的规则没有实现。
 */
UCLASS()
class WUWA_API UWuwaEffectMaterialControllerSpec : public UWuwaEffectSpec, public FTickableGameObject
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

	/** 当前段（0 开始、1 循环、2 结束）与段内进度，测试用。 */
	uint8 GetPhase() const { return Phase; }
	float GetFactor() const { return Factor; }

private:
	void Advance(float DeltaTime);
	void Apply();
	void Revert();
	void SetScalar(FWuwaControlledMaterial& Target, FName Name, float Value);
	void SetVector(FWuwaControlledMaterial& Target, FName Name, const FLinearColor& Value);

	UPROPERTY(Transient)
	TObjectPtr<UWuwaCharacterMaterialControllerData> Data;

	UPROPERTY(Transient)
	TArray<FWuwaControlledMaterial> Materials;

	float CurrentTime = 0.f;
	float WholeTimeCounter = 0.f;
	float PhaseTimeCounter = 0.f;
	float Factor = 0.f;
	uint8 Phase = 0;
	bool bUpdatedOnce = false;
	bool bReadyToDie = false;
	bool bPlaying = false;
};
