#include "Editor/WuwaRoleGameplayEditorLibrary.h"

#if WITH_EDITOR
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/Skeleton.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#endif

bool UWuwaRoleGameplayEditorLibrary::RemapMontageSequence(UAnimMontage* Montage, UAnimSequence* Sequence)
{
#if WITH_EDITOR
    if (!Montage || !Sequence || !Sequence->GetSkeleton() || Montage->SlotAnimTracks.Num() != 1
        || Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() != 1) { return false; }
    FAnimSegment& Segment = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0];
    UAnimSequenceBase* Old = Segment.GetAnimReference();
    if (!Old || !FMath::IsNearlyEqual(Old->GetPlayLength(), Sequence->GetPlayLength(), 0.0001f)) { return false; }
    TArray<float> SectionTimes;
    for (const auto& Section : Montage->CompositeSections) { SectionTimes.Add(Section.GetTime()); }
    TArray<FVector2f> NotifyTimes;
    for (const auto& Event : Montage->Notifies) { NotifyTimes.Emplace(Event.GetTime(), Event.GetDuration()); }
    Montage->Modify();
    Montage->SetSkeleton(Sequence->GetSkeleton());
    Segment.SetAnimReference(Sequence);
    for (int32 Index = 0; Index < SectionTimes.Num(); ++Index)
    {
        Montage->CompositeSections[Index].Link(Montage, SectionTimes[Index]);
    }
    for (int32 Index = 0; Index < NotifyTimes.Num(); ++Index)
    {
        FAnimNotifyEvent& Event = Montage->Notifies[Index];
        Event.Link(Montage, NotifyTimes[Index].X);
        Event.SetDuration(NotifyTimes[Index].Y);
        Event.EndLink.Link(Montage, NotifyTimes[Index].X + NotifyTimes[Index].Y);
    }
    Montage->RefreshCacheData();
    Montage->PostEditChange();
    Montage->MarkPackageDirty();
    return true;
#else
    return false;
#endif
}

int32 UWuwaRoleGameplayEditorLibrary::CopyNamedSequenceNotifies(UAnimSequence* Source, UAnimSequence* Destination)
{
#if WITH_EDITOR
    if (!Source || !Destination || !Destination->GetSkeleton() || Source->GetPlayLength() <= 0) { return -1; }
    const float Ratio = Destination->GetPlayLength() / Source->GetPlayLength();
    Destination->Modify();
    int32 Count = 0;
    for (const FAnimNotifyEvent& Old : Source->Notifies)
    {
        if (Old.Notify || Old.NotifyStateClass || Old.NotifyName.IsNone()) { continue; }
        Destination->Notifies.RemoveAll([&Old](const FAnimNotifyEvent& E)
        {
            return !E.Notify && !E.NotifyStateClass && E.NotifyName == Old.NotifyName;
        });
        const FName TrackName = Source->AnimNotifyTracks.IsValidIndex(Old.TrackIndex)
            ? Source->AnimNotifyTracks[Old.TrackIndex].TrackName : FName(TEXT("1"));
        int32 Track = Destination->AnimNotifyTracks.IndexOfByPredicate([TrackName](const FAnimNotifyTrack& T) { return T.TrackName == TrackName; });
        if (Track == INDEX_NONE) { Track = Destination->AnimNotifyTracks.Emplace(TrackName, FLinearColor::White); }
        FAnimNotifyEvent Event = Old;
        Event.TrackIndex = Track;
        Event.Guid = FGuid::NewGuid();
        Event.ChangeLinkMethod(EAnimLinkMethod::Absolute);
        Event.EndLink.ChangeLinkMethod(EAnimLinkMethod::Absolute);
        Event.Link(Destination, Old.GetTime() * Ratio);
        Event.SetDuration(Old.GetDuration() * Ratio);
        Event.EndLink.Link(Destination, (Old.GetTime() + Old.GetDuration()) * Ratio);
        Destination->Notifies.Add(Event);
        Destination->GetSkeleton()->AddNewAnimationNotify(Event.NotifyName);
        ++Count;
    }
    Destination->RefreshCacheData();
    Destination->PostEditChange();
    Destination->MarkPackageDirty();
    return Count;
#else
    return -1;
#endif
}

