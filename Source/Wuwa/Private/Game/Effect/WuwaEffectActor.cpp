// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Effect/WuwaEffectActor.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaEffectActor, Log, All);




void AWuwaEffectActor::ApplyEffect(AActor* Target, TSubclassOf<UGameplayEffect> EffectClass)
{
	checkf(EffectClass, TEXT("EffectClass cannot be null"));

	if (!IsValid(Target))
	{
		UE_LOG(LogWuwaEffectActor, Warning, TEXT("%s: target actor is invalid when granting effect."), *GetName());
		return;
	}

	UAbilitySystemComponent* TargetASC = nullptr;
	if (IAbilitySystemInterface* AbilityInterface = Cast<IAbilitySystemInterface>(Target))
	{
		TargetASC = AbilityInterface->GetAbilitySystemComponent();
	}

	if (TargetASC == nullptr)
	{
		UE_LOG(LogWuwaEffectActor, Warning, TEXT("%s: %s has no ASC when granting effect."), *GetName(), *Target->GetName());
		return;
	}

	FGameplayEffectContextHandle EffectContextHandle = TargetASC->MakeEffectContext();
	//Source Object 
	EffectContextHandle.AddSourceObject(this);

	const FGameplayEffectSpecHandle EffectSpecHandle = TargetASC->MakeOutgoingSpec(
		EffectClass,
		1.0f,
		EffectContextHandle);

	if (!EffectSpecHandle.IsValid())
	{
		UE_LOG(LogWuwaEffectActor, Warning, TEXT("%s: failed to create the GameplayEffect spec."), *GetName());
		return;
	}
	//用target将效果赋给自己
	TargetASC->ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());
}
