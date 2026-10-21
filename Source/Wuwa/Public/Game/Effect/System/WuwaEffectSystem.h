#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Game/Effect/Types/WuwaEffectHandle.h"
#include "Game/Effect/Types/WuwaEffectSpawnRequest.h"
#include "WuwaEffectSystem.generated.h"

class UWuwaEffectSpec;
class UWuwaEffectSpecFactory;
class UWuwaEffectSpecPool;

/** 保留借出时的池；运行中替换注册不会改变正在播放对象的归还目标。 */
USTRUCT()
struct FWuwaActiveEffect
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UWuwaEffectSpec> Spec;

	UPROPERTY(Transient)
	TObjectPtr<UWuwaEffectSpecPool> Pool;
};

/** 世界级统一播放入口。已注册 Audio、Niagara、Group；不负责解释各 Model 的具体参数。 */
UCLASS()
class WUWA_API UWuwaEffectSystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldEndPlay(UWorld& InWorld) override;

	UFUNCTION(BlueprintPure, Category = "Effect", meta = (WorldContext = "WorldContextObject"))
	static UWuwaEffectSystem* GetEffectSystem(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Effect")
	FWuwaEffectHandle SpawnEffect(const FWuwaEffectSpawnRequest& Request);

	/** 默认按执行器配置收尾；立即停止用于强制取消。淡出期间句柄仍有效。 */
	UFUNCTION(BlueprintCallable, Category = "Effect")
	bool StopEffect(FWuwaEffectHandle Handle, bool bImmediately = false);

	UFUNCTION(BlueprintPure, Category = "Effect")
	bool IsEffectActive(FWuwaEffectHandle Handle) const;

	UFUNCTION(BlueprintPure, Category = "Effect")
	int32 GetActiveEffectCount() const { return ActiveEffects.Num(); }

	UFUNCTION(BlueprintPure, Category = "Effect|Pool")
	int32 GetPooledEffectCount() const;

	/** 只释放空闲缓存，不打断正在播放的效果。 */
	UFUNCTION(BlueprintCallable, Category = "Effect|Pool")
	void ClearEffectPools();

	/** 后续新增类型只需注册对应关系；也允许注册蓝图或 C# 的 Spec 子类。 */
	UFUNCTION(BlueprintCallable, Category = "Effect")
	bool RegisterEffectSpec(TSubclassOf<UWuwaEffectModelBase> ModelClass, TSubclassOf<UWuwaEffectSpec> SpecClass,
		TSubclassOf<UWuwaEffectSpecPool> PoolClass = nullptr);

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	void HandleFinished(FGuid Id);
	void StopAllEffects();

	UPROPERTY(Transient)
	TObjectPtr<UWuwaEffectSpecFactory> Factory;

	UPROPERTY(Transient)
	TMap<FGuid, FWuwaActiveEffect> ActiveEffects;

	bool bShuttingDown = false;
};
