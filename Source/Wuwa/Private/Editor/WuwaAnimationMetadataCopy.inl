// Task-scoped UE 5.7 editor helper. Include from exactly one editor .cpp.
// Caller owns the transaction/backups/save/reload verification. This never saves.
// Uses Engine interfaces only; no AnimationBlueprintLibrary dependency.
#if WITH_EDITOR

#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/CurveIdentifier.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Curves/RealCurve.h"
#include "Curves/RichCurve.h"
#include "UObject/UObjectGlobals.h"

static bool CopyWuwaAnimationMetadata(UAnimSequence* Source, UAnimSequence* Dest, FString& Error)
{
	Error.Reset();
	if (!IsInGameThread() || !Source || !Dest || Source == Dest)
	{
		Error = TEXT("Metadata copy requires different valid sequences on the game thread.");
		return false;
	}
	const IAnimationDataModel* SourceModel = Source->GetDataModel();
	const IAnimationDataModel* DestModel = Dest->GetDataModel();
	if (!SourceModel || !DestModel || Dest->GetPlayLength() <= 0.0f)
	{
		Error = TEXT("Metadata copy requires populated source and destination data models.");
		return false;
	}

	const float DestLength = Dest->GetPlayLength();
	const int32 DestFrames = DestModel->GetNumberOfFrames();
	const FFrameRate DestRate = DestModel->GetFrameRate();
	USkeleton* const DestSkeleton = Dest->GetSkeleton();
	TArray<FName> OriginalBoneTracks;
	DestModel->GetBoneTrackNames(OriginalBoneTracks);
	// Snapshot source values before controller notifications can invalidate references.
	const TArray<FFloatCurve> SourceCurves = SourceModel->GetFloatCurves();
	const TArray<FAnimNotifyEvent> SourceNotifies = Source->Notifies;
	const TArray<FAnimSyncMarker> SourceMarkers = Source->AuthoredSyncMarkers;

	// Validate all timing up front. Keep absolute seconds; never scale/clamp/drop events.
	for (const FAnimSyncMarker& Marker : SourceMarkers)
	{
		if (!FMath::IsFinite(Marker.Time) || Marker.Time < 0.0f || Marker.Time > DestLength
			|| !Source->AnimNotifyTracks.IsValidIndex(Marker.TrackIndex))
		{
			Error = FString::Printf(TEXT("Invalid/out-of-range sync marker %s at %.9g."), *Marker.MarkerName.ToString(), Marker.Time);
			return false;
		}
	}
	for (const FAnimNotifyEvent& Event : SourceNotifies)
	{
		const float Time = Event.GetTime();
		const float Duration = Event.GetDuration();
		if (!FMath::IsFinite(Time) || !FMath::IsFinite(Duration) || Time < 0.0f || Duration < 0.0f
			|| Time + Duration > DestLength + UE_KINDA_SMALL_NUMBER
			|| !Source->AnimNotifyTracks.IsValidIndex(Event.TrackIndex))
		{
			Error = FString::Printf(TEXT("Invalid/out-of-range notify %s at %.9g, duration %.9g."), *Event.NotifyName.ToString(), Time, Duration);
			return false;
		}
	}
	for (const FFloatCurve& Curve : SourceCurves)
	{
		// Engine controller has no default-value setter. All five audited Walk_F
		// curves use the normal default. Fail rather than silently lose a custom one.
		if (Curve.GetName().IsNone() || Curve.FloatCurve.DefaultValue != FRichCurve().DefaultValue)
		{
			Error = FString::Printf(TEXT("Unsupported unnamed curve or custom default value: %s."), *Curve.GetName().ToString());
			return false;
		}
	}

	Dest->Modify();
	Dest->bEnableRootMotion = Source->bEnableRootMotion;
	Dest->RootMotionRootLock = Source->RootMotionRootLock;
	Dest->bForceRootLock = Source->bForceRootLock;
	Dest->bUseNormalizedRootMotionScale = Source->bUseNormalizedRootMotionScale;
	Dest->RateScale = Source->RateScale;
	Dest->Interpolation = Source->Interpolation;

	// Track pointers are caches into each sequence's own arrays. Copy only names
	// and colors, then rebuild the caches. StandAction03 has tracks in order 2,1.
	Dest->AnimNotifyTracks.Reset(Source->AnimNotifyTracks.Num());
	for (const FAnimNotifyTrack& Track : Source->AnimNotifyTracks)
	{
		Dest->AnimNotifyTracks.Emplace(Track.TrackName, Track.TrackColor);
	}
	if (Dest->AnimNotifyTracks.IsEmpty())
	{
		Dest->AnimNotifyTracks.Emplace(FName(TEXT("1")), FLinearColor::White);
	}
	Dest->AuthoredSyncMarkers = SourceMarkers;
	Dest->Notifies.Reset(SourceNotifies.Num());
	for (const FAnimNotifyEvent& OldEvent : SourceNotifies)
	{
		const float Time = OldEvent.GetTime();
		const float Duration = OldEvent.GetDuration();
		FAnimNotifyEvent NewEvent = OldEvent; // Includes every trigger/filter/offset field.
		NewEvent.Notify = OldEvent.Notify ? DuplicateObject<UAnimNotify>(OldEvent.Notify.Get(), Dest) : nullptr;
		NewEvent.NotifyStateClass = OldEvent.NotifyStateClass
			? DuplicateObject<UAnimNotifyState>(OldEvent.NotifyStateClass.Get(), Dest) : nullptr;
		if ((OldEvent.Notify && !NewEvent.Notify) || (OldEvent.NotifyStateClass && !NewEvent.NotifyStateClass))
		{
			Error = FString::Printf(TEXT("Failed to duplicate notify instance %s."), *OldEvent.NotifyName.ToString());
			return false;
		}
		// Relink using DISPLAY time, not GetTriggerTime(): the latter would apply
		// TriggerTimeOffset twice. Preserve the source offsets and GUIDs verbatim.
		NewEvent.ChangeLinkMethod(EAnimLinkMethod::Absolute);
		NewEvent.EndLink.ChangeLinkMethod(EAnimLinkMethod::Absolute);
		NewEvent.Link(Dest, Time);
		NewEvent.SetDuration(Duration);
		NewEvent.EndLink.Link(Dest, Time + Duration);
		Dest->Notifies.Add(MoveTemp(NewEvent));
		if (!OldEvent.Notify && !OldEvent.NotifyStateClass && !OldEvent.NotifyName.IsNone())
		{
			Dest->GetSkeleton()->AddNewAnimationNotify(OldEvent.NotifyName);
		}
	}
	Dest->RefreshSyncMarkerDataFromAuthored();
	Dest->RefreshCacheData();

	// Only add/replace source-named float curves. Preserve any additional target
	// curves, every bone track (including Ribbon), import data, skeleton and length.
	IAnimationDataController& Controller = Dest->GetController();
	{
		IAnimationDataController::FScopedBracket Bracket(Controller,
			FText::FromString(TEXT("Copy authored Changli animation metadata")), false);
		for (const FFloatCurve& Curve : SourceCurves)
		{
			const FAnimationCurveIdentifier Id(Curve.GetName(), ERawCurveTrackTypes::RCT_Float);
			const bool bExists = DestModel->FindFloatCurve(Id) != nullptr;
			if ((!bExists && !Controller.AddCurve(Id, Curve.GetCurveTypeFlags(), false))
				|| !Controller.SetCurveFlags(Id, Curve.GetCurveTypeFlags(), false))
			{
				Error = FString::Printf(TEXT("Could not create/configure float curve %s."), *Curve.GetName().ToString());
				return false;
			}
			FCurveAttributes Attributes;
			Attributes.SetPreExtrapolation(Curve.FloatCurve.PreInfinityExtrap);
			Attributes.SetPostExtrapolation(Curve.FloatCurve.PostInfinityExtrap);
			// Pass complete FRichCurveKey structures, including constant/cubic modes,
			// tangent modes, tangents, tangent weights and duplicate-time keys.
			if (!Controller.SetCurveKeys(Id, Curve.FloatCurve.GetConstRefOfKeys(), false)
				|| !Controller.SetCurveAttributes(Id, Attributes, false)
				|| !Controller.SetCurveColor(Id, Curve.GetColor(), false)
				|| !Controller.SetCurveComment(Id, Curve.Comment, false))
			{
				Error = FString::Printf(TEXT("Could not copy complete float curve %s."), *Curve.GetName().ToString());
				return false;
			}
		}
	}
	// Normal controller change/bracket notifications invalidate derived animation
	// data. Do not PopulateWithExistingModel/SetPlayLength/replace the data model.
	Dest->PostEditChange();
	Dest->MarkPackageDirty();

	TArray<FName> AfterBoneTracks;
	DestModel->GetBoneTrackNames(AfterBoneTracks);
	if (AfterBoneTracks != OriginalBoneTracks || DestModel->GetNumberOfFrames() != DestFrames
		|| DestModel->GetFrameRate() != DestRate || Dest->GetPlayLength() != DestLength
		|| Dest->GetSkeleton() != DestSkeleton)
	{
		Error = TEXT("Unexpected destination track/length/frame-rate/skeleton change; do not save.");
		return false;
	}
	for (const FFloatCurve& Curve : SourceCurves)
	{
		const FAnimationCurveIdentifier Id(Curve.GetName(), ERawCurveTrackTypes::RCT_Float);
		const FFloatCurve* Copied = DestModel->FindFloatCurve(Id);
		if (!Copied || Copied->GetCurveTypeFlags() != Curve.GetCurveTypeFlags()
			|| Copied->GetColor() != Curve.GetColor() || Copied->Comment != Curve.Comment
			|| Copied->FloatCurve.PreInfinityExtrap != Curve.FloatCurve.PreInfinityExtrap
			|| Copied->FloatCurve.PostInfinityExtrap != Curve.FloatCurve.PostInfinityExtrap
			|| Copied->FloatCurve.GetConstRefOfKeys().Num() != Curve.FloatCurve.GetConstRefOfKeys().Num())
		{
			Error = FString::Printf(TEXT("Float curve count/flags verification failed: %s."), *Curve.GetName().ToString());
			return false;
		}
		const auto& Keys = Curve.FloatCurve.GetConstRefOfKeys();
		const auto& Actual = Copied->FloatCurve.GetConstRefOfKeys();
		for (int32 Index = 0; Index < Keys.Num(); ++Index)
		{
			const FRichCurveKey& A = Keys[Index];
			const FRichCurveKey& B = Actual[Index];
			// Sequencer stores frame times (up to 240 kHz); allow only its tiny time
			// conversion error. Interpolation modes remain exact, with no resampling.
			if (A.InterpMode != B.InterpMode || A.TangentMode != B.TangentMode || A.TangentWeightMode != B.TangentWeightMode
				|| !FMath::IsNearlyEqual(A.Time, B.Time, 0.00001f) || !FMath::IsNearlyEqual(A.Value, B.Value, 0.00001f)
				|| !FMath::IsNearlyEqual(A.ArriveTangent, B.ArriveTangent, 0.0001f)
				|| !FMath::IsNearlyEqual(A.LeaveTangent, B.LeaveTangent, 0.0001f)
				|| !FMath::IsNearlyEqual(A.ArriveTangentWeight, B.ArriveTangentWeight, 0.0001f)
				|| !FMath::IsNearlyEqual(A.LeaveTangentWeight, B.LeaveTangentWeight, 0.0001f))
			{
				Error = FString::Printf(TEXT("Rich curve key verification failed: %s key %d; do not save."), *Curve.GetName().ToString(), Index);
				return false;
			}
		}
	}
	return true;
}

#endif // WITH_EDITOR
