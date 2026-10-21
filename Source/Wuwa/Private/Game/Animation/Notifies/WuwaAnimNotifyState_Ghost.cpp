#include "Game/Animation/Notifies/WuwaAnimNotifyState_Ghost.h"

#include "Components/SkeletalMeshComponent.h"
#include "Game/Effect/System/WuwaEffectSystem.h"
#include "Game/EffectModel/Models/WuwaEffectModelGhost.h"

FString UWuwaAnimNotifyState_Ghost::GetNotifyName_Implementation() const
{
	return Model ? FString::Printf(TEXT("Ghost: %s"), *Model->GetName()) : TEXT("Character Ghost");
}

void UWuwaAnimNotifyState_Ghost::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (!MeshComp || !Model) return;
	UWuwaEffectSystem* Effects = UWuwaEffectSystem::GetEffectSystem(MeshComp);
	if (!Effects) return;

	FWuwaEffectSpawnRequest Request;
	Request.Model = Model;
	Request.AttachTo = MeshComp;
	Request.Ghost.bUseSpawnRate = bUseSpawnRate;
	Request.Ghost.SpawnRate = SpawnRate;
	Request.Ghost.SpawnInterval = SpawnInterval;
	Request.Ghost.GhostLifeTime = GhostLifeTime;
	const FWuwaEffectHandle Handle = Effects->SpawnEffect(Request);
	if (Handle.Id.IsValid()) Handles.Add(MeshComp, Handle);
}

void UWuwaAnimNotifyState_Ghost::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	FWuwaEffectHandle Handle;
	if (!Handles.RemoveAndCopyValue(MeshComp, Handle)) return;
	if (UWuwaEffectSystem* Effects = UWuwaEffectSystem::GetEffectSystem(MeshComp))
	{
		Effects->StopEffect(Handle, false);
	}
}
