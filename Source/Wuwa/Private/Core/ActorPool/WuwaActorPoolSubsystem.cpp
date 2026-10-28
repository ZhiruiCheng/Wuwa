#include "Core/ActorPool/WuwaActorPoolSubsystem.h"
#include "GameFramework/Actor.h"              // 用到 AActor 的成员函数
#include "Core/ActorPool/WuwaPoolableActor.h" // 用到 Execute_OnTakenFromPool
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "DefaultLevelSequenceInstanceData.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
DEFINE_LOG_CATEGORY_STATIC(LogWuwaActorPool, Log, All);

namespace WuwaActorPool
{
	// 每帧最多销毁几个待销毁的 Actor,流量控制
	static int32 MaxDestroyPerFrame = 1;
	static FAutoConsoleVariableRef CVarMaxDestroyPerFrame(TEXT("wuwa.ActorPool.MaxDestroyPerFrame"), MaxDestroyPerFrame,
		TEXT("对象池每帧最多销毁几个被淘汰的 Actor，默认 1。调大清得更快，但单帧开销更大。"));
}

#pragma region 子系统基础接口
//分了四种类,有Editor,Game(正在运行的世界)等等
bool UWuwaActorPoolSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

//性能宏统计
TStatId UWuwaActorPoolSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWuwaActorPoolSubsystem, STATGROUP_Tickables);
}

UWuwaActorPoolSubsystem* UWuwaActorPoolSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UWuwaActorPoolSubsystem>() : nullptr;
}
#pragma endregion

#pragma region 生命周期
void UWuwaActorPoolSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	//SetBudget(某个特效Actor类::StaticClass(), 100); 
}

//每帧最多销毁 wuwa.ActorPool.MaxDestroyPerFrame 个（默认 1），但是具体还是得看GC情况
void UWuwaActorPoolSubsystem::Tick(float DeltaTime)
{
	const int32 MaxDestroy = FMath::Max(1, WuwaActorPool::MaxDestroyPerFrame);
	int32 Destroyed = 0;
	while (PendingDestroy.Num() > 0 && Destroyed < MaxDestroy)
	{
		// 从末尾取出，同时移出待销毁列表。
		AActor* Actor = PendingDestroy.Pop().Get();

		// 移除对象池归属记录。
		PooledActors.Remove(Actor);

		// 已经失效的直接跳过，不占本帧销毁名额。
		if (!IsValid(Actor))
		{
			continue;
		}

		Actor->Destroy();
		++Destroyed; // 只有真正销毁的才计数，达到上限后本帧停止。
	}
}

void UWuwaActorPoolSubsystem::Deinitialize()
{
	Pools.Empty();
	Budgets.Empty();
	PooledActors.Empty();
	PendingDestroy.Empty();
	IdleCount = 0;
	TouchCounter = 0;
	Super::Deinitialize();
}
#pragma endregion


void UWuwaActorPoolSubsystem::SetBudget(TSubclassOf<AActor> ActorClass, int32 Budget)
{
	if (ActorClass == nullptr)
	{
		UE_LOG(LogWuwaActorPool, Error,
			TEXT("ActorPool::SetBudget failed: ActorClass is null."));
		return;
	}
	if (Budget < 0)
	{
		UE_LOG(LogWuwaActorPool, Error,
			TEXT("ActorPool::SetBudget failed: Numbers of budget should bigger than zero."));
		return;
	}
	//就算有也只是会更新数值而不是添加一个新的
	Budgets.Add(ActorClass,Budget);
	
	if (FWuwaActorPoolEntry* Entry = Pools.Find(ActorClass.Get()))
	{
		Entry->Budget = Budget;
	}
	return;
}


