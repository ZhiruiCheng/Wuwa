#include "Editor/Commandlets/WuwaFormationInputSetupCommandlet.h"

#if WITH_EDITOR
#include "EnhancedActionKeyMapping.h"
#include "Game/Common/WuwaGameTags.h"
#include "Game/Input/DataAsset/WuwaInputDataAsset.h"
#include "HAL/FileManager.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaFormationInputSetup, Log, All);

UWuwaFormationInputSetupCommandlet::UWuwaFormationInputSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpDescription = TEXT("Audits the formation switch input (keys 1/2/3); -Apply maps IA_SwitchChar_1..3 in IMC_Character and adds their rows to DA_IARouterMap.");
	HelpUsage = TEXT("-run=WuwaFormationInputSetup [-Apply]");
}

#if WITH_EDITOR
namespace WuwaFormationInputSetup
{
	constexpr const TCHAR* ContextPath = TEXT("/Game/CoreInput/Contexts/IMC_Character.IMC_Character");
	constexpr const TCHAR* BindingsPath = TEXT("/Game/CoreInput/DataAsset/DA_IARouterMap.DA_IARouterMap");

	struct FSlot
	{
		const TCHAR* ActionPath;
		FKey Key;
		FGameplayTag InputTag;
		UInputAction* Action = nullptr;
	};

	TArray<FSlot> MakeSlots()
	{
		const FWuwaGameTags& Tags = FWuwaGameTags::Get();
		return {
			{ TEXT("/Game/CoreInput/Actions/IA_SwitchChar_1.IA_SwitchChar_1"), EKeys::One, Tags.Formation_SwitchtoOne },
			{ TEXT("/Game/CoreInput/Actions/IA_SwitchChar_2.IA_SwitchChar_2"), EKeys::Two, Tags.Formation_SwitchtoTwo },
			{ TEXT("/Game/CoreInput/Actions/IA_SwitchChar_3.IA_SwitchChar_3"), EKeys::Three, Tags.Formation_SwitchtoThree },
		};
	}

	bool IsDigitKey(const FKey& Key)
	{
		return Key == EKeys::One || Key == EKeys::Two || Key == EKeys::Three;
	}

	FString DescribeTriggers(const TArray<TObjectPtr<UInputTrigger>>& Triggers)
	{
		TArray<FString> Names;
		for (const UInputTrigger* Trigger : Triggers) Names.Add(GetNameSafe(Trigger ? Trigger->GetClass() : nullptr));
		return Names.IsEmpty() ? TEXT("none") : FString::Join(Names, TEXT(","));
	}

	void Audit(const TArray<FSlot>& Slots, const UInputMappingContext* Context, const UWuwaInputDataAsset* Bindings)
	{
		for (const FSlot& Slot : Slots)
		{
			UE_LOG(LogWuwaFormationInputSetup, Display, TEXT("Action %s: %s valueType=%d triggers=%s"), Slot.ActionPath,
				Slot.Action ? TEXT("found") : TEXT("MISSING"), Slot.Action ? static_cast<int32>(Slot.Action->ValueType) : -1,
				Slot.Action ? *DescribeTriggers(Slot.Action->Triggers) : TEXT("-"));
		}
		if (Context)
		{
			for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
			{
				const FString ActionName = GetPathNameSafe(Mapping.Action);
				if (IsDigitKey(Mapping.Key) || ActionName.Contains(TEXT("SwitchChar")) || !Mapping.Action)
				{
					UE_LOG(LogWuwaFormationInputSetup, Display, TEXT("IMC mapping: key=%s action=%s triggers=%s"), *Mapping.Key.ToString(),
						*ActionName, *DescribeTriggers(Mapping.Triggers));
				}
			}
		}
		if (Bindings)
		{
			for (const FInputDataAsset& Row : Bindings->InputDataAssetMap)
			{
				const FString ActionName = GetPathNameSafe(Row.InputAction);
				if (ActionName.Contains(TEXT("SwitchChar")) || Row.RouteTag == FWuwaGameTags::Get().Input_Route_Formation)
				{
					UE_LOG(LogWuwaFormationInputSetup, Display, TEXT("DA row: action=%s tag=%s route=%s"), *ActionName,
						*Row.InputTag.ToString(), *Row.RouteTag.ToString());
				}
			}
		}
	}

