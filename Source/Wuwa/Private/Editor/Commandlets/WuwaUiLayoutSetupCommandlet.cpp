#include "Editor/Commandlets/WuwaUiLayoutSetupCommandlet.h"

#if WITH_EDITOR
#include "Core/ActorComponent/UIItem.h"
#include "HAL/FileManager.h"
#include "Layout/LGUICanvasScaler.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "PrefabSystem/LGUIPrefab.h"
#include "PrefabSystem/LGUIPrefabHelperObject.h"
#include "PrefabSystem/LGUIPrefabManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaUiLayoutSetup, Log, All);

UWuwaUiLayoutSetupCommandlet::UWuwaUiLayoutSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpDescription = TEXT("Audits UI full-screen layout; -Apply sets LP_UIRoot's canvas scaler to ScaleWithScreenSize 1920x1080 (match height) and stretches the HUD_Main root.");
	HelpUsage = TEXT("-run=WuwaUiLayoutSetup [-Apply]");
}

#if WITH_EDITOR
namespace WuwaUiLayoutSetup
{
	constexpr const TCHAR* UiRootPath = TEXT("/Game/LGUI/LP_UIRoot.LP_UIRoot");
	constexpr const TCHAR* HudMainPath = TEXT("/Game/LGUI/HUD/HUD_Main.HUD_Main");
	const FVector2D ReferenceResolution(1920.f, 1080.f);
	constexpr float MatchHeight = 1.f;

	/** 用 LGUI 预制体编辑器同一套对象加载：保留 GUID 和嵌套子预制体，保存时原样写回。 */
	struct FLoadedPrefab
	{
		ULGUIPrefab* Asset = nullptr;
		ULGUIPrefabHelperObject* Helper = nullptr;
		AActor* Root = nullptr;

		explicit FLoadedPrefab(const TCHAR* Path)
		{
			Asset = LoadObject<ULGUIPrefab>(nullptr, Path);
			if (!Asset)
			{
				UE_LOG(LogWuwaUiLayoutSetup, Error, TEXT("Cannot load %s"), Path);
				return;
			}
			Helper = NewObject<ULGUIPrefabHelperObject>(GetTransientPackage());
			Helper->AddToRoot();
			Helper->PrefabAsset = Asset;
			Helper->LoadPrefab(ULGUIPrefabManagerObject::GetPreviewWorldForPrefabPackage(), nullptr);
			Root = Helper->LoadedRootActor;
			if (!Root)
			{
				UE_LOG(LogWuwaUiLayoutSetup, Error, TEXT("Cannot instantiate %s"), Path);
			}
		}

		~FLoadedPrefab()
		{
			if (Helper)
			{
				Helper->ClearLoadedPrefab();
				Helper->RemoveFromRoot();
			}
		}

		template <typename T>
		T* FindInHierarchy() const
		{
			TArray<AActor*> Actors;
			if (Root)
			{
				Actors.Add(Root);
				Root->GetAttachedActors(Actors, false, true);
			}
			for (AActor* Actor : Actors)
			{
				if (T* Component = Actor ? Actor->FindComponentByClass<T>() : nullptr)
				{
					return Component;
				}
			}
			return nullptr;
		}
	};

	bool Backup(const FString& Root, const ULGUIPrefab* Asset)
	{
		const FString Source = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		if (IFileManager::Get().Copy(*(Root / FPaths::GetCleanFilename(Source)), *Source) != COPY_OK)
		{
			UE_LOG(LogWuwaUiLayoutSetup, Error, TEXT("Could not back up %s; nothing saved."), *Source);
			return false;
		}
		return true;
	}

	bool Save(ULGUIPrefabHelperObject* Helper)
	{
		// 先让 LGUI 把场景里的对象序列化回预制体资产，再把资产包写到磁盘。
		Helper->SavePrefab();
		UPackage* Package = Helper->PrefabAsset->GetOutermost();
		Package->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		Args.Error = GWarn;
		if (!UPackage::SavePackage(Package, Helper->PrefabAsset, *Filename, Args))
		{
			UE_LOG(LogWuwaUiLayoutSetup, Error, TEXT("Could not save %s (is the editor open?)."), *Filename);
			return false;
		}
		UE_LOG(LogWuwaUiLayoutSetup, Display, TEXT("Saved %s"), *Package->GetName());
		return true;
	}

