// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Asset/WuwaAssetManager.h"

#include "Core/Tag/WuwaGameTags.h"

UWuwaAssetManager& UWuwaAssetManager::Get()
{
	UAssetManager& AssetManager = UAssetManager::Get();

	UWuwaAssetManager* WuwaAssetManager =Cast<UWuwaAssetManager>(&AssetManager);

	checkf(WuwaAssetManager,TEXT("Asset Manager 配置错误，请将 AssetManagerClassName 设置为 UWuwaAssetManager"));

	return *WuwaAssetManager;
}

void UWuwaAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();
	
	FWuwaGameTags::InitializeGameTags();
}


