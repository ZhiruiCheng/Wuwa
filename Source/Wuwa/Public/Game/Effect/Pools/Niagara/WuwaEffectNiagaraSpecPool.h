#pragma once

#include "Game/Effect/Pools/Base/WuwaEffectSpecPool.h"
#include "WuwaEffectNiagaraSpecPool.generated.h"

/** 归还时保留 NiagaraComponent；淘汰时才销毁组件。 */
UCLASS()
class WUWA_API UWuwaEffectNiagaraSpecPool : public UWuwaEffectSpecPool
{
	GENERATED_BODY()

protected:
	virtual UWuwaEffectSpec* CreateSpec_Implementation() override;
	virtual void ResetSpec_Implementation(UWuwaEffectSpec* Spec) override;
	virtual void DestroySpec_Implementation(UWuwaEffectSpec* Spec) override;
};
