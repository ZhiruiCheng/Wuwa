#include "Editor/Commandlets/WuwaRoleUiSetupCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Game/Module/Hud/WuwaHudTypes.h"
#include "Game/Module/Hud/EnergyBar/WuwaSpecialEnergyBarBase.h"
#include "Game/NewWorld/Character/Role/DataAsset/WuwaRoleDataAsset.h"
#include "GameplayTagsManager.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PrefabSystem/LGUIPrefab.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaRoleUiSetup, Log, All);

UWuwaRoleUiSetupCommandlet::UWuwaRoleUiSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpUsage = TEXT("-run=WuwaRoleUiSetup -Recipe=\"<abs role_ui.recipe.json>\" [-Apply]");
}

#if WITH_EDITOR
namespace WuwaRoleUiSetup
{
	struct FContext
	{
		bool bApply = false;
		FString BackupDir;
		FString TextureFolder;
		TMap<FString, UTexture2D*> Textures;   // PNG 相对路径 → 贴图
		int32 Errors = 0;
	};

	template <typename T>
	T* LoadAsset(const FString& PackageName)
	{
		return LoadObject<T>(nullptr, *(PackageName + TEXT(".") + FPackageName::GetShortName(PackageName)), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	template <typename T>
	TSoftObjectPtr<T> SoftRef(const FString& PackageName)
	{
		return TSoftObjectPtr<T>(FSoftObjectPath(PackageName + TEXT(".") + FPackageName::GetShortName(PackageName)));
	}

	bool SaveAsset(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		if (!UPackage::SavePackage(Package, Asset, *Filename, Args))
		{
			UE_LOG(LogWuwaRoleUiSetup, Error, TEXT("Failed to save %s"), *Filename);
			return false;
		}
		return true;
	}

	bool Backup(FContext& Ctx, const FString& PackageName)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		if (!IFileManager::Get().FileExists(*Filename)) return true;
		if (Ctx.BackupDir.IsEmpty())
		{
			Ctx.BackupDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/RoleUiSetup") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
		}
		if (IFileManager::Get().Copy(*(Ctx.BackupDir / FPaths::GetCleanFilename(Filename)), *Filename) != COPY_OK)
		{
			UE_LOG(LogWuwaRoleUiSetup, Error, TEXT("Failed to back up %s"), *Filename);
			return false;
		}
		return true;
	}

	/**
	 * PNG → 贴图（UI 用：不压缩，sRGB）；同名的直接覆盖源数据。返回软引用用的包名。
	 * 和 HUD 预制体里的贴图不同，这些图（头像 152、技能图标 128……）在界面上都是缩小显示的，不生成 mip 会有明显锯齿，所以要 mip。
	 */
	FString ImportTexture(FContext& Ctx, const FString& RelativeFile)
	{
		const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RelativeFile);
		const FString PackageName = Ctx.TextureFolder / FPaths::GetBaseFilename(File);
		if (Ctx.Textures.Contains(RelativeFile)) return PackageName;
		IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> Png = Images.CreateImageWrapper(EImageFormat::PNG);
		TArray<uint8> Compressed, Pixels;
		if (!FFileHelper::LoadFileToArray(Compressed, *File) || !Png->SetCompressed(Compressed.GetData(), Compressed.Num())
			|| !Png->GetRaw(ERGBFormat::BGRA, 8, Pixels))
		{
			UE_LOG(LogWuwaRoleUiSetup, Error, TEXT("Cannot read %s"), *File);
			++Ctx.Errors;
			return PackageName;
		}
		UTexture2D* Texture = LoadAsset<UTexture2D>(PackageName);
		Ctx.Textures.Add(RelativeFile, Texture);
		if (!Ctx.bApply) return PackageName;
		if (!Texture)
		{
			UPackage* Package = CreatePackage(*PackageName);
			Texture = NewObject<UTexture2D>(Package, *FPaths::GetBaseFilename(File), RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Texture);
		}
		Texture->PreEditChange(nullptr);
		Texture->Source.Init(Png->GetWidth(), Png->GetHeight(), 1, 1, TSF_BGRA8, Pixels.GetData());
		Texture->CompressionSettings = TC_EditorIcon;
		Texture->MipGenSettings = TMGS_SimpleAverage;
		Texture->LODGroup = TEXTUREGROUP_UI;
		Texture->SRGB = true;
		Texture->NeverStream = true;
		if (!Texture->AssetImportData) Texture->AssetImportData = NewObject<UAssetImportData>(Texture);
		Texture->AssetImportData->Update(File);
		Texture->PostEditChange();
		if (!SaveAsset(Texture)) ++Ctx.Errors;
		Ctx.Textures.Add(RelativeFile, Texture);
		return PackageName;
	}

