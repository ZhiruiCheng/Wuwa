#include "Commandlets/WuwaAnimFbxImportCommandlet.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "AnimationModifiersAssetUserData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "InterchangeGenericAnimationPipeline.h"
#include "InterchangeGenericAssetsPipeline.h"
#include "InterchangeGenericAssetsPipelineSharedSettings.h"
#include "InterchangeGenericMaterialPipeline.h"
#include "InterchangeGenericMeshPipeline.h"
#include "InterchangeGenericTexturePipeline.h"
#include "InterchangeManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modifiers/WuwaPromoteRootMotionModifier.h"
#include "ObjectTools.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaAnimImport, Log, All);

namespace WuwaAnimFbxImport
{
	const TCHAR* PreviewPath = TEXT("/Game/__WuwaAnimImportPreview");
	const FName MotionBone = TEXT("Root");

	struct FExisting
	{
		UAnimSequence* Sequence = nullptr;
		bool bHadRootModifier = false;
		double OldLength = 0.0;
		FFrameRate OldRate;
	};

	bool HasRootModifier(UAnimSequence* Sequence)
	{
		const auto* Data = Cast<UAnimationModifiersAssetUserData>(
			Sequence->GetAssetUserDataOfClass(UAnimationModifiersAssetUserData::StaticClass()));
		if (!Data)
		{
			return false;
		}
		for (const UAnimationModifier* Modifier : Data->GetAnimationModifierInstances())
		{
			if (Modifier && Modifier->IsA<UWuwaPromoteRootMotionModifier>())
			{
				return true;
			}
		}
		return false;
	}

	/** Root 骨骼离开第一帧位置的最大距离（cm）。Root 挂在 FBX 包装骨骼下，包装骨骼缩放 100，所以要乘上它的缩放。 */
	double RootTravel(const UAnimSequence* Sequence)
	{
		const IAnimationDataModel* Model = Sequence->GetDataModel();
		TArray<FTransform> Keys, Wrapper;
		Model->GetBoneTrackTransforms(MotionBone, Keys);
		const FReferenceSkeleton& Ref = Sequence->GetSkeleton()->GetReferenceSkeleton();
		Model->GetBoneTrackTransforms(Ref.GetBoneName(0), Wrapper);
		const double Scale = Wrapper.Num() ? Wrapper[0].GetScale3D().X : Ref.GetRefBonePose()[0].GetScale3D().X;
		double Travel = 0.0;
		for (const FTransform& Key : Keys)
		{
			Travel = FMath::Max(Travel, FVector::Dist(Key.GetTranslation(), Keys[0].GetTranslation()));
		}
		return Travel * Scale;
	}

