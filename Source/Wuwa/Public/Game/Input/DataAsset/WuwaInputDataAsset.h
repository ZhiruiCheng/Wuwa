// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "WuwaInputDataAsset.generated.h"


USTRUCT(BlueprintType)
struct FInputDataAsset
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = "Wuwa|Input")
	const class UInputAction* InputAction = nullptr;
	
	// 玩家想做什么
	UPROPERTY(EditAnywhere, Category = "Wuwa|Input")
	FGameplayTag InputTag;

	// 交给哪个系统
	UPROPERTY(EditAnywhere, Category = "Wuwa|Input")
	FGameplayTag RouteTag;

	/**
	 * 长按连续触发（原作 BP_InputBase 长按配置的"连续触发"）：按键配了 Hold 触发器时，到阈值后每帧都发一次 Held，
	 * 而不是每次按下只发一次。原作攻击键是连续触发：按住就每帧请求一次长按招，到可接的时机就接下一段（嘉贝重击 1-2-3 循环）。
	 */
	UPROPERTY(EditAnywhere, Category = "Wuwa|Input")
	bool bContinuousHold = false;
	
	bool IsConfigured() const
	{
		return InputAction != nullptr
			&& InputTag.IsValid()
			&& RouteTag.IsValid();
	}
};

/**
 * 
 */
UCLASS()
class WUWA_API UWuwaInputDataAsset : public UDataAsset
{
	GENERATED_BODY()
public:
	const UInputAction* MatchInputActionWithTag( FGameplayTag tag );
	
	FGameplayTag MatchTagWithInputAction( UInputAction* inputAction );
	
	UPROPERTY(EditAnywhere, Category = "Wuwa|Input")
	TArray<FInputDataAsset> InputDataAssetMap;
};