	bool IsScalerReady(ULGUICanvasScaler* Scaler)
	{
		return Scaler->GetUIScaleMode() == ELGUICanvasScaleMode::ScaleWithScreenSize
			&& Scaler->GetScreenMatchMode() == ELGUICanvasScreenMatchMode::MatchWidthOrHeight
			&& Scaler->GetReferenceResolution().Equals(ReferenceResolution)
			&& FMath::IsNearlyEqual(Scaler->GetMatchFromWidthToHeight(), MatchHeight);
	}

	bool IsStretched(const UUIItem* Item)
	{
		return Item->GetAnchorMin().Equals(FVector2D(0.f, 0.f)) && Item->GetAnchorMax().Equals(FVector2D(1.f, 1.f))
			&& Item->GetSizeDelta().IsNearlyZero() && Item->GetAnchoredPosition().IsNearlyZero();
	}
}
#endif

int32 UWuwaUiLayoutSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaUiLayoutSetup;
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	const FString BackupRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/UiLayoutSetup")
		/ FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	bool bOk = true;

	// —— LP_UIRoot：根画布缩放 ——
	{
		FLoadedPrefab UiRoot(UiRootPath);
		ULGUICanvasScaler* Scaler = UiRoot.FindInHierarchy<ULGUICanvasScaler>();
		if (!Scaler)
		{
			UE_LOG(LogWuwaUiLayoutSetup, Error, TEXT("LP_UIRoot has no LGUICanvasScaler; add one on the root canvas actor in the editor."));
			bOk = false;
		}
		else
		{
			UE_LOG(LogWuwaUiLayoutSetup, Display, TEXT("LP_UIRoot scaler on %s: mode=%d match=%d reference=%s matchValue=%.2f"),
				*GetNameSafe(Scaler->GetOwner()), static_cast<int32>(Scaler->GetUIScaleMode()), static_cast<int32>(Scaler->GetScreenMatchMode()),
				*Scaler->GetReferenceResolution().ToString(), Scaler->GetMatchFromWidthToHeight());
			if (IsScalerReady(Scaler))
			{
				UE_LOG(LogWuwaUiLayoutSetup, Display, TEXT("LP_UIRoot scaler already scales with the screen; left unchanged."));
			}
			else if (bApply)
			{
				if (!Backup(BackupRoot, UiRoot.Asset)) return 1;
				Scaler->SetUIScaleMode(ELGUICanvasScaleMode::ScaleWithScreenSize);
				Scaler->SetScreenMatchMode(ELGUICanvasScreenMatchMode::MatchWidthOrHeight);
				Scaler->SetReferenceResolution(ReferenceResolution);
				Scaler->SetMatchFromWidthToHeight(MatchHeight);
				bOk &= Save(UiRoot.Helper);
			}
		}
	}

	// —— HUD_Main：根节点拉伸 ——
	{
		FLoadedPrefab Hud(HudMainPath);
		UUIItem* Root = Hud.Root ? Cast<UUIItem>(Hud.Root->GetRootComponent()) : nullptr;
		if (!Root)
		{
			UE_LOG(LogWuwaUiLayoutSetup, Error, TEXT("HUD_Main has no UIItem root."));
			bOk = false;
		}
		else
		{
			UE_LOG(LogWuwaUiLayoutSetup, Display, TEXT("HUD_Main root: anchorMin=%s anchorMax=%s size=%s pos=%s"),
				*Root->GetAnchorMin().ToString(), *Root->GetAnchorMax().ToString(), *Root->GetSizeDelta().ToString(), *Root->GetAnchoredPosition().ToString());
			if (IsStretched(Root))
			{
				UE_LOG(LogWuwaUiLayoutSetup, Display, TEXT("HUD_Main root already stretches; left unchanged."));
			}
			else if (bApply)
			{
				if (!Backup(BackupRoot, Hud.Asset)) return 1;
				FUIAnchorData Stretch;
				Stretch.Pivot = FVector2D(0.5f, 0.5f);
				Stretch.AnchorMin = FVector2D(0.f, 0.f);
				Stretch.AnchorMax = FVector2D(1.f, 1.f);
				Stretch.SizeDelta = FVector2D::ZeroVector;
				Stretch.AnchoredPosition = FVector2D::ZeroVector;
				Root->SetAnchorData(Stretch);
				bOk &= Save(Hud.Helper);
			}
		}
	}

	if (!bApply)
	{
		UE_LOG(LogWuwaUiLayoutSetup, Display, TEXT("Read-only audit complete. No assets saved. Use -Apply to change them."));
	}
	else if (bOk)
	{
		UE_LOG(LogWuwaUiLayoutSetup, Display, TEXT("UI layout setup complete. Backups in %s"), *BackupRoot);
	}
	return bOk ? 0 : 1;
#else
	return 1;
#endif
}
