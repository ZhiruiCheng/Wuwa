#pragma once

#include "Tickable.h"
#include "Game/Effect/Specs/Base/WuwaEffectSpec.h"
#include "WuwaEffectSkeletalMeshSpec.generated.h"

class USkeletalMeshComponent;
class UWuwaEffectModelSkeletalMesh;

/**
 * 一次骨骼网格特效播放（例如长离第四段的伞），对应原作 EffectModelSkeletalMeshSpec：
 * 建骨骼网格组件、设网格、播放动画（Looping 决定是否循环），开头隐藏 HideFrames + 1 帧等姿态准备好，
 * 每帧按播放时长写入 Location / Rotation / Scale 曲线；停止时隐藏、停动画并销毁组件。
 * 原作还会挂 CharRenderingComponent 做角色身体特效的抖动淡出（库洛卡通渲染），这里没有适配。
 */
UCLASS()
class WUWA_API UWuwaEffectSkeletalMeshSpec : public UWuwaEffectSpec, public FTickableGameObject
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

	USkeletalMeshComponent* GetMeshComponent() const { return MeshComponent; }

private:
	void ApplyStateAtAge();
	void ReleaseComponent();

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> MeshComponent;

	UPROPERTY(Transient)
	TObjectPtr<UWuwaEffectModelSkeletalMesh> Model;

	// 请求给出的变换：挂接时是相对插槽，否则是世界变换。Model 的变换曲线叠加在它之上。
	FTransform BaseTransform = FTransform::Identity;
	bool bAttached = false;
	bool bPlaying = false;
	float Age = 0.f;
	/** 还要隐藏的帧数（原作 HideFrames + 1，等第一帧骨骼姿态算完再显示）。 */
	int32 HideCounter = 0;
};
