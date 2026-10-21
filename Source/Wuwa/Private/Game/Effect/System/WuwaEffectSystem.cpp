#include "Game/Effect/System/WuwaEffectSystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/Effect/Factory/WuwaEffectSpecFactory.h"
#include "Game/Effect/Pools/Audio/WuwaEffectAudioSpecPool.h"
#include "Game/Effect/Specs/Audio/WuwaEffectAudioSpec.h"
#include "Game/EffectModel/Models/WuwaEffectModelAudio.h"
#include "Game/Effect/Pools/Niagara/WuwaEffectNiagaraSpecPool.h"
#include "Game/Effect/Specs/Niagara/WuwaEffectNiagaraSpec.h"
#include "Game/Effect/Specs/Group/WuwaEffectGroupSpec.h"
#include "Game/EffectModel/Models/WuwaEffectModelNiagara.h"
#include "Game/EffectModel/Models/WuwaEffectModelGroup.h"
#include "Game/Effect/Specs/Decal/WuwaEffectDecalSpec.h"
#include "Game/EffectModel/Models/WuwaEffectModelDecal.h"
#include "Game/Effect/Specs/Ghost/WuwaEffectGhostSpec.h"
#include "Game/EffectModel/Models/WuwaEffectModelGhost.h"
#include "Game/Effect/Specs/Light/WuwaEffectLightSpec.h"
#include "Game/Effect/Specs/MaterialController/WuwaEffectMaterialControllerSpec.h"
#include "Game/EffectModel/Models/WuwaEffectModelMaterialController.h"
#include "Game/EffectModel/Models/WuwaEffectModelLight.h"
#include "Game/Effect/Specs/PostProcess/WuwaEffectPostProcessSpec.h"
#include "Game/EffectModel/Models/WuwaEffectModelPostProcess.h"
#include "Game/Effect/Specs/SkeletalMesh/WuwaEffectSkeletalMeshSpec.h"
#include "Game/EffectModel/Models/WuwaEffectModelSkeletalMesh.h"

bool UWuwaEffectSystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

#pragma region override function
void UWuwaEffectSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Factory = NewObject<UWuwaEffectSpecFactory>(this);
	RegisterEffectSpec(UWuwaEffectModelAudio::StaticClass(), UWuwaEffectAudioSpec::StaticClass(),
		UWuwaEffectAudioSpecPool::StaticClass());
	RegisterEffectSpec(UWuwaEffectModelNiagara::StaticClass(), UWuwaEffectNiagaraSpec::StaticClass(),
		UWuwaEffectNiagaraSpecPool::StaticClass());
	// Group 不持有组件，通用池即可；子项各自从对应的池借出。
	RegisterEffectSpec(UWuwaEffectModelGroup::StaticClass(), UWuwaEffectGroupSpec::StaticClass());
	// 贴花组件每次播放新建、结束销毁，通用池只复用 Spec 对象。
	RegisterEffectSpec(UWuwaEffectModelDecal::StaticClass(), UWuwaEffectDecalSpec::StaticClass());
	// 灯光、后处理同样每次播放新建组件。
	RegisterEffectSpec(UWuwaEffectModelLight::StaticClass(), UWuwaEffectLightSpec::StaticClass());
	RegisterEffectSpec(UWuwaEffectModelPostProcess::StaticClass(), UWuwaEffectPostProcessSpec::StaticClass());
	// 残影组件在 Spec 内按部位复用，结束时销毁。
	RegisterEffectSpec(UWuwaEffectModelGhost::StaticClass(), UWuwaEffectGhostSpec::StaticClass());
	// 角色材质控制器只改角色材质参数，结束时还原，不持有组件。
	RegisterEffectSpec(UWuwaEffectModelMaterialController::StaticClass(), UWuwaEffectMaterialControllerSpec::StaticClass());
	// 骨骼网格特效（技能里带动画的道具，例如长离的伞）：每次播放新建组件，结束销毁。
	RegisterEffectSpec(UWuwaEffectModelSkeletalMesh::StaticClass(), UWuwaEffectSkeletalMeshSpec::StaticClass());
}