	/**
	 * 把导入结果的动画数据整个写进已有资产（帧率、帧数、骨骼轨道、浮点曲线），资产本身（引用、通知、根运动设置、修改器）不动。
	 * 引擎的 PopulateWithExistingModel 不支持 5.x 的 AnimationSequencerDataModel，所以逐项写。
	 */
	bool CopyAnimData(const UAnimSequence* Source, UAnimSequence* Target)
	{
		const IAnimationDataModel* From = Source->GetDataModel();
		if (!From->GetAttributes().IsEmpty())
		{
			UE_LOG(LogWuwaAnimImport, Warning, TEXT("%s 有 %d 条自定义属性，没有复制"), *Source->GetName(), From->GetAttributes().Num());
		}
		// 资产上还存着「目标采样帧率」（protected）。旧资产是 24，数据换成 30fps 后按 24 重采样会出现小数帧，
		// 压缩时触发断言（Stop_Run_L3 实测崩溃），所以先把它和导入结果对齐。
		for (const TCHAR* PropName : {TEXT("PlatformTargetFrameRate"), TEXT("TargetFrameRate")})
		{
			if (const FProperty* Prop = UAnimSequence::StaticClass()->FindPropertyByName(PropName))
			{
				Prop->CopyCompleteValue(Prop->ContainerPtrToValuePtr<void>(Target), Prop->ContainerPtrToValuePtr<void>(Source));
			}
		}
		// 资产上在 UE 里做的东西（手加的曲线、同步标记、通知）按秒存；先记下来，换完数据按 新时长/旧时长 放回去。
		// 不这样做的话：曲线被清掉（长离 Walk_F 的 foot_*/IsWalk 实测丢过），标记 / 通知被引擎挪到末尾。
		const float OldLength = Target->GetPlayLength();
		const TArray<FFloatCurve> OldCurves = Target->GetDataModel()->GetFloatCurves();
		const TArray<FAnimSyncMarker> OldMarkers = Target->AuthoredSyncMarkers;
		TArray<TPair<float, float>> OldNotifyTimes;
		for (const FAnimNotifyEvent& Notify : Target->Notifies)
		{
			OldNotifyTimes.Emplace(Notify.GetTime(), Notify.GetDuration());
		}

		IAnimationDataController& To = Target->GetController();
		IAnimationDataController::FScopedBracket Bracket(To, NSLOCTEXT("WuwaAnimFbxImport", "Copy", "Copy imported animation"), false);
		// 有关键帧时引擎不允许把 24fps 改成 30fps（不是整数倍），所以先清空数据模型再设帧率
		To.ResetModel(false);
		To.SetFrameRate(From->GetFrameRate(), false);
		To.SetNumberOfFrames(FFrameNumber(From->GetNumberOfFrames()), false);
		TArray<FName> Bones;
		From->GetBoneTrackNames(Bones);
		for (const FName Bone : Bones)
		{
			TArray<FTransform> Keys;
			From->GetBoneTrackTransforms(Bone, Keys);
			TArray<FVector> Positions, Scales;
			TArray<FQuat> Rotations;
			for (const FTransform& Key : Keys)
			{
				Positions.Add(Key.GetTranslation());
				Rotations.Add(Key.GetRotation());
				Scales.Add(Key.GetScale3D());
			}
			if (!To.AddBoneCurve(Bone, false) || !To.SetBoneTrackKeys(Bone, Positions, Rotations, Scales, false))
			{
				return false;
			}
		}
		for (const FFloatCurve& Curve : From->GetFloatCurves())
		{
			const FAnimationCurveIdentifier Id(Curve.GetName(), ERawCurveTrackTypes::RCT_Float);
			To.AddCurve(Id, Curve.GetCurveTypeFlags(), false);
			To.SetCurveKeys(Id, Curve.FloatCurve.GetConstRefOfKeys(), false);
		}
		Target->ImportFileFramerate = Source->ImportFileFramerate;
		Target->ImportResampleFramerate = Source->ImportResampleFramerate;

		const float Ratio = OldLength > 0.f ? Source->GetPlayLength() / OldLength : 1.f;
		for (const FFloatCurve& Curve : OldCurves)
		{
			const FAnimationCurveIdentifier Id(Curve.GetName(), ERawCurveTrackTypes::RCT_Float);
			if (Target->GetDataModel()->FindFloatCurve(Id))
			{
				continue; // FBX 里也有这条，用导入的
			}
			TArray<FRichCurveKey> Keys = Curve.FloatCurve.GetConstRefOfKeys();
			for (FRichCurveKey& Key : Keys)
			{
				Key.Time *= Ratio;
				Key.ArriveTangent /= Ratio; // 切线是每秒的斜率
				Key.LeaveTangent /= Ratio;
			}
			To.AddCurve(Id, Curve.GetCurveTypeFlags(), false);
			To.SetCurveKeys(Id, Keys, false);
		}
		Target->AuthoredSyncMarkers = OldMarkers;
		for (FAnimSyncMarker& Marker : Target->AuthoredSyncMarkers)
		{
			Marker.Time *= Ratio;
		}
		Target->RefreshSyncMarkerDataFromAuthored();
		for (int32 I = 0; I < Target->Notifies.Num() && I < OldNotifyTimes.Num(); ++I)
		{
			FAnimNotifyEvent& Notify = Target->Notifies[I];
			const float Time = OldNotifyTimes[I].Key * Ratio;
			Notify.Link(Target, Time);
			Notify.TriggerTimeOffset = GetTriggerTimeOffsetForType(Target->CalculateOffsetForNotify(Time));
			if (OldNotifyTimes[I].Value > 0.f)
			{
				const float Duration = OldNotifyTimes[I].Value * Ratio;
				Notify.SetDuration(Duration);
				Notify.EndLink.Link(Target, Time + Duration);
				Notify.EndTriggerTimeOffset = GetTriggerTimeOffsetForType(Target->CalculateOffsetForNotify(Time + Duration));
			}
		}
		Target->RefreshCacheData();

		const IAnimationDataModel* Result = Target->GetDataModel();
		return Result->GetFrameRate() == From->GetFrameRate() && Result->GetNumberOfFrames() == From->GetNumberOfFrames()
			&& Target->GetSamplingFrameRate() == Source->GetSamplingFrameRate();
	}

