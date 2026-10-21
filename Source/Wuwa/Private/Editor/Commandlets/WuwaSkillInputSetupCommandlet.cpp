#include "Editor/Commandlets/WuwaSkillInputSetupCommandlet.h"

#if WITH_EDITOR
#include "EnhancedActionKeyMapping.h"
#include "GameplayTagsManager.h"
#include "Game/Input/DataAsset/WuwaInputDataAsset.h"
#include "HAL/FileManager.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaSkillInputSetup, Log, All);

UWuwaSkillInputSetupCommandlet::UWuwaSkillInputSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpDescription = TEXT("Audits the skill keys (E/R/Q); -Apply adds IA_Skill1 / IA_Ultimate / IA_Echo1 rows to DA_IARouterMap.");
	HelpUsage = TEXT("-run=WuwaSkillInputSetup [-Apply]");
}

#if WITH_EDITOR
namespace WuwaSkillInputSetup
{
	constexpr const TCHAR* ContextPath = TEXT("/Game/CoreInput/Contexts/IMC_Character.IMC_Character");
	constexpr const TCHAR* BindingsPath = TEXT("/Game/CoreInput/DataAsset/DA_IARouterMap.DA_IARouterMap");
	constexpr const TCHAR* RouteTag = TEXT("Input.Route.Ability");

	struct FSlot
	{
		const TCHAR* ActionPath;
		const TCHAR* InputTag;
		UInputAction* Action = nullptr;
	};

	bool Backup(const UObject* Asset)
	{
		const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/SkillInputSetup")
			/ FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
		const FString Source = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		if (IFileManager::Get().Copy(*(Root / FPaths::GetCleanFilename(Source)), *Source) != COPY_OK)
		{
			UE_LOG(LogWuwaSkillInputSetup, Error, TEXT("Could not back up %s; nothing saved."), *Source);
			return false;
		}
		UE_LOG(LogWuwaSkillInputSetup, Display, TEXT("Original asset backed up to %s"), *Root);
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
			UE_LOG(LogWuwaSkillInputSetup, Error, TEXT("Could not save %s (is the editor open?)."), *Filename);
			return false;
		}
		UE_LOG(LogWuwaSkillInputSetup, Display, TEXT("Saved %s"), *Package->GetName());
		return true;
	}
}
#endif

int32 UWuwaSkillInputSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaSkillInputSetup;
	FSlot Slots[] = {
		{TEXT("/Game/CoreInput/Actions/IA_Skill1.IA_Skill1"), TEXT("Input.Combat.Skill")},
		{TEXT("/Game/CoreInput/Actions/IA_Ultimate.IA_Ultimate"), TEXT("Input.Combat.Ultimate")},
		{TEXT("/Game/CoreInput/Actions/IA_Echo1.IA_Echo1"), TEXT("Input.Combat.Echo")},
	};
	const UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, ContextPath);
	UWuwaInputDataAsset* Bindings = LoadObject<UWuwaInputDataAsset>(nullptr, BindingsPath);
	const FGameplayTag Route = UGameplayTagsManager::Get().RequestGameplayTag(RouteTag, false);
	bool bReady = Context && Bindings && Route.IsValid();
	for (FSlot& Slot : Slots)
	{
		Slot.Action = LoadObject<UInputAction>(nullptr, Slot.ActionPath);
		const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(Slot.InputTag, false);
		const FEnhancedActionKeyMapping* Mapping = Context ? Context->GetMappings().FindByPredicate(
			[&Slot](const FEnhancedActionKeyMapping& M) { return M.Action == Slot.Action; }) : nullptr;
		const FInputDataAsset* Row = Bindings ? Bindings->InputDataAssetMap.FindByPredicate(
			[&Slot](const FInputDataAsset& R) { return R.InputAction == Slot.Action; }) : nullptr;
		UE_LOG(LogWuwaSkillInputSetup, Display, TEXT("%s: action %s, key %s, tag %s, router row %s"), Slot.ActionPath,
			Slot.Action ? TEXT("found") : TEXT("MISSING"), Mapping ? *Mapping->Key.ToString() : TEXT("none"),
			Tag.IsValid() ? Slot.InputTag : TEXT("NOT REGISTERED"), Row ? *FString::Printf(TEXT("%s / %s"), *Row->InputTag.ToString(), *Row->RouteTag.ToString()) : TEXT("none"));
		bReady &= Slot.Action && Tag.IsValid();
	}
	if (!FParse::Param(*Params, TEXT("Apply")))
	{
		UE_LOG(LogWuwaSkillInputSetup, Display, TEXT("Read-only audit complete. Use -Apply to add the router rows."));
		return 0;
	}
	if (!bReady)
	{
		UE_LOG(LogWuwaSkillInputSetup, Error, TEXT("IMC, DA, an input action or a tag is missing; nothing saved."));
		return 1;
	}
	if (!Backup(Bindings)) return 1;
	// 同一个 IA 只保留一行
	for (const FSlot& Slot : Slots)
	{
		const UInputAction* Action = Slot.Action;
		Bindings->InputDataAssetMap.RemoveAll([Action](const FInputDataAsset& R) { return R.InputAction == Action; });
		FInputDataAsset Row;
		Row.InputAction = Action;
		Row.InputTag = UGameplayTagsManager::Get().RequestGameplayTag(Slot.InputTag);
		Row.RouteTag = Route;
		Bindings->InputDataAssetMap.Add(Row);
	}
	return Save(Bindings) ? 0 : 1;
#else
	return 1;
#endif
}
