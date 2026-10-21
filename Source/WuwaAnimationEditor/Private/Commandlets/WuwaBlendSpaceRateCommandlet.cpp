#include "Commandlets/WuwaBlendSpaceRateCommandlet.h"

#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaBlendSpaceRate, Log, All);

UWuwaBlendSpaceRateCommandlet::UWuwaBlendSpaceRateCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UWuwaBlendSpaceRateCommandlet::Main(const FString& Params)
{
	FString Path;
	FString Folder;
	float Rate = 0.f;
	float From = 0.f;
	FParse::Value(*Params, TEXT("BlendSpace="), Path, false);
	FParse::Value(*Params, TEXT("Folder="), Folder, false);
	FString Contains;
	FParse::Value(*Params, TEXT("Contains="), Contains, false);
	const bool bHasRate = FParse::Value(*Params, TEXT("Rate="), Rate);
	const bool bHasFrom = FParse::Value(*Params, TEXT("From="), From);
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	const FString BackupRoot = FPaths::ProjectSavedDir() / TEXT("Backups/BlendSpaceRate") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
	auto Backup = [&BackupRoot](UPackage* Package)
	{
		const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		return IFileManager::Get().Copy(*(BackupRoot / Package->GetName().RightChop(6) + FPackageName::GetAssetPackageExtension()), *File) == COPY_OK;
	};

	if (!Folder.IsEmpty())
	{
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		Registry.ScanPathsSynchronous({ Folder }, true);
		TArray<FAssetData> Assets;
		Registry.GetAssetsByClass(UAnimSequence::StaticClass()->GetClassPathName(), Assets);
		TArray<UPackage*> ToSave;
		for (const FAssetData& Asset : Assets)
		{
			if (!Asset.PackageName.ToString().StartsWith(Folder) || !Asset.AssetName.ToString().Contains(Contains)) continue;
			UAnimSequence* Sequence = Cast<UAnimSequence>(Asset.GetAsset());
			if (!Sequence || FMath::IsNearlyEqual(Sequence->RateScale, 1.f)) continue;
			const bool bChange = bHasFrom && bHasRate && Rate > 0.f && FMath::IsNearlyEqual(Sequence->RateScale, From);
			UE_LOG(LogWuwaBlendSpaceRate, Display, TEXT("%s: %.3fs, %s fps, RateScale %.2f%s"), *Sequence->GetPathName(), Sequence->GetPlayLength(),
				*Sequence->GetSamplingFrameRate().ToPrettyText().ToString(), Sequence->RateScale, bChange ? *FString::Printf(TEXT(" -> %.2f"), Rate) : TEXT(""));
			if (!bChange || !bApply) continue;
			if (!Backup(Sequence->GetOutermost()))
			{
				UE_LOG(LogWuwaBlendSpaceRate, Error, TEXT("备份 %s 失败"), *Sequence->GetPathName());
				return 1;
			}
			Sequence->Modify();
			Sequence->RateScale = Rate;
			Sequence->MarkPackageDirty();
			ToSave.Add(Sequence->GetOutermost());
		}
		if (ToSave.IsEmpty()) return 0;
		UE_LOG(LogWuwaBlendSpaceRate, Display, TEXT("已备份到 %s，改了 %d 个动画序列"), *BackupRoot, ToSave.Num());
		return UEditorLoadingAndSavingUtils::SavePackages(ToSave, false) ? 0 : 1;
	}
	if (Path.IsEmpty() || !bHasRate || Rate <= 0.f)
	{
		UE_LOG(LogWuwaBlendSpaceRate, Error, TEXT("需要 -BlendSpace=<路径> -Rate=<大于 0 的倍率>，或 -Folder=<目录>"));
		return 1;
	}
	UBlendSpace* Space = LoadObject<UBlendSpace>(nullptr, *Path);
	if (!Space)
	{
		UE_LOG(LogWuwaBlendSpaceRate, Error, TEXT("找不到混合空间 %s"), *Path);
		return 1;
	}

	// SampleData 是 private，按反射改（和编辑器细节面板改值走同一个属性）。
	FArrayProperty* SamplesProperty = CastField<FArrayProperty>(UBlendSpace::StaticClass()->FindPropertyByName(TEXT("SampleData")));
	if (!SamplesProperty)
	{
		UE_LOG(LogWuwaBlendSpaceRate, Error, TEXT("UBlendSpace 没有 SampleData 属性"));
		return 1;
	}
	TArray<FBlendSample>& Samples = *SamplesProperty->ContainerPtrToValuePtr<TArray<FBlendSample>>(Space);
	int32 Changed = 0;
	for (const FBlendSample& Sample : Samples)
	{
		const bool bChange = !FMath::IsNearlyEqual(Sample.RateScale, Rate);
		Changed += bChange;
		UE_LOG(LogWuwaBlendSpaceRate, Display, TEXT("(%.0f, %.0f) %s %.3fs: RateScale %.2f%s"), Sample.SampleValue.X, Sample.SampleValue.Y,
			*GetNameSafe(Sample.Animation), Sample.Animation ? Sample.Animation->GetPlayLength() : 0.f, Sample.RateScale,
			bChange ? *FString::Printf(TEXT(" -> %.2f"), Rate) : TEXT(""));
	}
	if (!bApply || Changed == 0)
	{
		UE_LOG(LogWuwaBlendSpaceRate, Display, TEXT("%d 个采样需要修改%s"), Changed, bApply ? TEXT("") : TEXT("（预览，加 -Apply 才保存）"));
		return 0;
	}

	UPackage* Package = Space->GetOutermost();
	if (!Backup(Package))
	{
		UE_LOG(LogWuwaBlendSpaceRate, Error, TEXT("备份 %s 失败"), *Package->GetName());
		return 1;
	}
	Space->Modify();
	FPropertyChangedEvent Event(SamplesProperty, EPropertyChangeType::ValueSet);
	Space->PreEditChange(SamplesProperty);
	for (FBlendSample& Sample : Samples)
	{
		Sample.RateScale = Rate;
	}
	Space->PostEditChangeProperty(Event);
	Space->MarkPackageDirty();
	UE_LOG(LogWuwaBlendSpaceRate, Display, TEXT("已备份到 %s，改了 %d 个采样"), *BackupRoot, Changed);
	return UEditorLoadingAndSavingUtils::SavePackages({ Package }, false) ? 0 : 1;
}