	FColor HexColor(const FString& Hex)
	{
		// 原作的颜色是 RRGGBB 或 RRGGBBAA
		return FColor::FromHex(Hex);
	}

	FGameplayTag RequestTag(FContext& Ctx, const FString& Name)
	{
		const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(*Name, false);
		if (!Tag.IsValid())
		{
			UE_LOG(LogWuwaRoleUiSetup, Error, TEXT("Gameplay tag %s is not registered (Config/DefaultGameplayTags.ini)."), *Name);
			++Ctx.Errors;
		}
		return Tag;
	}

	/** 整表重写：没有就新建（行结构 RowStruct）。 */
	UDataTable* PrepareTable(FContext& Ctx, const FString& PackageName, UScriptStruct* RowStruct)
	{
		UDataTable* Table = LoadAsset<UDataTable>(PackageName);
		if (Table && Table->GetRowStruct() != RowStruct)
		{
			UE_LOG(LogWuwaRoleUiSetup, Error, TEXT("%s exists with another row struct."), *PackageName);
			++Ctx.Errors;
			return nullptr;
		}
		UE_LOG(LogWuwaRoleUiSetup, Display, TEXT("Table %s: will %s."), *PackageName, Table ? TEXT("rewrite") : TEXT("create"));
		if (!Ctx.bApply) return nullptr;
		if (Table)
		{
			if (!Backup(Ctx, PackageName)) return nullptr;
			Table->EmptyTable();
			return Table;
		}
		UPackage* Package = CreatePackage(*PackageName);
		Table = NewObject<UDataTable>(Package, *FPackageName::GetShortName(PackageName), RF_Public | RF_Standalone | RF_Transactional);
		Table->RowStruct = RowStruct;
		FAssetRegistryModule::AssetCreated(Table);
		return Table;
	}

	bool SetupElements(FContext& Ctx, const FString& TablePackage, const TArray<TSharedPtr<FJsonValue>>& Elements)
	{
		TMap<FName, FWuwaElementUiInfo> Rows;
		for (const TSharedPtr<FJsonValue>& Value : Elements)
		{
			const TSharedPtr<FJsonObject> Element = Value->AsObject();
			const FString Tag = Element->GetStringField(TEXT("tag"));
			RequestTag(Ctx, Tag);
			FWuwaElementUiInfo Row;
			Row.Icon = SoftRef<UTexture2D>(ImportTexture(Ctx, Element->GetStringField(TEXT("icon"))));
			Row.Color = HexColor(Element->GetStringField(TEXT("color")));
			Rows.Add(*Tag, Row);
		}
		UDataTable* Table = PrepareTable(Ctx, TablePackage, FWuwaElementUiInfo::StaticStruct());
		if (!Table) return !Ctx.bApply && !Ctx.Errors;
		for (const TPair<FName, FWuwaElementUiInfo>& Pair : Rows) Table->AddRow(Pair.Key, Pair.Value);
		return SaveAsset(Table);
	}

