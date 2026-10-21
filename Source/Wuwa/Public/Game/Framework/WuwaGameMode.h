// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WuwaGameMode.generated.h"

/**
 * 
 */
UCLASS()
class WUWA_API AWuwaGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	virtual  void RestartPlayer(AController* NewPlayer) override;
	
};
