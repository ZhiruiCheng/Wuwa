#pragma once

#include "CoreMinimal.h"
#include "WuwaEffectSpawnRequest.generated.h"

class USceneComponent;
class UWuwaEffectModelBase;

/**
 * 对应原作 EffectRuntimeGhostEffectContext：残影的生成节奏和单个残影寿命由通知给出，不在 Model 里。
 * 只有残影 Spec 读取；残影源网格是请求的 AttachTo（骨骼网格组件）。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaEffectGhostContext
{
	GENERATED_BODY()

	/** 为 true 时生成间隔 = 1 / SpawnRate，否则用 SpawnInterval。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost")
	bool bUseSpawnRate = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost", meta = (Units = "Hz"))
	float SpawnRate = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost", meta = (Units = "s"))
	float SpawnInterval = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghost", meta = (Units = "s"))
	float GhostLifeTime = 0.f;
};

/** 一次播放的输入。Model 是共享配置，其余字段描述本次播放的位置和挂接目标。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaEffectSpawnRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UWuwaEffectModelBase> Model;

	/** 无 AttachTo 时为世界变换；有 AttachTo 时为相对 SocketName 的变换。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FTransform Transform = FTransform::Identity;

	/** 留空则在世界中播放。弱引用避免播放请求延长场景组件的生命周期。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	TWeakObjectPtr<USceneComponent> AttachTo;

	/** 留空使用组件原点。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FName SocketName;

	/** 仅残影播放使用。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FWuwaEffectGhostContext Ghost;
};
