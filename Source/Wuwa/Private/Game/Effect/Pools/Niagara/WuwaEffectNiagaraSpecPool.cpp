#include "Game/Effect/Pools/Niagara/WuwaEffectNiagaraSpecPool.h"
#include "Game/Effect/Specs/Niagara/WuwaEffectNiagaraSpec.h"

UWuwaEffectSpec* UWuwaEffectNiagaraSpecPool::CreateSpec_Implementation()
{
	return SpecClass && SpecClass->IsChildOf(UWuwaEffectNiagaraSpec::StaticClass())
		? Super::CreateSpec_Implementation() : nullptr;
}

void UWuwaEffectNiagaraSpecPool::ResetSpec_Implementation(UWuwaEffectSpec* Spec)
{
	CastChecked<UWuwaEffectNiagaraSpec>(Spec)->ResetForPool();
}

void UWuwaEffectNiagaraSpecPool::DestroySpec_Implementation(UWuwaEffectSpec* Spec)
{
	CastChecked<UWuwaEffectNiagaraSpec>(Spec)->ReleaseNiagaraComponent();
}