	FString Rate(const FFrameRate& R)
	{
		return R.IsValid() ? FString::Printf(TEXT("%.0f"), R.AsDecimal()) : TEXT("-");
	}

	UInterchangeGenericAssetsPipeline* MakePipeline(USkeleton* Skeleton)
	{
		UInterchangeGenericAssetsPipeline* Pipeline = NewObject<UInterchangeGenericAssetsPipeline>(GetTransientPackage());
		Pipeline->bUseSourceNameForAsset = true; // 资产名 = FBX 文件名 + 动作名（和以前手动导入一致）
		Pipeline->CommonSkeletalMeshesAndAnimationsProperties->bImportOnlyAnimations = true;
		Pipeline->CommonSkeletalMeshesAndAnimationsProperties->Skeleton = Skeleton;
		Pipeline->MeshPipeline->bImportSkeletalMeshes = false;
		Pipeline->MeshPipeline->bImportStaticMeshes = false;
		Pipeline->MaterialPipeline->bImportMaterials = false;
		Pipeline->MaterialPipeline->TexturePipeline->bImportTextures = false;
		UInterchangeGenericAnimationPipeline* Anim = Pipeline->AnimationPipeline;
		Anim->bImportAnimations = true;
		Anim->bImportBoneTracks = true;
		Anim->AnimationRange = EInterchangeAnimationRange::Timeline;
		Anim->bUse30HzToBakeBoneAnimation = false;
		Anim->CustomBoneAnimationSampleRate = 0; // 用 FBX 自己的帧率
		Anim->bSnapToClosestFrameBoundary = false;
		Anim->bImportCustomAttribute = true;
		return Pipeline;
	}
}

