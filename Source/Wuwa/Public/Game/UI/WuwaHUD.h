// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Game/UI/WuwaUserWidget.h"
#include "GameFramework/HUD.h"
#include "WuwaHUD.generated.h"

class UWuwaAttributeSet;
class UWuwaAbilitySystemComponent;
class AWuwaPlayerState;
class AWuwaPlayerController;
class UAttributeSet;
class UWuwaWidgetController;
class UAbilitySystemComponent;
/**
 * 
 */
UCLASS()
class WUWA_API AWuwaHUD : public AHUD
{
	GENERATED_BODY()
public:
	
	void InitCtrAndWidget(UWuwaAbilitySystemComponent* asc,UWuwaAttributeSet* as,AWuwaPlayerController* playerController);
	
	UPROPERTY()
	TObjectPtr<UWuwaWidgetController> WuwaWidgetController;
	UPROPERTY()
	TObjectPtr<UWuwaUserWidget> WuwaUserWidget;
	
	UPROPERTY(EditAnywhere,Category="WuwaHUD")
	TSubclassOf<UWuwaUserWidget> WuwaUserWidgetClass;
	UPROPERTY(EditAnywhere,Category="WuwaHUD")
	TSubclassOf<UWuwaWidgetController> WuwaUIControllerClass;
};
