#include "Commandlets/WuwaAnimRetimeFixCommandlet.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaAnimRetime, Log, All);

namespace WuwaAnimRetime
{
	const TCHAR* MountRoot = TEXT("/WuwaBackup/");

	void Retime(FAnimNotifyEvent& Notify, const FAnimNotifyEvent& Original, UAnimSequence* Target, float Ratio)
	{
		const float Time = Original.GetTime() * Ratio;
		Notify.Link(Target, Time);
		Notify.TriggerTimeOffset = GetTriggerTimeOffsetForType(Target->CalculateOffsetForNotify(Time));
		if (Original.GetDuration() > 0.f)
		{
			const float Duration = Original.GetDuration() * Ratio;
			Notify.SetDuration(Duration);
			Notify.EndLink.Link(Target, Time + Duration);
			Notify.EndTriggerTimeOffset = GetTriggerTimeOffsetForType(Target->CalculateOffsetForNotify(Time + Duration));
		}
	}

	TArray<FRichCurveKey> ScaleKeys(const TArray<FRichCurveKey>& Keys, float Ratio)
	{
		TArray<FRichCurveKey> Scaled = Keys;
		for (FRichCurveKey& Key : Scaled)
		{
			Key.Time *= Ratio;
			// 切线是每秒的斜率，时间压缩后斜率按反比放大
			Key.ArriveTangent /= Ratio;
			Key.LeaveTangent /= Ratio;
		}
		return Scaled;
	}
}

