#include "Editor/Commandlets/WuwaAssetDumpCommandlet.h"

#include "Engine/Blueprint.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaAssetDump, Log, All);

UWuwaAssetDumpCommandlet::UWuwaAssetDumpCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpUsage = TEXT("-run=WuwaAssetDump -Asset=/Game/... [-Property=Name]");
}

int32 UWuwaAssetDumpCommandlet::Main(const FString& Params)
{
	FString Path, PropertyName;
	if (!FParse::Value(*Params, TEXT("Asset="), Path))
	{
		UE_LOG(LogWuwaAssetDump, Error, TEXT("Usage: %s"), *HelpUsage);
		return 1;
	}
	FParse::Value(*Params, TEXT("Property="), PropertyName);
	if (!Path.Contains(TEXT("."))) Path += TEXT(".") + FPackageName::GetShortName(Path);
	UObject* Object = LoadObject<UObject>(nullptr, *Path);
	if (const UBlueprint* Blueprint = Cast<UBlueprint>(Object)) Object = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;
	if (!Object)
	{
		UE_LOG(LogWuwaAssetDump, Error, TEXT("%s not found."), *Path);
		return 1;
	}
	for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
	{
		if (!PropertyName.IsEmpty() && It->GetName() != PropertyName) continue;
		if (PropertyName.IsEmpty() && !It->HasAnyPropertyFlags(CPF_Edit)) continue;
		FString Text;
		It->ExportTextItem_InContainer(Text, Object, nullptr, Object, PPF_None);
		UE_LOG(LogWuwaAssetDump, Display, TEXT("%s = %s"), *It->GetName(), *Text);
	}
	return 0;
}