	bool SetupEnergyBars(FContext& Ctx, const FString& TablePackage, const TArray<TSharedPtr<FJsonValue>>& Bars)
	{
		TMap<FName, FWuwaSpecialEnergyBarInfo> Rows;
		const UEnum* TypeEnum = StaticEnum<EWuwaSpecialEnergyBarType>();
		for (const TSharedPtr<FJsonValue>& Value : Bars)
		{
			const TSharedPtr<FJsonObject> Bar = Value->AsObject();
			const FString Id = FString::FromInt(static_cast<int32>(Bar->GetNumberField(TEXT("id"))));
			const FString Prefab = Bar->GetStringField(TEXT("prefab"));
			if (!LoadAsset<ULGUIPrefab>(Prefab))
			{
				UE_LOG(LogWuwaRoleUiSetup, Warning, TEXT("Energy bar %s: prefab %s not built yet (-run=WuwaKuroHudBuild)."), *Id, *Prefab);
			}
			FWuwaSpecialEnergyBarInfo Row;
			const int64 Type = TypeEnum->GetValueByNameString(Bar->GetStringField(TEXT("prefabType")));
			Row.Type = Type == INDEX_NONE ? EWuwaSpecialEnergyBarType::Custom : static_cast<EWuwaSpecialEnergyBarType>(Type);
			Row.Prefab = SoftRef<ULGUIPrefab>(Prefab);
			Row.SlotNum = static_cast<int32>(Bar->GetNumberField(TEXT("slotNum")));
			Row.SpecialEnergyIndex = static_cast<int32>(Bar->GetNumberField(TEXT("specialEnergyIndex")));
			Row.DisableKeyOnPercent = static_cast<float>(Bar->GetNumberField(TEXT("disableKeyOnPercent")));
			const FString ClassPath = Bar->GetStringField(TEXT("barClass"));
			if (!ClassPath.IsEmpty())
			{
				Row.BarClass = LoadClass<UWuwaSpecialEnergyBarBase>(nullptr, *ClassPath);
				if (!Row.BarClass)
				{
					UE_LOG(LogWuwaRoleUiSetup, Error, TEXT("Energy bar %s: class %s not found."), *Id, *ClassPath);
					++Ctx.Errors;
				}
			}
			FString File;
			if (Bar->TryGetStringField(TEXT("texture"), File)) Row.Texture = SoftRef<UTexture2D>(ImportTexture(Ctx, File));
			if (Bar->TryGetStringField(TEXT("enableTexture"), File)) Row.EnableTexture = SoftRef<UTexture2D>(ImportTexture(Ctx, File));
			Row.EffectColor = HexColor(Bar->GetStringField(TEXT("effectColor")));
			Row.PointColor = HexColor(Bar->GetStringField(TEXT("pointColor")));
			for (const TSharedPtr<FJsonValue>& Param : Bar->GetArrayField(TEXT("extraFloatParams"))) Row.ExtraFloatParams.Add(Param->AsNumber());
			Rows.Add(*Id, Row);
		}
		UDataTable* Table = PrepareTable(Ctx, TablePackage, FWuwaSpecialEnergyBarInfo::StaticStruct());
		if (!Table) return !Ctx.bApply && !Ctx.Errors;
		for (const TPair<FName, FWuwaSpecialEnergyBarInfo>& Pair : Rows) Table->AddRow(Pair.Key, Pair.Value);
		return SaveAsset(Table);
	}

