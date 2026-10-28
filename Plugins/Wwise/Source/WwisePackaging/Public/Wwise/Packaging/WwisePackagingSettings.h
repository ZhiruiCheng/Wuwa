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
#include "WwiseAssetLibraryGroup.h"

#include "WwisePackagingSettings.generated.h"

class UWwiseAssetLibrary;

UCLASS(config = Game, defaultconfig)
class WWISEPACKAGING_API UWwisePackagingSettings : public UObject
{
	GENERATED_BODY()

public:
	static UWwisePackagingSettings* Get();
	
	UWwisePackagingSettings() {}
	virtual ~UWwisePackagingSettings() override {}

	//Determines whether the files are written as loose files in the packages, or inside the UAssets as Bulk Data.
	UPROPERTY(Config, EditAnywhere, Category = "Cooking", DisplayName = "Package as Bulk Data")
	bool bPackageAsBulkData{ true };

	UPROPERTY(Transient)
	mutable TArray<TObjectPtr<UWwiseAssetLibrary>> AssetLibrariesKeepAlive;
	UPROPERTY(Transient)
	mutable TArray<TSoftObjectPtr<UWwiseAssetLibraryGroup>> AssetLibraryGroupsToKeepAlive;


	// Template Library Group can be used when you create Modular Gameplay. By default, the template does nothing.
	UPROPERTY(Config, EditAnywhere, Category = "Cooking", meta=(EditCondition="bPackageAsBulkData", EditConditionHides=true))
	TSoftObjectPtr<UWwiseAssetLibraryGroup> TemplateAssetLibraryGroup;

	UPROPERTY(Config, VisibleAnywhere, Category = "GroupPreview", meta=(EditCondition="bPackageAsBulkData", EditConditionHides=true))
	TArray<FString> GameplayModuleWithoutGroups;


#if WITH_EDITORONLY_DATA
	//Editor: If we are enabling the Bulk Data for the first time, we should create a default Shared Asset library. 
	UPROPERTY(Config)
	bool bInitialAssetLibraryCreated{ false };

	//Used to track whether Library Groups exist after migrating the integration
	UPROPERTY(Config)
	bool bMigratedToLibraryGroup = false;
#endif

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSettingsChanged, UWwisePackagingSettings*);
#if WITH_EDITOR
	/**
	 * Called when the settings for Asset Libraries are changed.
	 *
	 * This is the equivalent of an UObject-specific FCoreUObjectDelegates::OnObjectPropertyChanged. 
	 */
	FOnSettingsChanged OnSettingsChanged;

	void ProcessGameplayModulesUsingDefaultGroup();
	virtual void CreateInitialAssetLibraryGroups();
	virtual bool ShouldPromptInitialAssetLibraryGroups() const;
	bool SaveConfigFile();
#endif

#if WITH_EDITOR
protected:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostInitProperties() override;

	virtual bool EnsurePostEngineInit();

	void GetGameplayModuleUsingDefaultGroup(TArray<FString>& OutGameplayModuleUsedInDefaultGroup);
#endif
};