void UWuwaEffectSystem::Deinitialize()
{
	bShuttingDown = true;
	StopAllEffects();
	if (Factory) Factory->Close();
	Factory = nullptr;
	Super::Deinitialize();
}

void UWuwaEffectSystem::OnWorldEndPlay(UWorld& InWorld)
{
	bShuttingDown = true;
	StopAllEffects();
	if (Factory) Factory->Close();
	Super::OnWorldEndPlay(InWorld);
}


#pragma endregion

bool UWuwaEffectSystem::RegisterEffectSpec(TSubclassOf<UWuwaEffectModelBase> ModelClass,
	TSubclassOf<UWuwaEffectSpec> SpecClass, TSubclassOf<UWuwaEffectSpecPool> PoolClass)
{
	return !bShuttingDown && Factory && Factory->Register(ModelClass, SpecClass, PoolClass);
}


UWuwaEffectSystem* UWuwaEffectSystem::GetEffectSystem(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject,
		EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UWuwaEffectSystem>() : nullptr;
}


FWuwaEffectHandle UWuwaEffectSystem::SpawnEffect(const FWuwaEffectSpawnRequest& Request)
{
	if (bShuttingDown || !IsValid(Request.Model) || !Factory) return {};
	UWuwaEffectSpecPool* Pool = Factory->FindPool(Request.Model);
	if (!Pool)
	{
		UE_LOG(LogTemp, Warning, TEXT("EffectSystem: no Spec registered for %s (%s)."),
			*Request.Model->GetClass()->GetName(), *Request.Model->GetName());
		return {};
	}
	UWuwaEffectSpec* Spec = Pool->Acquire();
	if (!Spec) return {};

	FWuwaEffectHandle Handle;
	Handle.Id = FGuid::NewGuid();
	// 先登记再播放，使同步播放失败/结束也能通过同一回调正确释放。
	ActiveEffects.Add(Handle.Id, {Spec, Pool});
	Spec->OnFinished.BindUObject(this, &ThisClass::HandleFinished, Handle.Id);
	if (!Spec->Play(Request))
	{
		// Play 可能已同步 Finish 并归还，不能再操作这个已被回收的 Spec。
		if (ActiveEffects.Contains(Handle.Id))
		{
			Spec->Stop(true);
			HandleFinished(Handle.Id);
		}
		return {};
	}
	return IsEffectActive(Handle) ? Handle : FWuwaEffectHandle{};
}

bool UWuwaEffectSystem::StopEffect(FWuwaEffectHandle Handle, bool bImmediately)
{
	const auto* Entry = ActiveEffects.Find(Handle.Id);
	if (!Entry) return false;
	UWuwaEffectSpec* Spec = Entry->Spec;
	Spec->Stop(bImmediately);
	return true;
}

bool UWuwaEffectSystem::IsEffectActive(FWuwaEffectHandle Handle) const
{
	return ActiveEffects.Contains(Handle.Id);
}

void UWuwaEffectSystem::HandleFinished(FGuid Id)
{
	FWuwaActiveEffect Finished;
	if (ActiveEffects.RemoveAndCopyValue(Id, Finished))
	{
		Finished.Pool->Release(Finished.Spec);
	}
}

int32 UWuwaEffectSystem::GetPooledEffectCount() const
{
	return Factory ? Factory->GetIdleCount() : 0;
}

void UWuwaEffectSystem::ClearEffectPools()
{
	if (Factory) Factory->ClearPools();
}

void UWuwaEffectSystem::StopAllEffects()
{
	// Stop 可同步回调 HandleFinished；先移出表，避免在遍历中修改同一容器。
	auto StoppingEffects = MoveTemp(ActiveEffects);
	for (const auto& Entry : StoppingEffects)
	{
		Entry.Value.Spec->OnFinished.Unbind();
		Entry.Value.Spec->Stop(true);
		Entry.Value.Pool->Release(Entry.Value.Spec);
	}
}