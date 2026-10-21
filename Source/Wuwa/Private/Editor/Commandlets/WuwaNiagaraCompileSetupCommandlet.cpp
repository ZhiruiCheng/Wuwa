#include "Editor/Commandlets/WuwaNiagaraCompileSetupCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NiagaraSystem.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaNiagaraCompile, Log, All);

UWuwaNiagaraCompileSetupCommandlet::UWuwaNiagaraCompileSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpUsage = TEXT("-run=WuwaNiagaraCompileSetup [-Paths=/Game/Effects,/Game/LGUI] [-Apply]");
}

int32 UWuwaNiagaraCompileSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	FString PathsText = TEXT("/Game");
	FParse::Value(*Params, TEXT("Paths="), PathsText, false);
	TArray<FString> PathStrings;
	PathsText.ParseIntoArray(PathStrings, TEXT(","));

	IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();
	Registry.SearchAllAssets(true);
	FARFilter Filter;
	Filter.ClassPaths.Add(UNiagaraSystem::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	for (const FString& Path : PathStrings) Filter.PackagePaths.Add(FName(*Path.TrimStartAndEnd()));
	TArray<FAssetData> Assets;
	Registry.GetAssets(Filter, Assets);

	const FString BackupDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/NiagaraCompileSetup") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	int32 Stale = 0, Errors = 0;
	for (const FAssetData& Asset : Assets)
	{
		UNiagaraSystem* System = Cast<UNiagaraSystem>(Asset.GetAsset());
		if (!System) continue;
		// 加载时发现过期会标记待编译；RequestCompile 只有在确实需要时才启动编译（返回 true）
		const bool bPending = System->NeedsRequestCompile();
		const bool bLaunched = System->RequestCompile(false);
		System->WaitForCompilationComplete(true, false);
		if (!bPending && !bLaunched) continue;
		++Stale;
		UE_LOG(LogWuwaNiagaraCompile, Display, TEXT("  %s: compiled data was out of date%s"), *Asset.PackageName.ToString(), System->IsReadyToRun() ? TEXT("") : TEXT(" (still not ready to run!)"));
		if (!bApply) continue;
		const FString Filename = FPackageName::LongPackageNameToFilename(Asset.PackageName.ToString(), FPackageName::GetAssetPackageExtension());
		if (IFileManager::Get().Copy(*(BackupDir / FPaths::GetCleanFilename(Filename)), *Filename) != COPY_OK)
		{
			UE_LOG(LogWuwaNiagaraCompile, Error, TEXT("Failed to back up %s"), *Filename);
			++Errors;
			continue;
		}
		UPackage* Package = System->GetOutermost();
		Package->MarkPackageDirty();
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		Args.Error = GWarn;
		if (!UPackage::SavePackage(Package, System, *Filename, Args))
		{
			UE_LOG(LogWuwaNiagaraCompile, Error, TEXT("Failed to save %s (is the editor open?)"), *Filename);
			++Errors;
		}
	}
	UE_LOG(LogWuwaNiagaraCompile, Display, TEXT("%d Niagara systems under %s, %d out of date%s"), Assets.Num(), *PathsText, Stale,
		bApply ? *FString::Printf(TEXT(" (saved, backups in %s)"), *BackupDir) : TEXT(" — read-only, pass -Apply to save"));
	return Errors > 0 ? 1 : 0;
#else
	return 1;
#endif
}