AActor* UWuwaActorPoolSubsystem::GetActor(TSubclassOf<AActor> ActorClass, const FTransform& Transform, AActor* Owner)
{
	//1.检查class是不是为空
	if (ActorClass == nullptr)
	{
		UE_LOG(LogWuwaActorPool, Warning,
			TEXT("ActorPool::GetActor failed: ActorClass is null. Owner=%s"),
			*GetNameSafe(Owner));
		return nullptr;
	}
	
	//池子没开就直接返回World的默认创建函数创建
	if (bEnabled == false)
	{
		AActor* Actor = SpawnActor(ActorClass, Transform, Owner);
		return Actor;
	}
	
	//从池子里面查找,没有就创建池子,然后标记一个新创建的池对象返回(注意这时Pool还不知道有这个对象)
	FWuwaActorPoolEntry* entry = Pools.Find(ActorClass.Get());
	if (entry == nullptr)
	{
		FindOrAddEntry(ActorClass);
		return SpawnPooledActor(ActorClass, Transform, Owner);
	}
	
	//如果池子里面有,就拿出最早归还的那个也就是第0个,并且做拿之前的初始化
	while (entry!= nullptr && entry->IdleActors.Num() > 0)
	{
		AActor* actor = entry->IdleActors[0].Get();
		entry->IdleActors.RemoveAt(0);
		--IdleCount;
		//可能已经被外部销毁就跳过他
		if (!IsValid(actor))
		{
			UE_LOG(LogWuwaActorPool, Warning,
			TEXT("ActorPool::GetActor: Invalid idle actor. Class=%s"),
			*GetNameSafe(ActorClass.Get()));
			continue;
		}
		if (PrepareBeforeGet(actor) == false)
		{
			entry = Pools.Find(ActorClass.Get());//有可能期间扩容的时候导致指针失效,所以要重新调用
			PendingDestroy.Add(actor);
			continue;
		}
		
		//从池子里面拿了一个出来,所以要算上Touch里面
		TouchEntry(Pools.Find(ActorClass.Get())); 
		actor->SetActorTransform(Transform,false,nullptr,ETeleportType::TeleportPhysics);
		if (Owner != nullptr){actor->SetOwner(Owner);}
		return actor;
	}
	
	// 池原本就是空的，或者所有闲置 Actor 都不可用。(有一种非常极端的情况,Prepare的里面调用了Put然后超过了最大容量导致刚刚拿下的Actor的那个池子被删除了因为刚刚取出了最后一个,所以之后的entry是null,导致转发到这个分支)
	return SpawnPooledActor(ActorClass, Transform, Owner);
}

//true表示成功还回,false表示没返回成功可能是重复或者不是这个池子里面的对象
bool UWuwaActorPoolSubsystem::PutActor(AActor* Actor, const FString& Reason)
{
	if (!IsValid(Actor))
	{
		UE_LOG(LogWuwaActorPool, Warning,
		   TEXT("ActorPool::PutActor failed: Actor is invalid. Reason=%s"),
		   *Reason);
		return false;
	}
	
	if (bEnabled == false)
	{
		Actor->Destroy();
		return true;
	}
	
	//不是池子里面出来的直接销毁,因为池子不知道如何管理这个Actor
	if (PooledActors.Find(Actor) == nullptr)
	{
		Actor->Destroy();
		return false;
	}
	
	//是池子里面生成的
	CleanBeforePut(Actor);
	
	//防止重复载入,比如特效播完说我结束了归还到了池子,管理系统也说结束了将其归还给池子导致调用了两次
	const FWuwaActorPoolEntry* Existing = Pools.Find(Actor->GetClass());
	if (Existing && Existing->IdleActors.Contains(Actor))
	{
		UE_LOG(LogWuwaActorPool, Warning,
		   TEXT("ActorPool::PutActor has already be put on pool: Put twice. Reason=%s"),
		   *Reason);
		return false;
	}
	
	//查找池子
	FWuwaActorPoolEntry& Entry = FindOrAddEntry(Actor->GetClass());
	Entry.IdleActors.Add(Actor);
	++IdleCount;
	TouchEntry(Pools.Find(Actor->GetClass()));
	//当总体的数量大于了总的容量就做清理
	while (IdleCount > Capacity || Pools.Num() > Capacity)
	{
		if (!EvictOne()) break;
	}
	return true;
}


AActor* UWuwaActorPoolSubsystem::SpawnPooledActor(TSubclassOf<AActor> ActorClass, const FTransform& Transform,
                                                  AActor* Owner)
{
	const double Start = FPlatformTime::Seconds();
	AActor* Actor = SpawnActor(ActorClass, Transform, Owner);
	const double CostMs = (FPlatformTime::Seconds() - Start) * 1000.0;
	TouchEntry(Pools.Find(ActorClass.Get()), CostMs);
	if (IsValid(Actor))
	{
		PooledActors.Add(Actor);
		// 被谁销毁都能知道，及时清掉记录（≈ 原作生成池 Actor 时绑的销毁回调）
		Actor->OnDestroyed.AddUniqueDynamic(this, &UWuwaActorPoolSubsystem::HandlePooledActorDestroyed);
		return Actor;
	}
	return nullptr;
}

