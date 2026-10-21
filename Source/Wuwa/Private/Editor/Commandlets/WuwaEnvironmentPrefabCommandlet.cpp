#include "Editor/Commandlets/WuwaEnvironmentPrefabCommandlet.h"

#if WITH_EDITOR
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "LandscapeProxy.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaEnvironmentPrefab, Log, All);

UWuwaEnvironmentPrefabCommandlet::UWuwaEnvironmentPrefabCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpDescription = TEXT("Saves the environment of a region level (landscape + WuwaProps actors, no lighting/player start) as a reusable LI_ level for Level Instances. Read-only unless -Apply.");
	HelpUsage = TEXT("-run=WuwaEnvironmentPrefab (-Region=crownless | -Level=/Game/Environment/JiaoTing/Maps/L_JiaoTing) [-Apply] -AllowCommandletRendering");
}

int32 UWuwaEnvironmentPrefabCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	// 关卡来源：-Region=<地形配方名>（读 ArtSource/Terrain/<名>.terrain.json 的 level）或直接 -Level=<关卡包路径>（例如 WuwaSceneImport 导入的场景）
	FString Region, Source;
	FParse::Value(*Params, TEXT("Level="), Source);
	if (Source.IsEmpty() && (!FParse::Value(*Params, TEXT("Region="), Region) || Region.IsEmpty()))
	{
		UE_LOG(LogWuwaEnvironmentPrefab, Error, TEXT("Usage: %s"), *HelpUsage);
		return 1;
	}
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	if (Source.IsEmpty())
	{
		FString Text;
		TSharedPtr<FJsonObject> Recipe;
		const FString RecipePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("ArtSource/Terrain") / (Region + TEXT(".terrain.json")));
		if (!FFileHelper::LoadFileToString(Text, *RecipePath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Recipe) || !Recipe.IsValid())
		{
			UE_LOG(LogWuwaEnvironmentPrefab, Error, TEXT("Cannot read %s"), *RecipePath);
			return 1;
		}
		Source = Recipe->GetStringField(TEXT("level"));
	}
	FString ShortName = FPackageName::GetShortName(Source);
	ShortName.RemoveFromStart(TEXT("L_"));
	const FString Target = FPackageName::GetLongPackagePath(Source) / (TEXT("LI_") + ShortName);

	UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(Source);
	if (!World)
	{
		UE_LOG(LogWuwaEnvironmentPrefab, Error, TEXT("Cannot load %s"), *Source);
		return 1;
	}
	// 环境内容：Landscape（含代理）和物件 Actor；其余（光照、天空、雾、PlayerStart……）都去掉。世界设置、刷子等关卡自带的保留
	TArray<AActor*> Remove;
	int32 Kept = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor->IsA<AWorldSettings>() || Actor == World->GetDefaultBrush()) continue;
		const bool bEnvironment = Actor->IsA<ALandscapeProxy>() || Actor->Tags.Contains(TEXT("WuwaProps"));
		if (bEnvironment) ++Kept;
		else Remove.Add(Actor);
	}
	UE_LOG(LogWuwaEnvironmentPrefab, Display, TEXT("%s -> %s: keep %d environment actors, remove %d others."), *Source, *Target, Kept, Remove.Num());
	for (AActor* Actor : Remove) UE_LOG(LogWuwaEnvironmentPrefab, Display, TEXT("  remove %s (%s)"), *Actor->GetActorLabel(), *Actor->GetClass()->GetName());
	if (!bApply)
	{
		UE_LOG(LogWuwaEnvironmentPrefab, Display, TEXT("Read-only; pass -Apply to write."));
		return 0;
	}
	const FString TargetFile = FPackageName::LongPackageNameToFilename(Target, FPackageName::GetMapPackageExtension());
	if (IFileManager::Get().FileExists(*TargetFile))
	{
		const FString BackupDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/EnvironmentPrefab") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
		IFileManager::Get().Copy(*(BackupDir / FPaths::GetCleanFilename(TargetFile)), *TargetFile);
	}
	for (AActor* Actor : Remove) World->DestroyActor(Actor);
	// 另存为新关卡；源关卡文件不变（没有保存它）
	if (!UEditorLoadingAndSavingUtils::SaveMap(World, Target))
	{
		UE_LOG(LogWuwaEnvironmentPrefab, Error, TEXT("Failed to save %s"), *Target);
		return 1;
	}
	UE_LOG(LogWuwaEnvironmentPrefab, Display, TEXT("Saved %s"), *Target);
	return 0;
#else
	return 1;
#endif
}