bool UWuwaRoleGameplayEditorLibrary::ConfigureRoleGameplayMontage(UAnimMontage* TemplateMontage,
    UAnimMontage* Destination, UAnimSequence* Sequence)
{
#if WITH_EDITOR
    if (!TemplateMontage || !Destination || TemplateMontage == Destination || !Sequence
        || !Sequence->GetSkeleton() || TemplateMontage->SlotAnimTracks.Num() != 1
        || TemplateMontage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() != 1
        || TemplateMontage->GetPlayLength() <= 0 || Sequence->GetPlayLength() <= 0) { return false; }
    Destination->Modify();
    Destination->SetSkeleton(Sequence->GetSkeleton());
    Destination->BlendIn = TemplateMontage->BlendIn;
    Destination->BlendOut = TemplateMontage->BlendOut;
    Destination->BlendOutTriggerTime = TemplateMontage->BlendOutTriggerTime;
    Destination->bEnableAutoBlendOut = TemplateMontage->bEnableAutoBlendOut;
    Destination->RateScale = TemplateMontage->RateScale;
    Destination->SlotAnimTracks = TemplateMontage->SlotAnimTracks;
    FAnimSegment& Segment = Destination->SlotAnimTracks[0].AnimTrack.AnimSegments[0];
    Segment.SetAnimReference(Sequence);
    Segment.StartPos = 0;
    Segment.AnimStartTime = 0;
    Segment.AnimEndTime = Sequence->GetPlayLength();
    Segment.LoopingCount = 1;
    const float Length = Destination->CalculateSequenceLength();
    Destination->SetCompositeLength(Length);
    const float Ratio = Length / TemplateMontage->GetPlayLength();
    Destination->CompositeSections = TemplateMontage->CompositeSections;
    for (int32 Index = 0; Index < Destination->CompositeSections.Num(); ++Index)
    {
        auto& Section = Destination->CompositeSections[Index];
        Section.ChangeLinkMethod(EAnimLinkMethod::Absolute);
        Section.Link(Destination, TemplateMontage->CompositeSections[Index].GetTime() * Ratio);
    }
    Destination->AnimNotifyTracks.Reset();
    for (const FAnimNotifyTrack& Track : TemplateMontage->AnimNotifyTracks)
    {
        Destination->AnimNotifyTracks.Emplace(Track.TrackName, Track.TrackColor);
    }
    Destination->Notifies.Reset();
    for (const FAnimNotifyEvent& Source : TemplateMontage->Notifies)
    {
        const UObject* Notify = Source.Notify ? static_cast<UObject*>(Source.Notify.Get()) : Source.NotifyStateClass.Get();
        const FString ClassName = Notify ? Notify->GetClass()->GetName() : FString();
        if (!(ClassName.Contains(TEXT("SkillReadyEnd")) || ClassName.Contains(TEXT("MovementCancelWindow"))
            || ClassName.Contains(TEXT("DesireToKeepSprint")) || ClassName.Contains(TEXT("SkillAcceptInput")))) { continue; }
        FAnimNotifyEvent Event = Source;
        Event.Notify = Source.Notify ? DuplicateObject<UAnimNotify>(Source.Notify, Destination) : nullptr;
        Event.NotifyStateClass = Source.NotifyStateClass ? DuplicateObject<UAnimNotifyState>(Source.NotifyStateClass, Destination) : nullptr;
        Event.Guid = FGuid::NewGuid();
        Event.ChangeLinkMethod(EAnimLinkMethod::Absolute);
        Event.EndLink.ChangeLinkMethod(EAnimLinkMethod::Absolute);
        Event.Link(Destination, Source.GetTime() * Ratio);
        Event.SetDuration(Source.GetDuration() * Ratio);
        Event.EndLink.Link(Destination, (Source.GetTime() + Source.GetDuration()) * Ratio);
        Destination->Notifies.Add(MoveTemp(Event));
    }
    Destination->RefreshCacheData();
    Destination->PostEditChange();
    Destination->MarkPackageDirty();
    return true;
#else
    return false;
#endif
}

bool UWuwaRoleGameplayEditorLibrary::RemapGameplayAbility(UBlueprint* BP,
    const TArray<UAnimMontage*>& Sources, const TArray<UAnimMontage*>& Destinations, bool bRepairDash)
{
#if WITH_EDITOR
    if (!BP || Sources.Num() != Destinations.Num()) { return false; }
    for (int32 Index = 0; Index < Sources.Num(); ++Index)
    {
        if (!Sources[Index] || !Destinations[Index]) { return false; }
    }
    BP->Modify();
    TArray<UEdGraph*> Graphs;
    BP->GetAllGraphs(Graphs);
    UClass* RoleBase = bRepairDash ? LoadClass<UObject>(nullptr,
        TEXT("/Game/Characters/Role/Common/BP_RoleBase.BP_RoleBase_C")) : nullptr;
    if (bRepairDash && !RoleBase) { return false; }
    for (UEdGraph* Graph : Graphs)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            for (UEdGraphPin* Pin : Node->Pins)
            {
                for (int32 Index = 0; Index < Sources.Num(); ++Index)
                {
                    if (Pin && Pin->DefaultObject == Sources[Index]) { Pin->DefaultObject = Destinations[Index]; }
                }
            }
            if (!bRepairDash || Graph->GetName() != TEXT("Get Player CMC")) { continue; }
            if (auto* CastNode = Cast<UK2Node_DynamicCast>(Node))
            {
                UEdGraphPin* OldOutput = CastNode->GetCastResultPin();
                const TArray<UEdGraphPin*> OldLinks = OldOutput ? OldOutput->LinkedTo : TArray<UEdGraphPin*>();
                if (OldOutput) { OldOutput->BreakAllPinLinks(); }
                CastNode->TargetType = RoleBase;
                CastNode->ReconstructNode();
                UEdGraphPin* NewOutput = CastNode->GetCastResultPin();
                if (!NewOutput) { return false; }
                for (UEdGraphPin* Link : OldLinks) { NewOutput->MakeLinkTo(Link); }
            }
            if (auto* Call = Cast<UK2Node_CallFunction>(Node))
            {
                if (Call->FunctionReference.GetMemberName() != TEXT("SetVelocity")) { continue; }
                if (UEdGraphPin* Orphan = Call->FindPin(TEXT("Velocity")))
                {
                    Orphan->BreakAllPinLinks();
                    Orphan->DefaultValue.Reset();
                    Orphan->DefaultObject = nullptr;
                }
                UFunction* Function = RoleBase->FindFunctionByName(TEXT("SetVelocity"));
                if (!Function) { return false; }
                Call->SetFromFunction(Function);
                Call->ReconstructNode();
            }
        }
    }
    FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipSave, &Results);
    return Results.NumErrors == 0 && BP->Status != BS_Error;
#else
    return false;
#endif
}
