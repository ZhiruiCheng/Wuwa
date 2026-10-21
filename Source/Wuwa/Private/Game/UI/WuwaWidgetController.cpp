// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/UI/WuwaWidgetController.h"

#include "Core/Utilities/DebugHelper.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"

void UWuwaWidgetController::InitWuwaUIController(FWuwaUIControllerParams params)
{
	ASC = params.ASC;
	AS = params.AS;
	PlayerController = params.PlayerController;
}

void UWuwaWidgetController::BroadInitialValues()
{
	onHealthChanged.Broadcast(AS->GetHealth());
	onMaxHealthChanged.Broadcast(AS->GetMaxHealth());
}

void UWuwaWidgetController::BindCallBackDependencies()
{
	//Health
	ASC->GetGameplayAttributeValueChangeDelegate(AS->GetHealthAttribute()).AddLambda(
			[this](const FOnAttributeChangeData& Data)
			{
				onHealthChanged.Broadcast(Data.NewValue);
			}
		);
	
	//MaxHealth
	ASC->GetGameplayAttributeValueChangeDelegate(AS->GetMaxHealthAttribute()).AddLambda(
			[this](const FOnAttributeChangeData& Data)
			{
				onMaxHealthChanged.Broadcast(Data.NewValue);
			}
		);
	
	//这里注意This传入,只有传入了才能拿到信息
	ASC->AttributeEffectAppliedDelegate.AddLambda(
		[this](const FGameplayEffectSpec& EffectSpec)
		{
			FGameplayTagContainer AssetTags;
			FGameplayTagContainer GrantedTags;
			EffectSpec.GetAllAssetTags(AssetTags);
			EffectSpec.GetAllGrantedTags(GrantedTags);

			for (const FGameplayTag& Tag : AssetTags)
			{
				if (const FWidgetControllerTable* RowMsg =
					GetEffectUIMsgFromGETags<FWidgetControllerTable>(MessageTable, Tag))
				{
					Debug::Print(RowMsg->Message.ToString());
				}
			}

			for (const FGameplayTag& Tag : GrantedTags)
			{
				if (const FWidgetControllerTable* RowMsg =
					GetEffectUIMsgFromGETags<FWidgetControllerTable>(MessageTable, Tag))
				{
					Debug::Print(RowMsg->Message.ToString());
				}
			}
		}
	);
}
