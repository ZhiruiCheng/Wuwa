// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/UI/WuwaHUD.h"

#include "Game/Framework/WuwaPlayerState.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/UI/WuwaWidgetController.h"

void AWuwaHUD::InitCtrAndWidget(UWuwaAbilitySystemComponent* asc,UWuwaAttributeSet* as,AWuwaPlayerController* playerController)
{
	checkf(WuwaUserWidgetClass, TEXT("WuwaUserWidgetClass 未在 BP_WuwaHUD 中设置"));
	checkf(WuwaUIControllerClass, TEXT("WuwaUIControllerClass 未在 BP_WuwaHUD 中设置"));
	//Get WuwaWidget
	WuwaUserWidget = Cast<UWuwaUserWidget>(CreateWidget<UUserWidget>(GetWorld(),WuwaUserWidgetClass));
	
	//Create
	FWuwaUIControllerParams WuwaUIControllerParams(asc,as,playerController);
	WuwaWidgetController =NewObject<UWuwaWidgetController>(this,WuwaUIControllerClass);
	WuwaWidgetController->InitWuwaUIController(WuwaUIControllerParams);
	
	WuwaUserWidget->InitWidgetController(WuwaWidgetController);
	
	WuwaUserWidget->AddToViewport();
}
