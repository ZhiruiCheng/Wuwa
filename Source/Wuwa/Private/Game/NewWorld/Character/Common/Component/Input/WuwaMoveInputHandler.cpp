// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/NewWorld/Character/Common/Component/Input/WuwaMoveInputHandler.h"

#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaMoveInputConfig.h"
#include "GameFramework/PlayerController.h"
#include "Game/Input/WuwaInputTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaMoveInput, Log, All);

UWuwaMoveInputHandler::UWuwaMoveInputHandler()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UWuwaMoveInputHandler::HandleWuwaInput_Implementation(const FWuwaInputEvent& InputEvent)
{
	const UWuwaMoveInputConfig* LoadedConfig = Config.LoadSynchronous();
	if (!LoadedConfig)
	{
		if (!bReportedMissingConfig)
		{
			UE_LOG(LogWuwaMoveInput, Warning, TEXT("Move input config is missing: %s"), *Config.ToString());
			bReportedMissingConfig = true;
		}
		return false;
	}
	const FWuwaMoveInputBinding* Binding = LoadedConfig->FindBinding(InputEvent.InputTag, InputEvent.Phase);
	if (!Binding || !Binding->Action)
	{
		return false;
	}

	// 每次都取当前控制的 Pawn；切人后不能继续操作旧角色。
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	FWuwaMoveInputContext Context;
	Context.InputEvent = InputEvent;
	Context.Character = PC ? Cast<AWuwaCharacter>(PC->GetPawn()) : nullptr;
	return IsValid(Context.Character) && Binding->Action->Execute(Context);
}
