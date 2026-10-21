#include "Modifiers/WuwaPromoteRootMotionModifier.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "AnimationModifiersAssetUserData.h"
#include "Editor/WuwaAnimationMigrationLibrary.h"
#include "Misc/SecureHash.h"
#include "Serialization/MemoryWriter.h"

namespace
{
FString TrackDigest(const UAnimSequence* Sequence, FName Wrapper, FName Child)
{
    TArray<uint8> Bytes;
    FMemoryWriter Writer(Bytes);
    for (FName Bone : {Wrapper, Child})
    {
        TArray<FTransform> Keys;
        Sequence->GetDataModel()->GetBoneTrackTransforms(Bone, Keys);
        Writer << Keys;
    }
    return FMD5::HashBytes(Bytes.GetData(), Bytes.Num());
}

bool WriteTrack(UAnimSequence* Sequence, FName Bone, const TArray<FTransform>& Keys)
{
    TArray<FVector> Positions, Scales;
    TArray<FQuat> Rotations;
    for (const FTransform& Key : Keys)
    {
        Positions.Add(Key.GetTranslation());
        Rotations.Add(Key.GetRotation());
        Scales.Add(Key.GetScale3D());
    }
    return Sequence->GetController().SetBoneTrackKeys(Bone, Positions, Rotations, Scales, false);
}
}

void UWuwaPromoteRootMotionModifier::OnApply_Implementation(UAnimSequence* Sequence)
{
    if (!Sequence || !Sequence->GetSkeleton() || !Sequence->GetDataModel()) { return; }
    const FReferenceSkeleton& Ref = Sequence->GetSkeleton()->GetReferenceSkeleton();
    if (Ref.GetNum() == 0) { return; }
    WrapperBone = Ref.GetBoneName(0);
    OriginalWrapper.Reset();
    OriginalChild.Reset();
    AppliedTrackDigest.Reset();
    Sequence->GetDataModel()->GetBoneTrackTransforms(WrapperBone, OriginalWrapper);
    Sequence->GetDataModel()->GetBoneTrackTransforms(MotionBone, OriginalChild);
    bOriginalNormalizedRootMotionScale = Sequence->bUseNormalizedRootMotionScale;
    if (!UWuwaAnimationMigrationLibrary::PromoteChildRootMotion(Sequence, MotionBone))
    {
        UE_LOG(LogTemp, Error, TEXT("Cannot promote %s on %s: expected a wrapper root with one animated child."),
            *MotionBone.ToString(), *Sequence->GetPathName());
        return;
    }
    // A wrapper with scale 100 must retain its scale when extracting a displacement in centimeters.
    Sequence->bUseNormalizedRootMotionScale = false;
    AppliedTrackDigest = TrackDigest(Sequence, WrapperBone, MotionBone);
}

void UWuwaPromoteRootMotionModifier::OnRevert_Implementation(UAnimSequence* Sequence)
{
    if (!Sequence || !Sequence->GetDataModel() || AppliedTrackDigest.IsEmpty()) { return; }
    // A fresh FBX import replaced the source tracks. Do not overwrite it with an older saved source.
    if (TrackDigest(Sequence, WrapperBone, MotionBone) != AppliedTrackDigest) { return; }
    WriteTrack(Sequence, WrapperBone, OriginalWrapper);
    WriteTrack(Sequence, MotionBone, OriginalChild);
    Sequence->bUseNormalizedRootMotionScale = bOriginalNormalizedRootMotionScale;
}

bool UWuwaPromoteRootMotionModifier::ApplyToSequence(UAnimSequence* Sequence)
{
    if (!Sequence) { return false; }
    auto FindModifier = [Sequence]() -> UAnimationModifier*
    {
        const auto* Data = Cast<UAnimationModifiersAssetUserData>(
            Sequence->GetAssetUserDataOfClass(UAnimationModifiersAssetUserData::StaticClass()));
        if (Data)
        {
            for (UAnimationModifier* Modifier : Data->GetAnimationModifierInstances())
            {
                if (Modifier && Modifier->IsA< UWuwaPromoteRootMotionModifier>()) { return Modifier; }
            }
        }
        return nullptr;
    };
    UAnimationModifier* Modifier = FindModifier();
    if (!Modifier)
    {
        if (!UAnimationModifiersAssetUserData::AddAnimationModifierOfClass(Sequence, StaticClass())) { return false; }
        Modifier = FindModifier();
    }
    if (!Modifier) { return false; }
    UE::Anim::FApplyModifiersScope Scope(UE::Anim::FApplyModifiersScope::SuppressWarningAndError);
    Modifier->ApplyToAnimationSequence(Sequence);
    return Modifier->IsLatestRevisionApplied(Sequence);
}
