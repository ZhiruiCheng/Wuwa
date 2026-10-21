#include "Commandlets/WuwaPlayEffectRescaleCommandlet.h"

#include "Animation/AnimMontage.h"
#include "FileHelpers.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_PlayEffect.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaPlayEffectRescale, Log, All);

UWuwaPlayEffectRescaleCommandlet::UWuwaPlayEffectRescaleCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UWuwaPlayEffectRescaleCommandlet::Main(const FString& Params)
{
	FString MontagesText;
	FParse::Value(*Params, TEXT("Montages="), MontagesText, false);
	float Factor = 100.f;
	FParse::Value(*Params, TEXT("Factor="), Factor);
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	TArray<FString> Montages;
	MontagesText.ParseIntoArray(Montages, TEXT(";"));

	const FString BackupRoot = FPaths::ProjectSavedDir() / TEXT("Backups/PlayEffectRescale") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
	TArray<UPackage*> ToSave;
	for (const FString& Path : Montages)
	{
		UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *Path);
		if (!Montage)
		{
			UE_LOG(LogWuwaPlayEffectRescale, Error, TEXT("Missing %s"), *Path);
			return 1;
		}
		bool bChanged = false;
		for (const FAnimNotifyEvent& Event : Montage->Notifies)
		{
			auto* Notify = Cast<UWuwaAnimNotify_PlayEffect>(Event.Notify.Get());
			if (!Notify) continue;
			UE_LOG(LogWuwaPlayEffectRescale, Display, TEXT("%s %.3fs %s follow=%d loc=%s scale=%s"), *Montage->GetName(), Event.GetTime(),
				*Event.NotifyName.ToString(), Notify->bFollow, *Notify->LocationOffset.ToString(), *Notify->Scale.ToString());
			if (bApply)
			{
				Notify->Modify();
				Notify->LocationOffset *= Factor;
				Notify->Scale *= Factor;
				bChanged = true;
			}
		}
		if (bChanged)
		{
			const FString Package = Montage->GetOutermost()->GetName();
			IFileManager::Get().Copy(*(BackupRoot / Package.RightChop(6) + FPackageName::GetAssetPackageExtension()),
				*FPackageName::LongPackageNameToFilename(Package, FPackageName::GetAssetPackageExtension()));
			Montage->MarkPackageDirty();
			ToSave.Add(Montage->GetOutermost());
		}
	}
	if (!ToSave.IsEmpty())
	{
		if (!UEditorLoadingAndSavingUtils::SavePackages(ToSave, false)) return 1;
		UE_LOG(LogWuwaPlayEffectRescale, Display, TEXT("Scaled by %.1f and saved %d montages (backup %s)"), Factor, ToSave.Num(), *BackupRoot);
	}
	return 0;
}