	bool Backup(const TArray<UObject*>& Assets)
	{
		const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/FormationInputSetup")
			/ FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
		for (const UObject* Asset : Assets)
		{
			const FString Source = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
			if (IFileManager::Get().Copy(*(Root / FPaths::GetCleanFilename(Source)), *Source) != COPY_OK)
			{
				UE_LOG(LogWuwaFormationInputSetup, Error, TEXT("Could not back up %s; nothing saved."), *Source);
				return false;
			}
		}
		UE_LOG(LogWuwaFormationInputSetup, Display, TEXT("Original assets backed up to %s"), *Root);
		return true;
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
			UE_LOG(LogWuwaFormationInputSetup, Error, TEXT("Could not save %s (is the editor open?)."), *Filename);
			return false;
		}
		UE_LOG(LogWuwaFormationInputSetup, Display, TEXT("Saved %s"), *Package->GetName());
		return true;
	}
}
#endif

int32 UWuwaFormationInputSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaFormationInputSetup;
	TArray<FSlot> Slots = MakeSlots();
	for (FSlot& Slot : Slots) Slot.Action = LoadObject<UInputAction>(nullptr, Slot.ActionPath);
	UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, ContextPath);
	UWuwaInputDataAsset* Bindings = LoadObject<UWuwaInputDataAsset>(nullptr, BindingsPath);

	Audit(Slots, Context, Bindings);
	if (!FParse::Param(*Params, TEXT("Apply")))
	{
		UE_LOG(LogWuwaFormationInputSetup, Display, TEXT("Read-only audit complete. No assets saved. Use -Apply to configure keys 1/2/3."));
		return 0;
	}

	if (!Context || !Bindings || !FWuwaGameTags::Get().Input_Route_Formation.IsValid())
	{
		UE_LOG(LogWuwaFormationInputSetup, Error, TEXT("IMC, DA or the Input.Route.Formation tag is missing; nothing saved."));
		return 1;
	}
	for (const FSlot& Slot : Slots)
	{
		if (!Slot.Action || !Slot.InputTag.IsValid())
		{
			UE_LOG(LogWuwaFormationInputSetup, Error, TEXT("%s or its Formation tag is missing; nothing saved."), Slot.ActionPath);
			return 1;
		}
	}
	if (!Backup({ Context, Bindings })) return 1;

	// IMC 已经是"每个切人 IA 只绑自己的数字键、数字键不绑别的动作"就不动，保留映射上已有的设置。
	bool bContextReady = true;
	for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
	{
		const FSlot* Slot = Slots.FindByPredicate([&Mapping](const FSlot& S) { return S.Action == Mapping.Action; });
		if ((Slot && Mapping.Key != Slot->Key) || (!Slot && IsDigitKey(Mapping.Key)))
		{
			bContextReady = false;
		}
	}
	for (const FSlot& Slot : Slots)
	{
		bContextReady &= Context->GetMappings().ContainsByPredicate([&Slot](const FEnhancedActionKeyMapping& M) { return M.Action == Slot.Action && M.Key == Slot.Key; });
	}
	if (bContextReady)
	{
		UE_LOG(LogWuwaFormationInputSetup, Display, TEXT("IMC already maps keys 1/2/3 correctly; left unchanged."));
	}

	if (!bContextReady)
	{
		// IMC：每个切人 IA 只绑定自己的数字键；数字键原来绑在别的动作上的映射也移走，避免一个键同时触发两件事。
		for (const FSlot& Slot : Slots)
		{
			Context->UnmapAllKeysFromAction(Slot.Action);
		}
		TArray<TPair<const UInputAction*, FKey>> Stale;
		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			if (IsDigitKey(Mapping.Key))
			{
				Stale.Emplace(Mapping.Action, Mapping.Key);
			}
		}
		for (const TPair<const UInputAction*, FKey>& Pair : Stale)
		{
			UE_LOG(LogWuwaFormationInputSetup, Display, TEXT("Removing old mapping %s -> %s"), *Pair.Value.ToString(), *GetPathNameSafe(Pair.Key));
			Context->UnmapKey(Pair.Key, Pair.Value);
		}
		for (const FSlot& Slot : Slots)
		{
			Context->MapKey(Slot.Action, Slot.Key);
		}
	}

	// DA：同一个 IA 只保留一行，InputTag = Formation.SwitchtoN，RouteTag = Input.Route.Formation。
	const FGameplayTag Route = FWuwaGameTags::Get().Input_Route_Formation;
	for (const FSlot& Slot : Slots)
	{
		const UInputAction* Action = Slot.Action;
		Bindings->InputDataAssetMap.RemoveAll([Action](const FInputDataAsset& Row) { return Row.InputAction == Action; });
		FInputDataAsset Row;
		Row.InputAction = Action;
		Row.InputTag = Slot.InputTag;
		Row.RouteTag = Route;
		Bindings->InputDataAssetMap.Add(Row);
	}

	if ((!bContextReady && !Save(Context)) || !Save(Bindings)) return 1;
	Audit(Slots, Context, Bindings);
	UE_LOG(LogWuwaFormationInputSetup, Display, TEXT("Formation input setup complete."));
	return 0;
#else
	return 1;
#endif
}
