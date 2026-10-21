#pragma once
#include "CoreMinimal.h"
#include "WuwaUiLayer.generated.h"


UENUM()
enum class EWuwaUiLayer : uint8
{
	HUD,
	Normal,
	Pop,
	Float,
	Guide,
	Loading,
	Debug,
	Count UMETA(Hidden)
};
