#include "Editor/Commandlets/WuwaAudioNotifySetupCommandlet.h"
#include "Editor/WuwaAnimNotifyEditorUtils.h"

#if WITH_EDITOR
#include "Animation/AnimSequenceBase.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_AudioEvent.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_Footstep.h"
#include "Game/Animation/Notifies/WuwaAnimNotifyState_AudioEvent.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaAudioNotifySetup, Log, All);

UWuwaAudioNotifySetupCommandlet::UWuwaAudioNotifySetupCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	HelpUsage = TEXT("-run=WuwaAudioNotifySetup -Character=changli [-Apply]");
}

#if WITH_EDITOR
namespace WuwaAudioNotifySetup
{
	const FName TrackName(TEXT("WwiseAudio"));

	bool IsOurs(const FAnimNotifyEvent& Event)
	{
		return (Event.Notify && (Event.Notify->IsA<UWuwaAnimNotify_AudioEvent>() || Event.Notify->IsA<UWuwaAnimNotify_Footstep>()))
			|| (Event.NotifyStateClass && Event.NotifyStateClass->IsA<UWuwaAnimNotifyState_AudioEvent>());
	}

	bool Backup(UObject* Asset, const FString& Root)
	{
		FString Relative = Asset->GetOutermost()->GetName();
		if (!Relative.RemoveFromStart(TEXT("/Game/"))) return false;
		const FString Source = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		const FString Destination = Root / (Relative + FPackageName::GetAssetPackageExtension());
		return IFileManager::Get().MakeDirectory(*FPaths::GetPath(Destination), true)
			&& IFileManager::Get().Copy(*Destination, *Source, false, true) == COPY_OK;
	}

	bool Save(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		Args.Error = GWarn;
		Asset->MarkPackageDirty();
		if (!UPackage::SavePackage(Package, Asset, *Filename, Args))
		{
			UE_LOG(LogWuwaAudioNotifySetup, Error, TEXT("Could not save %s (is the editor open with it loaded?)"), *Filename);
			return false;
		}
		UE_LOG(LogWuwaAudioNotifySetup, Display, TEXT("Saved %s"), *Package->GetName());
		return true;
	}

	/** /Game/WwiseAudio 下所有 Wwise 事件资产：名字 → 软路径。 */
	TMap<FString, FSoftObjectPath> FindEvents()
	{
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.ScanPathsSynchronous({ TEXT("/Game/WwiseAudio") }, true);
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPath(TEXT("/Game/WwiseAudio"), Assets, true);
		TMap<FString, FSoftObjectPath> Events;
		for (const FAssetData& Asset : Assets)
		{
			if (Asset.AssetClassPath.GetAssetName() == TEXT("AkAudioEvent")) Events.Add(Asset.AssetName.ToString(), Asset.GetSoftObjectPath());
		}
		return Events;
	}
}
#endif

int32 UWuwaAudioNotifySetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaAudioNotifySetup;
	FString Character = TEXT("changli");
	FParse::Value(*Params, TEXT("Character="), Character);
	const FString Title = Character.Left(1).ToUpper() + Character.Mid(1).ToLower();
	const FString RecipePath = FPaths::ProjectDir() / TEXT("ArtSource/Audio") / Title / TEXT("audio.json");
	FString Text;
	TSharedPtr<FJsonObject> Recipe;
	if (!FFileHelper::LoadFileToString(Text, *RecipePath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Recipe) || !Recipe)
	{
		UE_LOG(LogWuwaAudioNotifySetup, Error, TEXT("Cannot read %s (run ArtSource/Audio/prepare_audio.py first)"), *RecipePath);
		return 1;
	}
	const TMap<FString, FSoftObjectPath> Events = FindEvents();

	// 按动画资产分组
	TMap<FString, TArray<TSharedPtr<FJsonObject>>> ByAsset;
	for (const TSharedPtr<FJsonValue>& Value : Recipe->GetArrayField(TEXT("notifies")))
	{
		const TSharedPtr<FJsonObject> Entry = Value->AsObject();
		if (!Events.Contains(Entry->GetStringField(TEXT("event"))))
		{
			UE_LOG(LogWuwaAudioNotifySetup, Error, TEXT("Wwise event asset %s not found under /Game/WwiseAudio (generate SoundBanks, then run WwiseReconcileCommandlet)."),
				*Entry->GetStringField(TEXT("event")));
			return 1;
		}
		ByAsset.FindOrAdd(Entry->GetStringField(TEXT("asset"))).Add(Entry);
	}

	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	const FString BackupRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/AudioNotifySetup")
		/ FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	int32 Installed = 0, Removed = 0, Skipped = 0;
	for (const auto& Pair : ByAsset)
	{
		UAnimSequenceBase* Anim = LoadObject<UAnimSequenceBase>(nullptr, *Pair.Key);
		if (!Anim)
		{
			UE_LOG(LogWuwaAudioNotifySetup, Error, TEXT("Missing animation %s"), *Pair.Key);
			return 1;
		}
		// 脚步：配方给出原作共用移动动画的两套候选（BaseAnim / BaseAnim3），按本动画的实际时长选一套：
		// 比值 1，或整段拉长 1.25 倍（24 帧导入的 30 帧原作动画）；都符合时优先比值 1，再优先名字提示的那套。脚步时刻按比值缩放。
		TArray<TSharedPtr<FJsonObject>> Entries;
		for (const TSharedPtr<FJsonObject>& Entry : Pair.Value)
		{
			const TSharedPtr<FJsonObject>* Sources = nullptr;
			if (!Entry->TryGetObjectField(TEXT("sources"), Sources))
			{
				Entries.Add(Entry);
				continue;
			}
			FString Best;
			double BestRatio = 0.0, BestScore = TNumericLimits<double>::Max();
			for (const auto& Source : (*Sources)->Values)
			{
				const double Ratio = Anim->GetPlayLength() / Source.Value->AsObject()->GetNumberField(TEXT("length"));
				for (const double Allowed : { 1.0, 1.25 })
				{
					if (FMath::Abs(Ratio - Allowed) > 0.03) continue;
					const double Score = (Allowed == 1.0 ? 0.0 : 2.0) + (Source.Key == Entry->GetStringField(TEXT("hint")) ? 0.0 : 1.0);
					if (Score < BestScore) { BestScore = Score; Best = Source.Key; BestRatio = Ratio; }
				}
			}
			if (Best.IsEmpty())
			{
				UE_LOG(LogWuwaAudioNotifySetup, Warning, TEXT("Skip %s on %s: length %.3fs matches no %s"), *Entry->GetStringField(TEXT("kind")),
					*Anim->GetName(), Anim->GetPlayLength(), *Entry->GetStringField(TEXT("sourceAsset")));
				continue;
			}
			for (const TSharedPtr<FJsonValue>& Step : (*Sources)->GetObjectField(Best)->GetArrayField(TEXT("steps")))
			{
				const TSharedPtr<FJsonObject> Concrete = MakeShared<FJsonObject>(*Entry);
				const TSharedPtr<FJsonObject> Source = Step->AsObject();
				Concrete->SetNumberField(TEXT("time"), Source->GetNumberField(TEXT("time")) * BestRatio);
				FString Variant;
				Source->TryGetStringField(TEXT("variant"), Variant);
				Concrete->SetStringField(TEXT("switchValue"), Variant);
				// 衣服拟音窗口：时长同样缩放；淡出参数照原作
				double Duration = 0.0, FadeMs = 500.0, FadeCurve = 4.0;
				if (Source->TryGetNumberField(TEXT("duration"), Duration)) Concrete->SetNumberField(TEXT("duration"), Duration * BestRatio);
				if (Source->TryGetNumberField(TEXT("fadeMs"), FadeMs)) Concrete->SetNumberField(TEXT("fadeMs"), FadeMs);
				if (Source->TryGetNumberField(TEXT("fadeCurve"), FadeCurve)) Concrete->SetNumberField(TEXT("fadeCurve"), FadeCurve);
				Concrete->SetStringField(TEXT("sourceAsset"), Entry->GetStringField(TEXT("sourceAsset")).Replace(TEXT("*"), *Best)
					+ (FMath::IsNearlyEqual(BestRatio, 1.0, 0.03) ? TEXT("") : FString::Printf(TEXT(" x%.2f"), BestRatio)));
				Entries.Add(Concrete);
			}
		}
		if (Entries.IsEmpty())
		{
			++Skipped;
			continue;
		}
		for (const TSharedPtr<FJsonObject>& Entry : Entries)
		{
			UE_LOG(LogWuwaAudioNotifySetup, Display, TEXT("%s @ %.3fs%s: %s (%s)"), *Anim->GetName(), Entry->GetNumberField(TEXT("time")),
				Entry->GetStringField(TEXT("kind")) == TEXT("state") ? *FString::Printf(TEXT(" for %.3fs"), Entry->GetNumberField(TEXT("duration"))) : TEXT(""),
				*Entry->GetStringField(TEXT("event")), *Entry->GetStringField(TEXT("sourceAsset")));
		}
		if (!bApply) continue;
		if (!Backup(Anim, BackupRoot))
		{
			UE_LOG(LogWuwaAudioNotifySetup, Error, TEXT("Could not back up %s"), *Anim->GetPathName());
			return 1;
		}

		Anim->Modify();
		Removed += Anim->Notifies.RemoveAll([](const FAnimNotifyEvent& Event) { return IsOurs(Event); });
		int32 Track = Anim->AnimNotifyTracks.IndexOfByPredicate([](const FAnimNotifyTrack& T) { return T.TrackName == TrackName; });
		if (Track == INDEX_NONE)
		{
			Track = Anim->AnimNotifyTracks.AddDefaulted();
			Anim->AnimNotifyTracks[Track].TrackName = TrackName;
			Anim->AnimNotifyTracks[Track].TrackColor = FLinearColor(.2f, .6f, 1.f);
		}
		for (const TSharedPtr<FJsonObject>& Entry : Entries)
		{
			const FSoftObjectPath Event = Events[Entry->GetStringField(TEXT("event"))];
			FString Socket;
			Entry->TryGetStringField(TEXT("socket"), Socket);
			bool bFollow = true;
			Entry->TryGetBoolField(TEXT("follow"), bFollow);
			const float Time = static_cast<float>(Entry->GetNumberField(TEXT("time")));
			FAnimNotifyEvent& Notify = Anim->Notifies.AddDefaulted_GetRef();
			Notify.TrackIndex = Track;
			Notify.NotifyTriggerChance = 1.f;
			Notify.TriggerTimeOffset = 0.f;
			Notify.Guid = FGuid::NewGuid();
			Notify.NotifyName = FName(*FString::Printf(TEXT("Audio.%s"), *Entry->GetStringField(TEXT("event"))));
			if (Entry->GetStringField(TEXT("kind")) == TEXT("footstep"))
			{
				auto* Footstep = NewObject<UWuwaAnimNotify_Footstep>(Anim, NAME_None, RF_Transactional);
				Footstep->AudioEvent = Event;
				Footstep->SwitchGroup = FName(*Entry->GetStringField(TEXT("switchGroup")));
				Footstep->Variant = FName(*Entry->GetStringField(TEXT("switchValue")));
				Footstep->SocketName = Socket.IsEmpty() ? NAME_None : FName(*Socket);
				Notify.Notify = Footstep;
				Notify.NotifyName = FName(*FString::Printf(TEXT("Footstep.%s"), *Entry->GetStringField(TEXT("switchValue"))));
				Notify.Link(Anim, Time);
			}
			else if (Entry->GetStringField(TEXT("kind")) == TEXT("state") || Entry->GetStringField(TEXT("kind")) == TEXT("foley"))
			{
				auto* State = NewObject<UWuwaAnimNotifyState_AudioEvent>(Anim, NAME_None, RF_Transactional);
				FString SwitchValue;
				if (Entry->TryGetStringField(TEXT("switchValue"), SwitchValue) && !SwitchValue.IsEmpty())
				{
					State->SwitchGroup = FName(*Entry->GetStringField(TEXT("switchGroup")));
					State->SwitchValue = FName(*SwitchValue);
				}
				if (Entry->GetStringField(TEXT("kind")) == TEXT("foley"))
				{
					Notify.NotifyName = FName(*FString::Printf(TEXT("Foley.%s"), SwitchValue.IsEmpty() ? TEXT("keep") : *SwitchValue));
				}
				State->AudioEvent = Event;
				State->SocketName = Socket.IsEmpty() ? NAME_None : FName(*Socket);
				State->bFollow = bFollow;
				Entry->TryGetBoolField(TEXT("keepAlive"), State->bKeepAlive);
				State->FadeDuration = static_cast<float>(Entry->GetNumberField(TEXT("fadeMs"))) / 1000.f;
				State->FadeCurve = static_cast<int32>(Entry->GetNumberField(TEXT("fadeCurve")));
				FString Trailing;
				if (Entry->TryGetStringField(TEXT("trailingEvent"), Trailing) && Events.Contains(Trailing)) State->TrailingAudioEvent = Events[Trailing];
				const float Duration = static_cast<float>(Entry->GetNumberField(TEXT("duration")));
				Notify.NotifyStateClass = State;
				Notify.Link(Anim, Time);
				Notify.SetDuration(Duration);
				Notify.EndLink.Link(Anim, Time + Duration);
			}
			else
			{
				auto* Instant = NewObject<UWuwaAnimNotify_AudioEvent>(Anim, NAME_None, RF_Transactional);
				Instant->AudioEvent = Event;
				Instant->SocketName = Socket.IsEmpty() ? NAME_None : FName(*Socket);
				Instant->bFollow = bFollow;
				Notify.Notify = Instant;
				Notify.Link(Anim, Time);
			}
			++Installed;
		}
		Anim->SortNotifies();
		WuwaAnimNotifyEditor::RefreshTriggerOffsets(Anim);
		Anim->RefreshCacheData();
		if (!Save(Anim)) return 1;
	}
	UE_LOG(LogWuwaAudioNotifySetup, Display, TEXT("%s: %d audio notifies on %d animations (%d replaced, %d animations skipped)%s."), *Title,
		bApply ? Installed : Recipe->GetArrayField(TEXT("notifies")).Num(), ByAsset.Num() - Skipped, Removed, Skipped,
		bApply ? TEXT("") : TEXT(" — audit only, run with -Apply"));
	return 0;
#else
	return 1;
#endif
}
