// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "WuwaWidgetController.generated.h"

/**
 * 
 */


class AWuwaPlayerState;
class AWuwaPlayerController;
class UWuwaAbilitySystemComponent;
class UWuwaAttributeSet;
class UTexture2D;
class UUserWidget;

struct FWuwaUIControllerParams
{
	TObjectPtr<UWuwaAbilitySystemComponent> ASC;
	TObjectPtr<UWuwaAttributeSet> AS;
	TObjectPtr<AWuwaPlayerController> PlayerController;
	FWuwaUIControllerParams(UWuwaAbilitySystemComponent* asc,UWuwaAttributeSet* as,AWuwaPlayerController* playerController):
	ASC(asc),AS(as),PlayerController(playerController)
	{
		
	}
};

USTRUCT(BlueprintType)
struct WUWA_API FWidgetControllerTable: public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag MessageTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Message;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UUserWidget> MessageWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UTexture2D> Image = nullptr;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAttributeChangeDelegate,float,newValue);

UCLASS(BlueprintType, Blueprintable)
class WUWA_API UWuwaWidgetController : public UObject
{
	GENERATED_BODY()
public:
	TObjectPtr<UWuwaAbilitySystemComponent> ASC;
	TObjectPtr<UWuwaAttributeSet> AS;
	TObjectPtr<AWuwaPlayerController> PlayerController;
	
	
	virtual void InitWuwaUIController(FWuwaUIControllerParams params);
	virtual void BroadInitialValues();
	
	//delegate
	UPROPERTY(BlueprintAssignable)
	FAttributeChangeDelegate onHealthChanged;
	UPROPERTY(BlueprintAssignable)
	FAttributeChangeDelegate onMaxHealthChanged;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UDataTable> MessageTable;
	
	UFUNCTION()
	void BindCallBackDependencies(); 
	
	template<typename T>
	T* GetEffectUIMsgFromGETags(UDataTable* MsgTable,const FGameplayTag& Tag);
};

template<typename T>
T* UWuwaWidgetController::GetEffectUIMsgFromGETags(UDataTable* MsgTable, const FGameplayTag& Tag)
{
	if (!IsValid(MsgTable) || !Tag.IsValid())
	{
		return nullptr;
	}

	return MsgTable->FindRow<T>(Tag.GetTagName(), TEXT("GetEffectUIMsgFromGETags"));
}
