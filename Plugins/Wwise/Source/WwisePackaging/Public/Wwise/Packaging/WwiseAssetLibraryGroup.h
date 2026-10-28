/*******************************************************************************
The content of this file includes portions of the proprietary AUDIOKINETIC Wwise
Technology released in source code form as part of the game integration package.
The content of this file may not be used without valid licenses to the
AUDIOKINETIC Wwise Technology.
Note that the use of the game engine is subject to the Unreal(R) Engine End User
License Agreement at https://www.unrealengine.com/en-US/eula/unreal
 
License Usage
 
Licensees holding valid licenses to the AUDIOKINETIC Wwise Technology may use
this file in accordance with the end user license agreement provided with the
software or, alternatively, in accordance with the terms contained
in a written agreement between you and Audiokinetic Inc.
Copyright (c) 2026 Audiokinetic Inc.
*******************************************************************************/

#pragma once
#include "Wwise/Packaging/WwiseAssetLibrary.h"
#include "WwiseAssetLibraryGroup.generated.h"

// Prioritized list of all Wwise Asset Libraries to include when packaging its Gameplay Module's Wwise Assets.
// If no values are set or if they fall through, shared Wwise Assets are cooked as Additional Data.
UCLASS(BlueprintType, Category="Wwise", DisplayName="Wwise Asset Library Filter Group", EditInlineNew)
class WWISEPACKAGING_API UWwiseAssetLibraryGroup : public UObject
{
	GENERATED_BODY()
	
public:

	UWwiseAssetLibraryGroup() {}
	virtual ~UWwiseAssetLibraryGroup() override{}

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//Prioritized list of all Wwise Libraries to include when packaging Wwise assets as Bulk Data.
	//If no value is set or if they fall through, shared Wwise asset files are cooked as Additional Data.
	UPROPERTY(EditAnywhere, Category = "Default")
	TArray<TSoftObjectPtr<UWwiseAssetLibrary>> Libraries;
};
