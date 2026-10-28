#include "Editor/Commandlets/WuwaMoveInputSetupCommandlet.h"

#if WITH_EDITOR
#include "Core/Tag/WuwaGameTags.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaMoveInputConfig.h"
#include "Game/NewWorld/Character/Common/Component/Input/MoveActions/WuwaMoveInputAction_ToggleWalkPreference.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaMoveInputSetup, Log, All);

UWuwaMoveInputSetupCommandlet::UWuwaMoveInputSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpDescription = TEXT("Audits the move input config; -Apply creates it (WalkRun -> toggle walk preference) when missing.");
	HelpUsage = TEXT("-run=WuwaMoveInputSetup [-Apply]");
}

#if WITH_EDITOR
namespace WuwaMoveInputSetup
{
	// 与 UWuwaMoveInputHandler 构造函数里的默认软路径一致。
	constexpr const TCHAR* ConfigPackage = TEXT("/Game/CoreInput/DataAsset/DA_WuwaMoveInputConfig");
	constexpr const TCHAR* ConfigPath = TEXT("/Game/CoreInput/DataAsset/DA_WuwaMoveInputConfig.DA_WuwaMoveInputConfig");

	bool HasWalkToggle(const UWuwaMoveInputConfig& Config)
	{
		const FWuwaMoveInputBinding* Binding = Config.FindBinding(FWuwaGameTags::Get().Player_Common_Movement_WalkRun, EWuwaInputPhase::Pressed);
		return Binding && Binding->Action && Binding->Action->IsA<UWuwaMoveInputAction_ToggleWalkPreference>();
	}
}
#endif

int32 UWuwaMoveInputSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaMoveInputSetup;
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	UObject* Existing = LoadObject<UObject>(nullptr, ConfigPath, nullptr, LOAD_NoWarn);
	if (Existing)
	{
		const UWuwaMoveInputConfig* Config = Cast<UWuwaMoveInputConfig>(Existing);
		if (!Config)
		{
			UE_LOG(LogWuwaMoveInputSetup, Error, TEXT("%s exists but is a %s, not a move input config. Nothing saved."), ConfigPath, *Existing->GetClass()->GetName());
			return 1;
		}
		UE_LOG(LogWuwaMoveInputSetup, Display, TEXT("%s exists with %d binding(s); WalkRun -> toggle walk preference: %s. Existing assets are never overwritten."),
			ConfigPath, Config->Bindings.Num(), HasWalkToggle(*Config) ? TEXT("yes") : TEXT("NO"));
		return HasWalkToggle(*Config) ? 0 : 1;
	}
	if (!bApply)
	{
		UE_LOG(LogWuwaMoveInputSetup, Display, TEXT("%s is missing. Run with -Apply to create it. Nothing saved."), ConfigPath);
		return 0;
	}

	UPackage* Package = CreatePackage(ConfigPackage);
	UWuwaMoveInputConfig* Config = NewObject<UWuwaMoveInputConfig>(Package, *FPackageName::GetShortName(ConfigPackage), RF_Public | RF_Standalone);
	FWuwaMoveInputBinding& WalkToggle = Config->Bindings.AddDefaulted_GetRef();
	WalkToggle.InputTag = FWuwaGameTags::Get().Player_Common_Movement_WalkRun;
	WalkToggle.Phase = EWuwaInputPhase::Pressed;
	// 指令对象作为配置表的内嵌子对象保存。
	WalkToggle.Action = NewObject<UWuwaMoveInputAction_ToggleWalkPreference>(Config);

	const FString Filename = FPackageName::LongPackageNameToFilename(ConfigPackage, FPackageName::GetAssetPackageExtension());
	if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true)) return 1;
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	Args.Error = GWarn;
	Config->MarkPackageDirty();
	if (!UPackage::SavePackage(Package, Config, *Filename, Args))
	{
		UE_LOG(LogWuwaMoveInputSetup, Error, TEXT("Could not save %s."), *Filename);
		return 1;
	}
	UE_LOG(LogWuwaMoveInputSetup, Display, TEXT("Created %s: WalkRun (Pressed) -> toggle walk preference."), ConfigPath);
	return 0;
#else
	UE_LOG(LogWuwaMoveInputSetup, Error, TEXT("WuwaMoveInputSetup requires an editor build."));
	return 1;
#endif
}
