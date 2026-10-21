#include "Game/Module/Hud/WuwaMainHudController.h"

#include "GameFramework/PlayerController.h"
#include "Game/Formation/WuwaFormationComponent.h"
#include "Game/LGUI/Core/WuwaUiLog.h"
#include "Game/LGUI/Core/WuwaUiManager.h"

const FName UWuwaMainHudController::MainHudViewName(TEXT("MainHUD"));

UWuwaUiView* UWuwaMainHudController::OpenMainHud()
{
	UWuwaUiManager* UiManager = GetUiManager();
	if (!UiManager)
	{
		return nullptr;
	}
	if (UWuwaUiView* Existing = UiManager->GetView(MainHudViewName))
	{
		return Existing;
	}
	UE_LOG(LogWuwaUI, Log, TEXT("MainHud: open %s"), *MainHudViewName.ToString());
	return UiManager->OpenView(MainHudViewName);
}

bool UWuwaMainHudController::ShouldShowMainHud(const APlayerController* PlayerController)
{
	return PlayerController && PlayerController->IsLocalController()
		&& PlayerController->FindComponentByClass<UWuwaFormationComponent>() != nullptr;
}
