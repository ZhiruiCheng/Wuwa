#include "Editor/Commandlets/WuwaNotifyTriggerOffsetFixCommandlet.h"

#if WITH_EDITOR
#include "Animation/AnimSequenceBase.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor/WuwaAnimNotifyEditorUtils.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaNotifyOffsetFix, Log, All);

UWuwaNotifyTriggerOffsetFixCommandlet::UWuwaNotifyTriggerOffsetFixCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpUsage = TEXT("-run=WuwaNotifyTriggerOffsetFix [-Path=/Game/Characters] [-Apply]");
}

int32 UWuwaNotifyTriggerOffsetFixCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	FString Root = TEXT("/Game/Characters");
	FParse::Value(*Params, TEXT("Path="), Root);
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	Registry.SearchAllAssets(true);
	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*Root));
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UAnimSequenceBase::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	TArray<FAssetData> Assets;
	Registry.GetAssets(Filter, Assets);
	const FString BackupDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/NotifyTriggerOffsetFix") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	TArray<UPackage*> ToSave;
	int32 Changed = 0;
	for (const FAssetData& Data : Assets)
	{
		UAnimSequenceBase* Anim = Cast<UAnimSequenceBase>(Data.GetAsset());
		if (!Anim || Anim->Notifies.IsEmpty()) continue;
		TArray<TPair<float, float>> Before;
		for (const FAnimNotifyEvent& Notify : Anim->Notifies) Before.Emplace(Notify.TriggerTimeOffset, Notify.EndTriggerTimeOffset);
		WuwaAnimNotifyEditor::RefreshTriggerOffsets(Anim);
		int32 Local = 0;
		for (int32 Index = 0; Index < Anim->Notifies.Num(); ++Index)
		{
			const FAnimNotifyEvent& Notify = Anim->Notifies[Index];
			if (Notify.TriggerTimeOffset == Before[Index].Key && Notify.EndTriggerTimeOffset == Before[Index].Value) continue;
			++Local;
			UE_LOG(LogWuwaNotifyOffsetFix, Display, TEXT("  %s @ %.3fs %s: offset %g/%g -> %g/%g"), *Anim->GetName(), Notify.GetTime(), *Notify.NotifyName.ToString(),
				Before[Index].Key, Before[Index].Value, Notify.TriggerTimeOffset, Notify.EndTriggerTimeOffset);
		}
		if (Local == 0) continue;
		Changed += Local;
		if (!bApply) continue;
		const FString Filename = FPackageName::LongPackageNameToFilename(Anim->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		FString Relative = Anim->GetOutermost()->GetName();
		Relative.RemoveFromStart(TEXT("/Game/"));
		IFileManager::Get().Copy(*(BackupDir / Relative + FPackageName::GetAssetPackageExtension()), *Filename);
		Anim->RefreshCacheData();
		Anim->MarkPackageDirty();
		ToSave.Add(Anim->GetOutermost());
	}
	UE_LOG(LogWuwaNotifyOffsetFix, Display, TEXT("%d notifies on %d assets need new trigger offsets%s"), Changed, ToSave.Num(), bApply ? TEXT("") : TEXT(" (read-only; -Apply to write)"));
	if (bApply && !ToSave.IsEmpty() && !UEditorLoadingAndSavingUtils::SavePackages(ToSave, false))
	{
		UE_LOG(LogWuwaNotifyOffsetFix, Error, TEXT("Saving failed (is the editor open?)"));
		return 1;
	}
	return 0;
#else
	return 1;
#endif
}