bool UWuwaActorPoolSubsystem::PrepareBeforeGet(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return false;
	}
	// 1. 将Actor上的组件重新注册到世界中。
	Actor->RegisterAllComponents();
	
	const AActor* Default = Actor->GetClass()->GetDefaultObject<AActor>();
	// 2. 使用默认的Hidden,拿出来就像新的一样
	Actor->SetActorHiddenInGame(Default->IsHidden());

	// 3. 开启 Actor 自身的 Ticks设置。
	Actor->SetActorTickEnabled(Default->PrimaryActorTick.bStartWithTickEnabled);

	// 4. 开启 Actor 的碰撞总开关。
	Actor->SetActorEnableCollision(true);

	// 5. 如果实现了对象池接口，通知它恢复自己的业务状态。
	if (Actor->GetClass()->ImplementsInterface(UWuwaPoolableActor::StaticClass()))
	{
		IWuwaPoolableActor::Execute_OnTakenFromPool(Actor);
	}

	// 回调可能销毁 Actor，因此最后再检查一次。
	return IsValid(Actor);
}

void UWuwaActorPoolSubsystem::CleanBeforePut(AActor* Actor)
{
	//先调用自己的清理函数
	if (Actor->Implements<UWuwaPoolableActor>()) IWuwaPoolableActor::Execute_OnReturnedToPool(Actor);
	//关卡序列 Actor 走专门的清理，不做下面的默认清理
	if (ALevelSequenceActor* SequenceActor = Cast<ALevelSequenceActor>(Actor))
	{
		CleanSequenceActorBeforePut(SequenceActor);
		return;
	}
	//重置位置->从别的Actor上摘下来->在游戏中隐藏->从世界注销所有组件->停止Tick->关闭Collision
	Actor->SetActorTransform(FTransform::Identity);
	Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Actor->SetActorHiddenInGame(true);
	Actor->UnregisterAllComponents();
	Actor->SetActorTickEnabled(false);
	Actor->SetActorEnableCollision(false);
}

void UWuwaActorPoolSubsystem::CleanSequenceActorBeforePut(ALevelSequenceActor* SequenceActor)
{
	//播放设置恢复默认 -> 清空对象绑定 -> 清空播放器回调 -> 清掉原点 -> 去掉序列资源
	SequenceActor->PlaybackSettings = FMovieSceneSequencePlaybackSettings();
	SequenceActor->ResetBindings();
	if (ULevelSequencePlayer* Player = SequenceActor->GetSequencePlayer())
	{
		Player->OnPlay.Clear();
		Player->OnPlayReverse.Clear();
		Player->OnStop.Clear();
		Player->OnPause.Clear();
		Player->OnFinished.Clear();
		Player->OnCameraCut.Clear();
	}
	SequenceActor->bOverrideInstanceData = false;
	if (UDefaultLevelSequenceInstanceData* Data = Cast<UDefaultLevelSequenceInstanceData>(SequenceActor->DefaultInstanceData))
	{
		Data->TransformOrigin = FTransform::Identity;
		Data->TransformOriginActor = nullptr;
	}
	//原作换成引擎扩展里的空序列；UE 原版传空只清掉资源引用（正在播放时不生效）
	SequenceActor->SetSequence(nullptr);
}

void UWuwaActorPoolSubsystem::HandlePooledActorDestroyed(AActor* DestroyedActor)
{
	//不在记录里：池自己销毁的（Tick / EvictOne 里已经先移出记录）或者已经处理过
	if (PooledActors.Remove(DestroyedActor) == 0)
	{
		return;
	}
	//还在闲置列表里：它在池里被别人销毁了（≈ 原作"ActorSystem的Actor意外删除"）
	if (FWuwaActorPoolEntry* Entry = Pools.Find(DestroyedActor->GetClass()))
	{
		const int32 Removed = Entry->IdleActors.Remove(DestroyedActor);
		if (Removed > 0)
		{
			IdleCount -= Removed;
			UE_LOG(LogWuwaActorPool, Error,
				TEXT("ActorPool: idle pooled actor %s was destroyed outside the pool."), *GetNameSafe(DestroyedActor));
		}
	}
}