	bool SetupRole(FContext& Ctx, const TSharedPtr<FJsonObject>& Recipe)
	{
		const FString AssetPackage = Recipe->GetStringField(TEXT("asset"));
		UWuwaRoleDataAsset* Role = LoadAsset<UWuwaRoleDataAsset>(AssetPackage);
		if (!Role)
		{
			UE_LOG(LogWuwaRoleUiSetup, Error, TEXT("Missing role asset %s"), *AssetPackage);
			++Ctx.Errors;
			return false;
		}
		const FString HeadIcon = ImportTexture(Ctx, Recipe->GetStringField(TEXT("headIcon")));
		const FGameplayTag Element = RequestTag(Ctx, Recipe->GetStringField(TEXT("elementTag")));
		TArray<FWuwaRoleSkillButton> Buttons;
		const UEnum* TypeEnum = StaticEnum<EWuwaSkillButtonType>();
		for (const TSharedPtr<FJsonValue>& Value : Recipe->GetArrayField(TEXT("skillButtons")))
		{
			const TSharedPtr<FJsonObject> Button = Value->AsObject();
			const int64 Type = TypeEnum->GetValueByNameString(Button->GetStringField(TEXT("type")));
			if (Type == INDEX_NONE)
			{
				UE_LOG(LogWuwaRoleUiSetup, Error, TEXT("%s: unknown button type %s"), *AssetPackage, *Button->GetStringField(TEXT("type")));
				++Ctx.Errors;
				continue;
			}
			FWuwaRoleSkillButton& Entry = Buttons.AddDefaulted_GetRef();
			Entry.ButtonType = static_cast<EWuwaSkillButtonType>(Type);
			Entry.SkillId = static_cast<int32>(Button->GetNumberField(TEXT("skillId")));
			Entry.Icon = SoftRef<UTexture2D>(ImportTexture(Ctx, Button->GetStringField(TEXT("icon"))));
		}
		const int32 EnergyBarId = static_cast<int32>(Recipe->GetNumberField(TEXT("specialEnergyBarId")));
		UE_LOG(LogWuwaRoleUiSetup, Display, TEXT("%s: head %s, element %s, %d skill buttons, energy bar %d (was head %s, element %s)."),
			*AssetPackage, *FPackageName::GetShortName(HeadIcon), *Element.ToString(), Buttons.Num(), EnergyBarId,
			*Role->HeadIcon.ToString(), *Role->ElementTag.ToString());
		if (!Ctx.bApply) return true;
		if (!Backup(Ctx, AssetPackage)) return false;
		Role->Modify();
		Role->HeadIcon = SoftRef<UTexture2D>(HeadIcon);
		Role->ElementTag = Element;
		Role->SkillButtons = MoveTemp(Buttons);
		Role->SpecialEnergyBarId = EnergyBarId;
		return SaveAsset(Role);
	}
}
#endif

int32 UWuwaRoleUiSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaRoleUiSetup;
	FString RecipePath;
	if (!FParse::Value(*Params, TEXT("Recipe="), RecipePath, false))
	{
		UE_LOG(LogWuwaRoleUiSetup, Error, TEXT("Usage: %s"), *HelpUsage);
		return 1;
	}
	FString Text;
	TSharedPtr<FJsonObject> Recipe;
	const FString RecipeFile = FPaths::IsRelative(RecipePath) ? FPaths::ProjectDir() / RecipePath : RecipePath;
	if (!FFileHelper::LoadFileToString(Text, *RecipeFile) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Recipe) || !Recipe)
	{
		UE_LOG(LogWuwaRoleUiSetup, Error, TEXT("Cannot read recipe %s"), *RecipeFile);
		return 1;
	}
	FContext Ctx;
	Ctx.bApply = FParse::Param(*Params, TEXT("Apply"));
	Ctx.TextureFolder = Recipe->GetStringField(TEXT("textureDir"));

	bool bOk = SetupElements(Ctx, Recipe->GetStringField(TEXT("elementTable")), Recipe->GetArrayField(TEXT("elements")));
	bOk &= SetupEnergyBars(Ctx, Recipe->GetStringField(TEXT("energyBarTable")), Recipe->GetArrayField(TEXT("energyBars")));
	for (const TSharedPtr<FJsonValue>& Role : Recipe->GetArrayField(TEXT("roles")))
	{
		bOk &= SetupRole(Ctx, Role->AsObject());
	}
	UE_LOG(LogWuwaRoleUiSetup, Display, TEXT("%s: %d textures -> %s."), Ctx.bApply ? TEXT("Applied") : TEXT("Dry run; pass -Apply to write"),
		Ctx.Textures.Num(), *Ctx.TextureFolder);
	if (!Ctx.BackupDir.IsEmpty()) UE_LOG(LogWuwaRoleUiSetup, Display, TEXT("Backups in %s"), *Ctx.BackupDir);
	return bOk && !Ctx.Errors ? 0 : 1;
#else
	return 1;
#endif
}
