// Zacray All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LGUIComponentReference.h"
#include "Game/LGUI/Layer/WuwaUiLayer.h"
#include "WuwaUiLayerRoot.generated.h"


class UUIItem;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class WUWA_API UWuwaUiLayerRoot : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UWuwaUiLayerRoot();
	UUIItem* GetLayerItem(EWuwaUiLayer Layer) const;
protected:

	UPROPERTY(EditAnywhere, EditFixedSize, Category = "Wuwa UI")
	TArray<FLGUIComponentReference> Layers;

		
};
