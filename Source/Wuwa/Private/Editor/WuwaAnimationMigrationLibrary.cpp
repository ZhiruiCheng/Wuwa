#include "Editor/WuwaAnimationMigrationLibrary.h"

#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "UObject/Package.h"
#include "Serialization/MemoryWriter.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "KismetCompiler.h"
#include "WuwaAnimationMetadataCopy.inl"
#endif

bool UWuwaAnimationMigrationLibrary::CopyAuthoredSettings(UAnimSequence* Source, UAnimSequence* Destination)
{
#if WITH_EDITOR
	FString Error;
	if (!CopyWuwaAnimationMetadata(Source, Destination, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Animation metadata migration: %s"), *Error);
		return false;
	}
	return true;
#else
	return false;
#endif
}

UAnimSequence* UWuwaAnimationMigrationLibrary::CreateConvertedSupport(UAnimSequence* Source,
	USkeleton* TargetSkeleton, const FString& DestinationPackage, double TranslationScale)
{
#if WITH_EDITOR
	if (!Source || !Source->GetSkeleton() || !TargetSkeleton || !Source->GetDataModel() || !FMath::IsFinite(TranslationScale)
		|| TranslationScale <= 0 || !FPackageName::IsValidLongPackageName(DestinationPackage))
	{
		return nullptr;
	}
	const FString Name = FPackageName::GetLongPackageAssetName(DestinationPackage);
	if (FindObject<UObject>(nullptr, *(DestinationPackage + TEXT(".") + Name))
		|| FPackageName::DoesPackageExist(DestinationPackage))
	{
		UE_LOG(LogTemp, Error, TEXT("Refusing to replace existing support asset %s"), *DestinationPackage);
		return nullptr;
	}
	const IAnimationDataModel* Model = Source->GetDataModel();
	const FReferenceSkeleton& SourceRef = Source->GetSkeleton()->GetReferenceSkeleton();
	const FReferenceSkeleton& TargetRef = TargetSkeleton->GetReferenceSkeleton();
	if (SourceRef.GetNum() == 0 || TargetRef.GetNum() == 0) { return nullptr; }
	const FName SourceWrapper = SourceRef.GetBoneName(0);
	const FName TargetWrapper = TargetRef.GetBoneName(0);
	const bool bRenameWrapper = TargetRef.FindBoneIndex(SourceWrapper) == INDEX_NONE;
	if (bRenameWrapper)
	{
		// Only a renamed top-level wrapper can be substituted. Confirm that its
		// reference transform represents the same world origin/orientation/units.
		FTransform ConvertedReference = SourceRef.GetRefBonePose()[0];
		ConvertedReference.SetScale3D(ConvertedReference.GetScale3D() / TranslationScale);
		if (!ConvertedReference.Equals(TargetRef.GetRefBonePose()[0], 0.001))
		{
			UE_LOG(LogTemp, Error, TEXT("Support wrapper reference poses do not match the requested units: %s -> %s"),
				*SourceWrapper.ToString(), *TargetWrapper.ToString());
			return nullptr;
		}
	}
	TArray<FName> Names;
	Model->GetBoneTrackNames(Names);
	for (const FName Bone : Names)
	{
		if (TargetRef.FindBoneIndex(Bone) == INDEX_NONE && !(bRenameWrapper && Bone == SourceWrapper))
		{
			UE_LOG(LogTemp, Error, TEXT("Support animation bone %s is absent from target skeleton"), *Bone.ToString());
			return nullptr;
		}
	}
	UAnimSequence* Result = NewObject<UAnimSequence>(CreatePackage(*DestinationPackage), *Name, RF_Public | RF_Standalone);
	Result->SetSkeleton(TargetSkeleton);
	IAnimationDataController& Controller = Result->GetController();
	Controller.InitializeModel();
	Controller.OpenBracket(FText::FromString(TEXT("Migrate support animation units")), false);
	Controller.SetFrameRate(Model->GetFrameRate(), false);
	Controller.SetNumberOfFrames(FFrameNumber(Model->GetNumberOfFrames()), false);
	for (const FName Bone : Names)
	{
		const bool bIsRenamedWrapper = bRenameWrapper && Bone == SourceWrapper;
		const FName DestinationBone = bIsRenamedWrapper ? TargetWrapper : Bone;
		TArray<FTransform> Transforms;
		Model->GetBoneTrackTransforms(Bone, Transforms);
		TArray<FVector> Positions, Scales;
		TArray<FQuat> Rotations;
		for (const FTransform& Transform : Transforms)
		{
			// The wrapper is already in world centimeters and can carry authored
			// root motion. Only child-local translations change units. Increasing
			// its scale compensates those child translations without losing motion.
			Positions.Add(bIsRenamedWrapper ? Transform.GetTranslation() : Transform.GetTranslation() * TranslationScale);
			Rotations.Add(Transform.GetRotation());
			Scales.Add(bIsRenamedWrapper ? Transform.GetScale3D() / TranslationScale : Transform.GetScale3D());
		}
		Controller.AddBoneCurve(DestinationBone, false);
		Controller.SetBoneTrackKeys(DestinationBone, Positions, Rotations, Scales, false);
	}
	// Old exports can omit the FBX wrapper track. Supply the target reference
	// wrapper so a moving child Root can subsequently be promoted without changing its pose.
	if (!Names.Contains(TargetWrapper) && !(bRenameWrapper && Names.Contains(SourceWrapper)))
	{
		const FName Wrapper = TargetRef.GetBoneName(0);
		const FTransform& Reference = TargetRef.GetRefBonePose()[0];
		const int32 KeyCount = Model->GetNumberOfKeys();
		TArray<FVector> Positions, Scales;
		TArray<FQuat> Rotations;
		Positions.Init(Reference.GetTranslation(), KeyCount);
		Rotations.Init(Reference.GetRotation(), KeyCount);
		Scales.Init(Reference.GetScale3D(), KeyCount);
		Controller.AddBoneCurve(Wrapper, false);
		Controller.SetBoneTrackKeys(Wrapper, Positions, Rotations, Scales, false);
	}
	Controller.NotifyPopulated();
	Controller.CloseBracket(false);
	Result->AdditiveAnimType = Source->AdditiveAnimType;
	Result->RefPoseType = Source->RefPoseType;
	Result->RefFrameIndex = Source->RefFrameIndex;
	// The caller resolves the reference sequence to the newly created helper before saving.
	Result->RefPoseSeq = Source->RefPoseSeq;
	if (!CopyAuthoredSettings(Source, Result))
	{
		return nullptr;
	}
	FAssetRegistryModule::AssetCreated(Result);
	Result->MarkPackageDirty();
	return Result;
#else
	return nullptr;
#endif
}

FString UWuwaAnimationMigrationLibrary::CompareConvertedBonePoses(UAnimSequence* Source,
	UAnimSequence* Destination, double TranslationScale)
{
#if WITH_EDITOR
	if (!Source || !Destination || !Source->GetSkeleton() || !Destination->GetSkeleton()
		|| !Source->GetDataModel() || !Destination->GetDataModel() || TranslationScale <= 0 || !FMath::IsFinite(TranslationScale))
	{ return TEXT("{\"ok\":false,\"error\":\"Invalid pose comparison input\"}"); }
	const IAnimationDataModel* A = Source->GetDataModel();
	const IAnimationDataModel* B = Destination->GetDataModel();
	if (A->GetNumberOfFrames() != B->GetNumberOfFrames() || A->GetFrameRate() != B->GetFrameRate())
	{ return TEXT("{\"ok\":false,\"error\":\"Animation timing changed\"}"); }
	const FReferenceSkeleton& RefA = Source->GetSkeleton()->GetReferenceSkeleton();
	const FReferenceSkeleton& RefB = Destination->GetSkeleton()->GetReferenceSkeleton();
	if (RefA.GetNum() == 0 || RefB.GetNum() == 0)
	{ return TEXT("{\"ok\":false,\"error\":\"Empty reference skeleton\"}"); }
	TArray<FName> TracksA, TracksB;
	A->GetBoneTrackNames(TracksA);
	B->GetBoneTrackNames(TracksB);
	TSet<FName> TrackSetA(TracksA), TrackSetB(TracksB);
	TArray<FIntPoint> Pairs;
	for (FName Bone : TracksA)
	{
		const int32 IndexA = RefA.FindBoneIndex(Bone);
		const int32 IndexB = IndexA == 0 ? 0 : RefB.FindBoneIndex(Bone);
		if (IndexA == INDEX_NONE || IndexB == INDEX_NONE)
		{ return TEXT("{\"ok\":false,\"error\":\"An authored bone has no counterpart\"}"); }
		Pairs.Emplace(IndexA, IndexB);
	}
	if (Pairs.IsEmpty()) { return TEXT("{\"ok\":false,\"error\":\"No authored bone tracks\"}"); }
	auto WorldPose = [](const IAnimationDataModel* Model, const FReferenceSkeleton& Ref,
		const TSet<FName>& Tracks, const FFrameTime& Frame, EAnimInterpolationType Interpolation, TArray<FTransform>& Result)
	{
		Result.SetNum(Ref.GetNum());
		for (int32 Index = 0; Index < Ref.GetNum(); ++Index)
		{
			const FName Bone = Ref.GetBoneName(Index);
			const FTransform Local = Tracks.Contains(Bone) ? Model->EvaluateBoneTrackTransform(Bone, Frame, Interpolation)
				: Ref.GetRefBonePose()[Index];
			const int32 Parent = Ref.GetParentIndex(Index);
			Result[Index] = Parent == INDEX_NONE ? Local : Local * Result[Parent];
		}
	};
	double MaxPosition = 0, MaxRotationDegrees = 0, MaxNormalizedScale = 0;
	TArray<FTransform> PoseA, PoseB;
	const int32 Samples = A->GetNumberOfFrames() * 2 + 1;
	for (int32 Sample = 0; Sample < Samples; ++Sample)
	{
		const FFrameTime Frame(FFrameNumber(Sample / 2), Sample % 2 ? 0.5f : 0.f);
		WorldPose(A, RefA, TrackSetA, Frame, Source->Interpolation, PoseA);
		WorldPose(B, RefB, TrackSetB, Frame, Destination->Interpolation, PoseB);
		for (const FIntPoint& Pair : Pairs)
		{
			const FTransform& Old = PoseA[Pair.X];
			const FTransform& New = PoseB[Pair.Y];
			if (Old.ContainsNaN() || New.ContainsNaN())
			{ return TEXT("{\"ok\":false,\"error\":\"Nonfinite world pose\"}"); }
			MaxPosition = FMath::Max(MaxPosition, FVector::Distance(Old.GetTranslation(), New.GetTranslation()));
			MaxRotationDegrees = FMath::Max(MaxRotationDegrees,
				FMath::RadiansToDegrees(Old.GetRotation().AngularDistance(New.GetRotation())));
			MaxNormalizedScale = FMath::Max(MaxNormalizedScale,
				(New.GetScale3D() * TranslationScale - Old.GetScale3D()).GetAbsMax());
		}
	}
	const bool bPassed = MaxPosition < 0.05 && MaxRotationDegrees < 0.01 && MaxNormalizedScale < 0.001;
	return FString::Printf(TEXT("{\"ok\":%s,\"bones\":%d,\"samples\":%d,\"max_position_error_cm\":%.9f,\"max_rotation_error_degrees\":%.9f,\"max_unit_normalized_scale_error\":%.9f}"),
		bPassed ? TEXT("true") : TEXT("false"), Pairs.Num(), Samples, MaxPosition, MaxRotationDegrees, MaxNormalizedScale);
#else
	return TEXT("{\"ok\":false,\"error\":\"Editor only\"}");
#endif
}

bool UWuwaAnimationMigrationLibrary::RemapBlendSpace(UBlendSpace* BlendSpace, USkeleton* TargetSkeleton,
	const TArray<UAnimSequence*>& Sources, const TArray<UAnimSequence*>& Destinations)
{
#if WITH_EDITOR
	if (!BlendSpace || !TargetSkeleton || Sources.Num() != Destinations.Num()) { return false; }
	TMap<UAnimationAsset*, UAnimationAsset*> Replacements;
	for (int32 Index = 0; Index < Sources.Num(); ++Index)
	{
		if (!Sources[Index] || !Destinations[Index] || Destinations[Index]->GetSkeleton() != TargetSkeleton) { return false; }
		Replacements.Add(Sources[Index], Destinations[Index]);
	}
	for (const FBlendSample& Sample : BlendSpace->GetBlendSamples())
	{
		if (Sample.Animation && !Replacements.Contains(Sample.Animation)) { return false; }
	}
	BlendSpace->Modify();
	BlendSpace->SetSkeleton(TargetSkeleton);
	BlendSpace->ReplaceReferredAnimations(Replacements);
	BlendSpace->ValidateSampleData();
	BlendSpace->ResampleData();
	BlendSpace->PostEditChange();
	BlendSpace->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

FString UWuwaAnimationMigrationLibrary::BoneTrackDigest(UAnimSequence* Sequence)
{
#if WITH_EDITOR
	if (!Sequence || !Sequence->GetDataModel()) { return FString(); }
	const IAnimationDataModel* Model = Sequence->GetDataModel();
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FFrameRate Rate = Model->GetFrameRate();
	int32 Frames = Model->GetNumberOfFrames();
	Writer << Rate << Frames;
	TArray<FName> Names;
	Model->GetBoneTrackNames(Names);
	for (const FName Bone : Names)
	{
		FString Name = Bone.ToString();
		TArray<FTransform> Transforms;
		Model->GetBoneTrackTransforms(Bone, Transforms);
		Writer << Name << Transforms;
	}
	return FMD5::HashBytes(Bytes.GetData(), Bytes.Num());
#else
	return FString();
#endif
}

bool UWuwaAnimationMigrationLibrary::PromoteChildRootMotion(UAnimSequence* Sequence, FName MotionBone)
{
#if WITH_EDITOR
	if (!Sequence || !Sequence->GetSkeleton() || !Sequence->GetDataModel()) { return false; }
	const FReferenceSkeleton& Ref = Sequence->GetSkeleton()->GetReferenceSkeleton();
	const int32 MotionIndex = Ref.FindBoneIndex(MotionBone);
	if (MotionIndex <= 0 || Ref.GetParentIndex(MotionIndex) != 0) { return false; }
	// A second branch directly under the wrapper would need its own compensation.
	for (int32 Index = 1; Index < Ref.GetNum(); ++Index)
	{
		if (Ref.GetParentIndex(Index) == 0 && Index != MotionIndex) { return false; }
	}
	IAnimationDataModel* Model = Sequence->GetDataModel();
	const FName RootBone = Ref.GetBoneName(0);
	TArray<FTransform> Parents, Children;
	Model->GetBoneTrackTransforms(RootBone, Parents);
	Model->GetBoneTrackTransforms(MotionBone, Children);
	if (Parents.Num() != Children.Num() || Parents.IsEmpty()) { return false; }
	TArray<FVector> RootPositions, RootScales, ChildPositions, ChildScales;
	TArray<FQuat> RootRotations, ChildRotations;
	// 子骨骼留在参考姿势（不是第 0 帧）：原作锁根把根骨锁回参考姿势，第 0 帧就偏开的动画（嘉贝 Attack_Jump_End02 Root 在前方 409 cm、
	// Attack08_H 在上方 213 cm）偏移整个交给包装骨，再由锁根去掉；留在子骨骼上的话模型会一直偏在胶囊外面
	const FTransform ChildStart = Ref.GetRefBonePose()[MotionIndex];
	for (int32 Index = 0; Index < Parents.Num(); ++Index)
	{
		const FTransform NewParent = ChildStart.Inverse() * Children[Index] * Parents[Index];
		if (!(ChildStart * NewParent).Equals(Children[Index] * Parents[Index], 0.001)) { return false; }
		RootPositions.Add(NewParent.GetTranslation());
		RootRotations.Add(NewParent.GetRotation());
		RootScales.Add(NewParent.GetScale3D());
		ChildPositions.Add(ChildStart.GetTranslation());
		ChildRotations.Add(ChildStart.GetRotation());
		ChildScales.Add(ChildStart.GetScale3D());
	}
	Sequence->Modify();
	IAnimationDataController& Controller = Sequence->GetController();
	{
		IAnimationDataController::FScopedBracket Bracket(Controller, FText::FromString(TEXT("Promote imported root motion")), false);
		if (!Controller.SetBoneTrackKeys(RootBone, RootPositions, RootRotations, RootScales, false)
			|| !Controller.SetBoneTrackKeys(MotionBone, ChildPositions, ChildRotations, ChildScales, false)) { return false; }
	}
	Sequence->PostEditChange();
	Sequence->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

FTransform UWuwaAnimationMigrationLibrary::ReadRootMotionDelta(UAnimSequence* Sequence)
{
#if WITH_EDITOR
	if (Sequence)
	{
		FAnimExtractContext Context(0.0, true);
		return Sequence->ExtractRootMotionFromRange(0.0, Sequence->GetPlayLength(), Context);
	}
#endif
	return FTransform::Identity;
}

bool UWuwaAnimationMigrationLibrary::CompileAnimationBlueprint(UBlueprint* Blueprint)
{
#if WITH_EDITOR
	if (!Blueprint) { return false; }
	FCompilerResultsLog Results;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipSave, &Results);
	return Results.NumErrors == 0;
#else
	return false;
#endif
}

int32 UWuwaAnimationMigrationLibrary::RemapAnimationPinDefaults(UBlueprint* Blueprint,
	const TArray<UAnimSequence*>& Sources, const TArray<UAnimSequence*>& Destinations)
{
#if WITH_EDITOR
	if (!Blueprint || Sources.Num() != Destinations.Num()) { return -1; }
	TMap<UObject*, UObject*> Replacements;
	for (int32 Index = 0; Index < Sources.Num(); ++Index)
	{
		if (!Sources[Index] || !Destinations[Index]) { return -1; }
		Replacements.Add(Sources[Index], Destinations[Index]);
	}
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	int32 Count = 0;
	for (UEdGraph* Graph : Graphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (UObject** Replacement = Replacements.Find(Pin->DefaultObject))
				{
					Node->Modify();
					Pin->DefaultObject = *Replacement;
					++Count;
				}
			}
		}
	}
	if (Count) { FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint); }
	return Count;
#else
	return -1;
#endif
}
