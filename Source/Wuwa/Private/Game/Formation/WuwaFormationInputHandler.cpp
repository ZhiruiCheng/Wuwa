#include "Game/Formation/WuwaFormationInputHandler.h"

#include "Core/Tag/WuwaGameTags.h"
#include "Game/Formation/WuwaFormationComponent.h"
#include "Game/Input/WuwaInputTypes.h"

UWuwaFormationInputHandler::UWuwaFormationInputHandler()
{
	PrimaryComponentTick.bCanEverTick = false;
}

int32 UWuwaFormationInputHandler::GetMemberIndexForInput(const FGameplayTag& InputTag)
{
	const FWuwaGameTags& GameTags = FWuwaGameTags::Get();
	if (InputTag == GameTags.Formation_SwitchtoOne) return 0;
	if (InputTag == GameTags.Formation_SwitchtoTwo) return 1;
	if (InputTag == GameTags.Formation_SwitchtoThree) return 2;
	return INDEX_NONE;
}

bool UWuwaFormationInputHandler::HandleWuwaInput_Implementation(const FWuwaInputEvent& InputEvent)
{
	const int32 Index = GetMemberIndexForInput(InputEvent.InputTag);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	// 切人只在按下那一刻发生一次；按住不放不会连续切。
	if (InputEvent.Phase == EWuwaInputPhase::Pressed)
	{
		if (UWuwaFormationComponent* Formation = GetFormation())
		{
			Formation->RequestSwitch(Index);
		}
	}
	return true;
}

UWuwaFormationComponent* UWuwaFormationInputHandler::GetFormation() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UWuwaFormationComponent>() : nullptr;
}
