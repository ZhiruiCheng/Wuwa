#include "Game/NewWorld/Buff/WuwaAttributeEventComponent.h"

#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "TimerManager.h"

namespace WuwaAttributeEvent
{
	/** 一次生效的 buff 的监听状态（多个 buff 实例互不影响）。 */
	struct FState
	{
		bool bInside = false;
		TArray<FActiveGameplayEffectHandle> Applied;
		FDelegateHandle ValueHandle;
		FDelegateHandle MaxHandle;
	};
}

bool UWuwaAttributeEventComponent::IsInInterval(const UAbilitySystemComponent& ASC) const
{
	bool bFound = false;
	float Value = ASC.GetGameplayAttributeValue(Attribute, bFound);
	if (!bFound) return false;
	if (bPerTenThousand)
	{
		bool bFoundMax = false;
		const float Max = ASC.GetGameplayAttributeValue(MaxAttribute, bFoundMax);
		if (!bFoundMax || Max <= 0.f) return false;
		Value = Value / Max * 10000.f;
	}
	// 原作 AttributeIntervalCheck：值 <= 上界 且 值 > 下界
	return Value <= UpperBound && Value > LowerBound;
}

bool UWuwaAttributeEventComponent::OnActiveGameplayEffectAdded(FActiveGameplayEffectsContainer& ActiveGEContainer, FActiveGameplayEffect& ActiveGE) const
{
	UAbilitySystemComponent* ASC = ActiveGEContainer.Owner;
	if (!ASC || !ASC->IsOwnerActorAuthoritative() || !Attribute.IsValid()) return true;

	using namespace WuwaAttributeEvent;
	const TSharedRef<FState> State = MakeShared<FState>();
	const TWeakObjectPtr<UAbilitySystemComponent> WeakASC(ASC);
	const TWeakObjectPtr<const UWuwaAttributeEventComponent> WeakThis(this);

	// 属性每次变化都重新判断；只在"进入 / 离开"区间的那一下加 / 撤 buff
	auto Evaluate = [WeakASC, WeakThis, State]()
	{
		UAbilitySystemComponent* Owner = WeakASC.Get();
		const UWuwaAttributeEventComponent* Self = WeakThis.Get();
		if (!Owner || !Self) return;
		const bool bInside = Self->IsInInterval(*Owner);
		if (bInside == State->bInside) return;
		State->bInside = bInside;
		if (bInside)
		{
			for (const TSubclassOf<UGameplayEffect>& Effect : Self->EffectsOnEnter)
			{
				if (!Effect) continue;
				const FActiveGameplayEffectHandle Handle = Owner->ApplyGameplayEffectToSelf(Effect->GetDefaultObject<UGameplayEffect>(), 1.f, Owner->MakeEffectContext());
				if (Handle.IsValid()) State->Applied.Add(Handle);
			}
		}
		else if (Self->bRemoveOnLeave)
		{
			for (const FActiveGameplayEffectHandle& Handle : State->Applied) Owner->RemoveActiveGameplayEffect(Handle);
			State->Applied.Reset();
		}
	};

	State->ValueHandle = ASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddLambda([Evaluate](const FOnAttributeChangeData&) { Evaluate(); });
	if (bPerTenThousand && MaxAttribute.IsValid())
	{
		State->MaxHandle = ASC->GetGameplayAttributeValueChangeDelegate(MaxAttribute).AddLambda([Evaluate](const FOnAttributeChangeData&) { Evaluate(); });
	}

	// buff 移除：只停止监听，加过的 buff 留着（原作 OnRemoved 只 RemoveListener）
	const FGameplayAttribute Watched = Attribute;
	const FGameplayAttribute WatchedMax = MaxAttribute;
	ActiveGE.EventSet.OnEffectRemoved.AddLambda([WeakASC, State, Watched, WatchedMax](const FGameplayEffectRemovalInfo&)
	{
		UAbilitySystemComponent* Owner = WeakASC.Get();
		if (!Owner) return;
		Owner->GetGameplayAttributeValueChangeDelegate(Watched).Remove(State->ValueHandle);
		if (State->MaxHandle.IsValid()) Owner->GetGameplayAttributeValueChangeDelegate(WatchedMax).Remove(State->MaxHandle);
		State->Applied.Reset();
	});

	// 加上时就在区间里也要触发（原作 OnCreated 先按当前值判断一次）；放到下一帧，不在 GE 应用过程中再套一层应用
	if (UWorld* World = ASC->GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([Evaluate]() { Evaluate(); }));
	}
	return true;
}
