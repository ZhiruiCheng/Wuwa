#include "Editor/Commandlets/WuwaClimbTraceSetupCommandlet.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaClimbTraceSetup, Log, All);

UWuwaClimbTraceSetupCommandlet::UWuwaClimbTraceSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpUsage = TEXT("-run=WuwaClimbTraceSetup [-Apply]");
}

int32 UWuwaClimbTraceSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	const FString BackupRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/ClimbTraceSetup") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	const TEnumAsByte<EObjectTypeQuery> WorldStatic = UEngineTypes::ConvertToObjectType(ECC_WorldStatic);
	const TEnumAsByte<EObjectTypeQuery> WorldDynamic = UEngineTypes::ConvertToObjectType(ECC_WorldDynamic);
	for (const TCHAR* Path : {TEXT("/Game/Characters/Role/changli/BP_Role_Changli.BP_Role_Changli"), TEXT("/Game/Characters/Role/jiabei/BP_Role_jiabei.BP_Role_jiabei")})
	{
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, Path);
		AWuwaCharacter* Defaults = Blueprint && Blueprint->GeneratedClass ? Cast<AWuwaCharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
		UWuwaMovementComponent* Movement = Defaults ? Cast<UWuwaMovementComponent>(Defaults->GetCharacterMovement()) : nullptr;
		if (!Movement)
		{
			UE_LOG(LogWuwaClimbTraceSetup, Error, TEXT("No Wuwa movement component on %s"), Path);
			return 1;
		}
		const FString Current = FString::JoinBy(Movement->ObjectTypes, TEXT(", "), [](const TEnumAsByte<EObjectTypeQuery>& Type)
			{ return StaticEnum<ECollisionChannel>()->GetNameStringByValue(UEngineTypes::ConvertToCollisionChannel(Type.GetValue())); });
		if (Movement->ObjectTypes.Contains(WorldStatic))
		{
			UE_LOG(LogWuwaClimbTraceSetup, Display, TEXT("%s: ObjectTypes [%s] (kept)"), *Blueprint->GetName(), *Current);
			continue;
		}
		UE_LOG(LogWuwaClimbTraceSetup, Display, TEXT("%s: ObjectTypes [%s] -> [ECC_WorldStatic, ECC_WorldDynamic]"), *Blueprint->GetName(), *Current);
		if (!bApply) continue;
		const FString PackageName = Blueprint->GetOutermost()->GetName();
		FString Relative = PackageName;
		Relative.RemoveFromStart(TEXT("/Game/"));
		const FString Source = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		const FString Destination = BackupRoot / (Relative + FPackageName::GetAssetPackageExtension());
		if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Destination), true) || IFileManager::Get().Copy(*Destination, *Source, false, true) != COPY_OK)
		{
			UE_LOG(LogWuwaClimbTraceSetup, Error, TEXT("Could not back up %s"), *Source);
			return 1;
		}
		Movement->Modify();
		Movement->ObjectTypes = {WorldStatic, WorldDynamic};
		UPackage* Package = Blueprint->GetOutermost();
		Package->MarkPackageDirty();
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		Args.Error = GWarn;
		if (!UPackage::SavePackage(Package, Blueprint, *Source, Args))
		{
			UE_LOG(LogWuwaClimbTraceSetup, Error, TEXT("Could not save %s (is the editor open?)"), *Source);
			return 1;
		}
		UE_LOG(LogWuwaClimbTraceSetup, Display, TEXT("Saved %s (backup in %s)"), *PackageName, *BackupRoot);
	}
	return 0;
#else
	return 1;
#endif
}