UWuwaAnimRetimeFixCommandlet::UWuwaAnimRetimeFixCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UWuwaAnimRetimeFixCommandlet::Main(const FString& Params)
{
	using namespace WuwaAnimRetime;

	FString Backup;
	FParse::Value(*Params, TEXT("Backup="), Backup);
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	FPaths::NormalizeDirectoryName(Backup);
	if (!IFileManager::Get().DirectoryExists(*Backup))
	{
		UE_LOG(LogWuwaAnimRetime, Error, TEXT("需要 -Backup=<导入前的备份目录>（当前 %s）"), *Backup);
		return 1;
	}
	FPackageName::RegisterMountPoint(MountRoot, Backup / TEXT(""));

	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *Backup, TEXT("*.uasset"), true, false);
	Files.Sort();

	TArray<FString> Lines;
	TArray<UPackage*> ToSave;
	int32 Problems = 0;
	for (const FString& File : Files)
	{
		FString Relative = File.RightChop(Backup.Len() + 1);
		Relative = FPaths::ChangeExtension(Relative, TEXT("")).Replace(TEXT("\\"), TEXT("/"));
		const FString Name = FPaths::GetBaseFilename(File);
		UAnimSequence* Original = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%s%s.%s"), MountRoot, *Relative, *Name));
		UAnimSequence* Target = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("/Game/%s.%s"), *Relative, *Name));
		if (!Original || !Target)
		{
			Lines.Add(FString::Printf(TEXT("MISSING %s (original=%d target=%d)"), *Relative, Original != nullptr, Target != nullptr));
			++Problems;
			continue;
		}

		const float OldLength = Original->GetPlayLength();
		const float NewLength = Target->GetPlayLength();
		const float Ratio = OldLength > 0.f ? NewLength / OldLength : 1.f;

		// 现在资产上的通知 / 标记是否还是原样（没被改过），是才按比例缩放
		bool bNotifiesSame = Target->Notifies.Num() == Original->Notifies.Num();
		for (int32 I = 0; bNotifiesSame && I < Original->Notifies.Num(); ++I)
		{
			bNotifiesSame = FMath::IsNearlyEqual(Target->Notifies[I].GetTime(), Original->Notifies[I].GetTime(), 1e-3f)
				&& Target->Notifies[I].NotifyName == Original->Notifies[I].NotifyName;
		}
		bool bMarkersSame = Target->AuthoredSyncMarkers.Num() == Original->AuthoredSyncMarkers.Num();
		for (int32 I = 0; bMarkersSame && I < Original->AuthoredSyncMarkers.Num(); ++I)
		{
			bMarkersSame = FMath::IsNearlyEqual(Target->AuthoredSyncMarkers[I].Time, Original->AuthoredSyncMarkers[I].Time, 1e-3f)
				&& Target->AuthoredSyncMarkers[I].MarkerName == Original->AuthoredSyncMarkers[I].MarkerName;
		}
		TArray<const FFloatCurve*> MissingCurves;
		const IAnimationDataModel* TargetModel = Target->GetDataModel();
		for (const FFloatCurve& Curve : Original->GetDataModel()->GetFloatCurves())
		{
			if (!TargetModel->FindFloatCurve(FAnimationCurveIdentifier(Curve.GetName(), ERawCurveTrackTypes::RCT_Float)))
			{
				MissingCurves.Add(&Curve);
			}
		}

		FString MissingNames;
		for (const FFloatCurve* Curve : MissingCurves)
		{
			MissingNames += Curve->GetName().ToString() + TEXT(" ");
		}
		Lines.Add(FString::Printf(TEXT("%-110s %.3fs -> %.3fs x%.4f  notifies=%d(%s) markers=%d(%s) missingCurves=%d %s"),
			*Relative, OldLength, NewLength, Ratio,
			Original->Notifies.Num(), bNotifiesSame ? TEXT("old-times") : TEXT("CHANGED"),
			Original->AuthoredSyncMarkers.Num(), bMarkersSame ? TEXT("old-times") : TEXT("CHANGED"),
			MissingCurves.Num(), *MissingNames));
		if (!bNotifiesSame || !bMarkersSame)
		{
			FString Detail = TEXT("    original:");
			for (const FAnimSyncMarker& M : Original->AuthoredSyncMarkers) Detail += FString::Printf(TEXT(" %s@%.3f"), *M.MarkerName.ToString(), M.Time);
			for (const FAnimNotifyEvent& N : Original->Notifies) Detail += FString::Printf(TEXT(" N:%s@%.3f+%.3f"), *N.NotifyName.ToString(), N.GetTime(), N.GetDuration());
			Detail += TEXT(" | now:");
			for (const FAnimSyncMarker& M : Target->AuthoredSyncMarkers) Detail += FString::Printf(TEXT(" %s@%.3f"), *M.MarkerName.ToString(), M.Time);
			for (const FAnimNotifyEvent& N : Target->Notifies) Detail += FString::Printf(TEXT(" N:%s@%.3f+%.3f"), *N.NotifyName.ToString(), N.GetTime(), N.GetDuration());
			Lines.Add(Detail);
		}
		// 数据替换时引擎会把标记 / 通知挪到末尾（实测 Run_F 的 Right@0.417 变成 0.733），所以不看现在的值，
		// 一律按「原始时间 × 比例」重设——重复运行结果也一样，不会二次缩放。
		bool bNotifiesMatch = Target->Notifies.Num() == Original->Notifies.Num();
		for (int32 I = 0; bNotifiesMatch && I < Original->Notifies.Num(); ++I)
		{
			bNotifiesMatch = Target->Notifies[I].NotifyName == Original->Notifies[I].NotifyName;
		}
		if (!bNotifiesMatch)
		{
			Lines.Add(TEXT("    !! 通知数量或名字和原资产对不上，没有处理，需要人工看"));
			++Problems;
			continue;
		}
		if (!bApply || FMath::IsNearlyEqual(Ratio, 1.f, 1e-4f))
		{
			continue;
		}

		Target->Modify();
		for (int32 I = 0; I < Original->Notifies.Num(); ++I)
		{
			Retime(Target->Notifies[I], Original->Notifies[I], Target, Ratio);
		}
		Target->SortNotifies();
		Target->AuthoredSyncMarkers = Original->AuthoredSyncMarkers;
		for (FAnimSyncMarker& Marker : Target->AuthoredSyncMarkers)
		{
			Marker.Time *= Ratio;
		}
		Target->RefreshSyncMarkerDataFromAuthored();
		if (!MissingCurves.IsEmpty())
		{
			IAnimationDataController& Controller = Target->GetController();
			IAnimationDataController::FScopedBracket Bracket(Controller, NSLOCTEXT("WuwaAnimRetime", "Curves", "Restore curves"), false);
			for (const FFloatCurve* Curve : MissingCurves)
			{
				const FAnimationCurveIdentifier Id(Curve->GetName(), ERawCurveTrackTypes::RCT_Float);
				Controller.AddCurve(Id, Curve->GetCurveTypeFlags(), false);
				Controller.SetCurveKeys(Id, ScaleKeys(Curve->FloatCurve.GetConstRefOfKeys(), Ratio), false);
			}
		}
		Target->RefreshCacheData();
		Target->MarkPackageDirty();
		ToSave.AddUnique(Target->GetOutermost());
	}

	for (const FString& Line : Lines)
	{
		UE_LOG(LogWuwaAnimRetime, Display, TEXT("%s"), *Line);
	}
	FFileHelper::SaveStringArrayToFile(Lines, *(FPaths::ProjectSavedDir() / TEXT("Logs/AnimFbxImport") /
		FString::Printf(TEXT("Retime_%s%s.txt"), *FPaths::GetCleanFilename(Backup), bApply ? TEXT("") : TEXT("_preview"))),
		FFileHelper::EEncodingOptions::ForceUTF8);
	UE_LOG(LogWuwaAnimRetime, Display, TEXT("汇总：%d 个资产，需要人工看的 %d 个，修改 %d 个"), Files.Num(), Problems, ToSave.Num());

	if (bApply && !ToSave.IsEmpty() && !UEditorLoadingAndSavingUtils::SavePackages(ToSave, false))
	{
		UE_LOG(LogWuwaAnimRetime, Error, TEXT("保存失败"));
		return 1;
	}
	FPackageName::UnRegisterMountPoint(MountRoot, Backup / TEXT(""));
	return Problems ? 1 : 0;
}