void UWuwaActorPoolSubsystem::SetCapacity(int32 NewCapacity)
{
	if (NewCapacity < 2)
	{
		UE_LOG(LogWuwaActorPool, Error,
			TEXT("ActorPool::SetCapacity failed: capacity must be >= 2, got %d."), NewCapacity);
		return;
	}
	Capacity = NewCapacity;
	//调小后立刻淘汰到不超过新容量
	while (IdleCount > Capacity || Pools.Num() > Capacity)
	{
		if (!EvictOne()) break;
	}
}

FWuwaActorPoolEntry& UWuwaActorPoolSubsystem::FindOrAddEntry(TSubclassOf<AActor> ActorClass)
{
	FWuwaActorPoolEntry NewEntry;
	NewEntry.Budget = Budgets.FindRef(ActorClass.Get());
	return Pools.FindOrAdd(ActorClass.Get(), NewEntry);
}

//根据当前的Entry的评分系统筛选哪个池子要被淘汰,删除池子不计入总量,规则是
bool UWuwaActorPoolSubsystem::EvictOne()
{
	if (Pools.Num() == 0)
	{
		return false;
	}
	
	UClass* SelectedClass = nullptr;
	FWuwaActorPoolEntry* SelectedEntry = nullptr;
	bool bSelectedOverBudget = false;
	
	for (auto& Pair :Pools)
	{
		FWuwaActorPoolEntry& Entry = Pair.Value;
		const bool bOverBudget = Entry.Budget > 0 && Entry.IdleActors.Num() > Entry.Budget;
		const bool bShouldSelect = SelectedEntry == nullptr 
		|| (bOverBudget && !bSelectedOverBudget)
		||(bOverBudget == bSelectedOverBudget && Entry.Score < SelectedEntry->Score);
		
		if (bShouldSelect)
		{
			SelectedClass = Pair.Key.Get();
			SelectedEntry = &Entry;
			bSelectedOverBudget = bOverBudget;
		}
	}
	
	if (SelectedEntry->IdleActors.Num() == 0)
	{
		Pools.Remove(SelectedClass);
		return true;
	}
	
	AActor* Actor = SelectedEntry->IdleActors[0].Get();
	SelectedEntry->IdleActors.RemoveAt(0);
	--IdleCount;
	
	TouchEntry(SelectedEntry, -1.0);
	
	if (IsValid(Actor))
	{
		PendingDestroy.Add(Actor);
	}
	else
	{
		PooledActors.Remove(Actor);
	}
	return true;
}

//取走一个、放进一个、淘汰一个，或者为它新生成了一个
void UWuwaActorPoolSubsystem::TouchEntry(FWuwaActorPoolEntry* Entry, double SpawnCostMs)
{
	if (Entry == nullptr)
	{
		UE_LOG(LogWuwaActorPool, Warning,
		   TEXT("ActorPool::TouchEntry the param Entry is nullptr"));
		return;
	}
	Entry->AccessCount++;
	TouchCounter++;
	if (SpawnCostMs >= 0)
	{
		Entry->AvgSpawnCostMs = Entry->AvgSpawnCostMs*0.5+0.5*SpawnCostMs;
	}
	//用得越频繁，越值得缓存+保存本次访问时的全局序号，让最近访问过的池获得优势+生成越耗时，复用它能省下越多开销+已经囤得越多，继续保留的优先级越低
	Entry->Score = Entry->AccessCount * 1+TouchCounter*1+Entry->AvgSpawnCostMs*10 - Entry->IdleActors.Num()*5;
}

AActor* UWuwaActorPoolSubsystem::SpawnActor(TSubclassOf<AActor> ActorClass, const FTransform& Transform, AActor* Owner) const
{
	FActorSpawnParameters Params;
	Params.Owner = Owner;
	Params.SpawnCollisionHandlingOverride =ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Actor = GetWorld()->SpawnActor<AActor>(ActorClass,Transform,Params);
	return Actor;
}
