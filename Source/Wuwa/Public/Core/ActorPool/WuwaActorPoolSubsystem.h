#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WuwaActorPoolSubsystem.generated.h"

class ALevelSequenceActor;

/**
 * 全局 Actor 对象池（≈ 原作 Core/Actor/ActorSystem）：按类缓存用完的 Actor，下次同类直接复用，
 * 超出容量的放进待销毁列表，每帧最多销毁一个。每个关卡世界一份，切换地图时随世界一起清空。
 */

USTRUCT()
struct FWuwaActorPoolEntry
{
	GENERATED_BODY()

	UPROPERTY() TArray<TObjectPtr<AActor>> IdleActors;   // 闲置的 Actor（≈ 原作 Entry.Values）
	int32 Budget = 0;                                    // 超过它的类在总容量满时优先被淘汰，0 = 不设预算
	int32  AccessCount = 0; //被访问次数
	double AvgSpawnCostMs = 0.0; //生成一个这个类平均要多久
	double Score = 0.0; //分数越高越值得保留
};


UCLASS()
class WUWA_API UWuwaActorPoolSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWuwaActorPoolSubsystem* Get(const UObject* WorldContext);
	virtual void Deinitialize() override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	// 对外接口（取出 / 还回 / 类预算）
	UFUNCTION(BlueprintCallable, Category = "Wuwa|ActorPool")
	void SetBudget(TSubclassOf<AActor> ActorClass, int32 Budget);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	UFUNCTION(BlueprintCallable, Category = "Wuwa|ActorPool")
	AActor* GetActor(TSubclassOf<AActor> ActorClass,const FTransform& Transform,AActor* Owner = nullptr);
	UFUNCTION(BlueprintCallable, Category = "Wuwa|ActorPool")
	bool PutActor(AActor* Actor, const FString& Reason);

	/** 设置总容量（≈ 原作 ActorSystem.Capacity 的 set）：小于 2 报错不改；调小后立刻淘汰到不超过新容量。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|ActorPool")
	void SetCapacity(int32 NewCapacity);
	UFUNCTION(BlueprintPure, Category = "Wuwa|ActorPool")
	int32 GetCapacity() const { return Capacity; }

	/** 总开关（≈ 原作 ActorSystem.Enable）：关掉后取出直接生成、还回直接销毁，池里已有的闲置 Actor 保留。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|ActorPool")
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }
	UFUNCTION(BlueprintPure, Category = "Wuwa|ActorPool")
	bool IsEnabled() const { return bEnabled; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	

	
private: 
	AActor* SpawnPooledActor(TSubclassOf<AActor> ActorClass, const FTransform& Transform, AActor* Owner); 
	bool PrepareBeforeGet(AActor* Actor);  
	void CleanBeforePut(AActor* Actor);
	AActor* SpawnActor(TSubclassOf<AActor> ActorClass, const FTransform& Transform, AActor* Owner) const;
	FWuwaActorPoolEntry& FindOrAddEntry(TSubclassOf<AActor> ActorClass);
	bool EvictOne();
	void TouchEntry(FWuwaActorPoolEntry* Entry, double SpawnCostMs = -1.0);
	/** 关卡序列 Actor 的专门清理（≈ 原作 ActorPoolGuard.ClearSequenceActor）：清设置、绑定、回调和原点，不隐藏、不注销组件。 */
	void CleanSequenceActorBeforePut(ALevelSequenceActor* SequenceActor);

	/**
	 * 池生成的 Actor 被销毁时调用（≈ 原作给池 Actor 绑的销毁 / 结束回调）：清掉 PooledActors 里的记录。
	 * 还在闲置列表里说明它在池里被别人销毁了，同时移出闲置列表并报错；正在使用中被销毁的只清记录。
	 */
	UFUNCTION()
	void HandlePooledActorDestroyed(AActor* DestroyedActor);
private:
	//内部函数和数据
	UPROPERTY()
	TMap<TObjectPtr<UClass>,FWuwaActorPoolEntry> Pools;//每种类型一个池
	UPROPERTY()
	TMap<TObjectPtr<UClass>,int32> Budgets; //最多存多少个
	UPROPERTY()
	TSet<TObjectPtr<AActor>> PooledActors;//代替引擎标记哪些是池子里创建出来的,if(PoolActors.Contain(Actor))如果是false就表明不是这个池子里创建出来的
	UPROPERTY()
	TArray<TObjectPtr<AActor>> PendingDestroy;
	
	//int32保证在各个平台的长度一致
	int32 IdleCount = 0;//池子里面空闲的数量
	int32 Capacity = 300;//总容量
	bool bEnabled = true;//总开关
	int64 TouchCounter = 0;//全局计数器,每"使用"任何一次类就加一表示有多新
	
};