UWuwaAnimFbxImportCommandlet::UWuwaAnimFbxImportCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UWuwaAnimFbxImportCommandlet::Main(const FString& Params)
{
	using namespace WuwaAnimFbxImport;

	FString Fbx, SkeletonPath, Dest, Search;
	FParse::Value(*Params, TEXT("Fbx="), Fbx);
	FParse::Value(*Params, TEXT("Skeleton="), SkeletonPath);
	FParse::Value(*Params, TEXT("Dest="), Dest);
	FParse::Value(*Params, TEXT("Search="), Search);
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	FPaths::NormalizeFilename(Fbx);
	if (Search.IsEmpty())
	{
		Search = Dest;
	}

	if (!FPaths::FileExists(Fbx) || Dest.IsEmpty())
	{
		UE_LOG(LogWuwaAnimImport, Error, TEXT("需要 -Fbx=<存在的 FBX> -Skeleton=<骨骼> -Dest=<目标文件夹>（当前 Fbx=%s Dest=%s）"), *Fbx, *Dest);
		return 1;
	}
	USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, *SkeletonPath);
	if (!Skeleton)
	{
		UE_LOG(LogWuwaAnimImport, Error, TEXT("找不到骨骼 %s"), *SkeletonPath);
		return 1;
	}

	// ---- 1. 现有的同前缀动画（导入会覆盖同名的） ----
	const FString Prefix = ObjectTools::SanitizeObjectName(FPaths::GetBaseFilename(Fbx));
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	Registry.SearchAllAssets(true);
	TArray<FAssetData> Found;
	Registry.GetAssetsByPath(FName(*Search), Found, true);
	TMap<FString, TArray<FExisting>> Existing;
	for (const FAssetData& Asset : Found)
	{
		const FString Name = Asset.AssetName.ToString();
		if (Asset.AssetClassPath != UAnimSequence::StaticClass()->GetClassPathName() || !Name.StartsWith(Prefix))
		{
			continue;
		}
		if (UAnimSequence* Sequence = Cast<UAnimSequence>(Asset.GetAsset()))
		{
			FExisting& E = Existing.FindOrAdd(Name).AddDefaulted_GetRef();
			E.Sequence = Sequence;
			E.bHadRootModifier = HasRootModifier(Sequence);
			E.OldLength = Sequence->GetPlayLength();
			E.OldRate = Sequence->GetSamplingFrameRate();
		}
	}
	UE_LOG(LogWuwaAnimImport, Display, TEXT("%s 下已有 %d 个名为 %s* 的动画"), *Search, Existing.Num(), *Prefix);

	// ---- 2. 备份要覆盖的 .uasset ----
	const FString Stamp = FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
	if (bApply)
	{
		const FString BackupRoot = FPaths::ProjectSavedDir() / TEXT("Backups/AnimFbxImport") / Stamp;
		for (const TPair<FString, TArray<FExisting>>& Pair : Existing)
		{
			for (const FExisting& E : Pair.Value)
			{
				const FString Package = E.Sequence->GetOutermost()->GetName();
				const FString File = FPackageName::LongPackageNameToFilename(Package, FPackageName::GetAssetPackageExtension());
				const FString Relative = Package.RightChop(6); // 去掉 /Game/
				IFileManager::Get().Copy(*(BackupRoot / Relative + FPackageName::GetAssetPackageExtension()), *File);
			}
		}
		UE_LOG(LogWuwaAnimImport, Display, TEXT("已备份到 %s"), *BackupRoot);
	}

	// ---- 3. 导入到空的临时文件夹 ----
	// 直接导入到已有资产的文件夹时，Interchange 会静默地什么都不产出（长离 NewModel 里实测），所以统一先导到空文件夹，
	// 再把数据复制进同名的已有资产，或者把新动画移进 Dest。临时文件夹的包从不保存。
	const FString ImportPath = FString(PreviewPath) / Stamp;
	UInterchangeGenericAssetsPipeline* Pipeline = MakePipeline(Skeleton);
	Pipeline->AddToRoot();
	FImportAssetParameters ImportParams;
	ImportParams.bIsAutomated = true;
	ImportParams.bReplaceExisting = true;
	ImportParams.OverridePipelines.Add(FSoftObjectPath(Pipeline));
	TArray<UObject*> Imported;
	const double ImportStart = FPlatformTime::Seconds();
	UInterchangeManager& Manager = UInterchangeManager::GetInterchangeManager();
	const bool bImported = Manager.ImportAsset(ImportPath, UInterchangeManager::CreateSourceData(Fbx), ImportParams, Imported);
	Pipeline->RemoveFromRoot();
	UE_LOG(LogWuwaAnimImport, Display, TEXT("导入 %s -> %s：%s，%d 个对象，用时 %.0fs"),
		*Fbx, *ImportPath, bImported ? TEXT("成功") : TEXT("失败"), Imported.Num(), FPlatformTime::Seconds() - ImportStart);
	if (!bImported)
	{
		return 1;
	}

	// ---- 4. 覆盖同名的已有资产 / 新动画移进 Dest，重新应用根运动修改器 ----
	TArray<UPackage*> ToSave;
	TArray<FString> Lines;
	int32 Updated = 0, Created = 0, Copies = 0, Modified = 0, Failed = 0;
	for (UObject* Object : Imported)
	{
		UAnimSequence* New = Cast<UAnimSequence>(Object);
		if (!New)
		{
			continue;
		}
		const FString Name = New->GetName();
		const TArray<FExisting>* Matches = Existing.Find(Name);
		const double NewLength = New->GetPlayLength();
		const FString NewRate = Rate(New->GetSamplingFrameRate());

		if (!Matches)
		{
			// 新动画：移进 Dest；Root 有位移就挂上根运动修改器
			const double Travel = RootTravel(New);
			bool bModifier = false;
			if (bApply)
			{
				UPackage* Package = CreatePackage(*(Dest / Name));
				Package->FullyLoad();
				if (!New->Rename(*Name, Package, REN_DontCreateRedirectors | REN_NonTransactional))
				{
					UE_LOG(LogWuwaAnimImport, Error, TEXT("无法把 %s 移到 %s"), *Name, *Dest);
					++Failed;
					continue;
				}
				FAssetRegistryModule::AssetCreated(New);
				New->MarkPackageDirty();
				if (Travel > 1.0)
				{
					bModifier = UWuwaPromoteRootMotionModifier::ApplyToSequence(New);
					Modified += bModifier ? 1 : 0;
				}
				ToSave.AddUnique(Package);
			}
			Lines.Add(FString::Printf(TEXT("NEW     %-70s %.3fs@%s root=%.0fcm%s"), *Name, NewLength, *NewRate, Travel, bModifier ? TEXT(" +RootModifier") : TEXT("")));
			++Created;
			continue;
		}

		for (const FExisting& E : *Matches)
		{
			const FString Where = FPackageName::GetLongPackagePath(E.Sequence->GetOutermost()->GetName());
			if (bApply)
			{
				if (!CopyAnimData(New, E.Sequence))
				{
					UE_LOG(LogWuwaAnimImport, Error, TEXT("复制数据到 %s 失败"), *E.Sequence->GetPathName());
					++Failed;
					continue;
				}
				++Copies;
				if (E.bHadRootModifier)
				{
					const bool bOk = UWuwaPromoteRootMotionModifier::ApplyToSequence(E.Sequence);
					Modified += bOk ? 1 : 0;
					if (!bOk)
					{
						UE_LOG(LogWuwaAnimImport, Warning, TEXT("%s 重新应用根运动修改器失败"), *E.Sequence->GetPathName());
					}
				}
				E.Sequence->MarkPackageDirty();
				ToSave.AddUnique(E.Sequence->GetOutermost());
			}
			// 预演显示导入结果；-Apply 显示写入后资产的实际时长和帧率
			const double FinalLength = bApply ? E.Sequence->GetPlayLength() : NewLength;
			const FString FinalRate = bApply ? Rate(E.Sequence->GetSamplingFrameRate()) : NewRate;
			Lines.Add(FString::Printf(TEXT("UPDATE  %-70s %.3fs@%s -> %.3fs@%s  %s%s"), *Name, E.OldLength, *Rate(E.OldRate), FinalLength, *FinalRate,
				*Where, E.bHadRootModifier ? TEXT(" +RootModifier") : TEXT("")));
			++Updated;
		}
	}

	Lines.Sort();
	for (const FString& Line : Lines)
	{
		UE_LOG(LogWuwaAnimImport, Display, TEXT("%s"), *Line);
	}
	UE_LOG(LogWuwaAnimImport, Display, TEXT("汇总：覆盖 %d（写入 %d），新建 %d，应用根运动修改器 %d，失败 %d"), Updated, Copies, Created, Modified, Failed);

	const FString ReportDir = FPaths::ProjectSavedDir() / TEXT("Logs/AnimFbxImport");
	FFileHelper::SaveStringArrayToFile(Lines, *(ReportDir / FString::Printf(TEXT("%s_%s%s.txt"), *Prefix, *Stamp, bApply ? TEXT("") : TEXT("_preview"))),
		FFileHelper::EEncodingOptions::ForceUTF8);

	if (!bApply)
	{
		UE_LOG(LogWuwaAnimImport, Display, TEXT("预演结束，没有保存任何东西。确认后加 -Apply。"));
		return 0;
	}
	if (!UEditorLoadingAndSavingUtils::SavePackages(ToSave, false))
	{
		UE_LOG(LogWuwaAnimImport, Error, TEXT("保存失败"));
		return 1;
	}
	UE_LOG(LogWuwaAnimImport, Display, TEXT("已保存 %d 个包"), ToSave.Num());
	return Failed ? 1 : 0;
}
