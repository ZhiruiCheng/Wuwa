#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Game/Effect/Types/WuwaEffectHandle.h"
#include "WuwaAnimNotifyState_Ghost.generated.h"

class UWuwaEffectModelGhost;

/**
 * 角色残影窗口，对应原作 AnimNotifyStateGhost：开始时以本骨骼网格为残影源播放 Model，
 * 结束时非立即停止（不再生成，已有残影淡完）。生成节奏与单个残影寿命是通知参数，不在 Model 里。
 */
UCLASS(meta = (DisplayName = "Character Ghost"))
class WUWA_API UWuwaAnimNotifyState_Ghost : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Ghost")
	TObjectPtr<UWuwaEffectModelGhost> Model;

	/** 原作默认用生成频率；关掉后改用 SpawnInterval。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Ghost")
	bool bUseSpawnRate = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Ghost", meta = (EditCondition = "bUseSpawnRate", Units = "Hz"))
	float SpawnRate = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Ghost", meta = (EditCondition = "!bUseSpawnRate", Units = "s"))
	float SpawnInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Ghost", meta = (Units = "s"))
	float GhostLifeTime = 0.6f;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

private:
	/** 通知对象被所有播放这段动画的网格共享，按网格记播放句柄（原作同样按网格记）。 */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FWuwaEffectHandle> Handles;
};
