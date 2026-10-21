#include "Editor/Commandlets/WuwaTerrainImportCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/FileManager.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeLayerInfoObject.h"
#include "Materials/MaterialInterface.h"
#include "LandscapeProxy.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaTerrainImport, Log, All);

UWuwaTerrainImportCommandlet::UWuwaTerrainImportCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpDescription = TEXT("Builds a level with a Landscape from ArtSource/Terrain/<region>.terrain.json (original terrain, same spacing/scale/world position). Read-only unless -Apply.");
	HelpUsage = TEXT("-run=WuwaTerrainImport -Region=crownless [-Apply] -AllowCommandletRendering");
}

#if WITH_EDITOR
namespace WuwaTerrainImport
{
	FVector ReadVector(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		const TArray<TSharedPtr<FJsonValue>>& Values = Object->GetArrayField(Field);
		return FVector(Values[0]->AsNumber(), Values[1]->AsNumber(), Values.Num() > 2 ? Values[2]->AsNumber() : 0.0);
	}

	template <typename T>
	T* Spawn(UWorld* World, const FString& Label, const FVector& Location = FVector::ZeroVector, const FRotator& Rotation = FRotator::ZeroRotator)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		T* Actor = World->SpawnActor<T>(Location, Rotation, Params);
		Actor->SetActorLabel(Label);
		return Actor;
	}

	/** 和编辑器"新建关卡 → 开放世界基础光照"类似的一套可移动光照：太阳要标记为大气太阳光，天光实时捕获天空。 */
	void SpawnOutdoorLighting(UWorld* World)
	{
		ADirectionalLight* Sun = Spawn<ADirectionalLight>(World, TEXT("Sun"), FVector(0, 0, 50000), FRotator(-40.f, 35.f, 0.f));
		UDirectionalLightComponent* SunLight = CastChecked<UDirectionalLightComponent>(Sun->GetLightComponent());
		SunLight->SetMobility(EComponentMobility::Movable);
		SunLight->SetAtmosphereSunLight(true);
		SunLight->SetIntensity(10.f);
		Spawn<ASkyAtmosphere>(World, TEXT("SkyAtmosphere"));
		ASkyLight* Sky = Spawn<ASkyLight>(World, TEXT("SkyLight"), FVector(0, 0, 50000));
		Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		Sky->GetLightComponent()->bRealTimeCapture = true;
		Spawn<AExponentialHeightFog>(World, TEXT("HeightFog"));
		Spawn<AVolumetricCloud>(World, TEXT("VolumetricCloud"));
	}
}
#endif

int32 UWuwaTerrainImportCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaTerrainImport;
	FString Region;
	if (!FParse::Value(*Params, TEXT("Region="), Region) || Region.IsEmpty())
	{
		UE_LOG(LogWuwaTerrainImport, Error, TEXT("Usage: %s"), *HelpUsage);
		return 1;
	}
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));

	const FString RecipePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("ArtSource/Terrain") / (Region + TEXT(".terrain.json")));
	FString RecipeText;
	TSharedPtr<FJsonObject> Recipe;
	if (!FFileHelper::LoadFileToString(RecipeText, *RecipePath)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(RecipeText), Recipe) || !Recipe.IsValid())
	{
		UE_LOG(LogWuwaTerrainImport, Error, TEXT("Cannot read %s (run ArtSource/Terrain/prepare_terrain.py --region %s first)."), *RecipePath, *Region);
		return 1;
	}
	const FString LevelPackage = Recipe->GetStringField(TEXT("level"));
	const FString HeightmapPath = Recipe->GetStringField(TEXT("heightmap"));
	const int32 Size = Recipe->GetIntegerField(TEXT("size"));
	const int32 ComponentQuads = Recipe->GetIntegerField(TEXT("componentQuads"));
	const int32 Subsections = Recipe->GetIntegerField(TEXT("subsections"));
	const FVector Location = ReadVector(Recipe, TEXT("location"));
	const FVector Scale = ReadVector(Recipe, TEXT("scale"));
	const FVector Boss = ReadVector(Recipe, TEXT("boss"));
	if (Size < 2 || ComponentQuads <= 0 || Subsections <= 0 || (Size - 1) % ComponentQuads != 0 || ComponentQuads % Subsections != 0)
	{
		UE_LOG(LogWuwaTerrainImport, Error, TEXT("Invalid landscape layout: size %d, component quads %d, subsections %d."), Size, ComponentQuads, Subsections);
		return 1;
	}

	// 材质：<区域>.material.json（WuwaTerrainMaterialSetup 已生成资产）；没有就用引擎默认材质
	TSharedPtr<FJsonObject> MaterialRecipe;
	UMaterialInterface* DefaultMaterial = nullptr;
	TMap<FIntPoint, UMaterialInterface*> ComponentMaterials;
	{
		FString MaterialText;
		const FString MaterialPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("ArtSource/Terrain") / (Region + TEXT(".material.json")));
		if (FFileHelper::LoadFileToString(MaterialText, *MaterialPath))
		{
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(MaterialText), MaterialRecipe);
		}
		if (MaterialRecipe.IsValid())
		{
			const FString InstanceFolder = MaterialRecipe->GetStringField(TEXT("instanceFolder"));
			const TSharedPtr<FJsonObject> Aliases = MaterialRecipe->GetObjectField(TEXT("aliases"));
			auto Resolve = [&](const FString& OriginalPath) -> UMaterialInterface*
			{
				FString Name;
				if (!Aliases->TryGetStringField(FPaths::GetBaseFilename(OriginalPath), Name)) return nullptr;
				return LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *InstanceFolder, *Name, *Name));
			};
			// 默认材质：组件数最多的那个（materialCounts 已按数量排序）
			for (const auto& Pair : Recipe->GetObjectField(TEXT("materialCounts"))->Values)
			{
				DefaultMaterial = Resolve(Pair.Key);
				if (DefaultMaterial) break;
			}
			const TArray<TSharedPtr<FJsonValue>>& Rows = Recipe->GetArrayField(TEXT("componentMaterials"));
			for (int32 CY = 0; CY < Rows.Num(); ++CY)
			{
				const TArray<TSharedPtr<FJsonValue>>& Row = Rows[CY]->AsArray();
				for (int32 CX = 0; CX < Row.Num(); ++CX)
				{
					if (Row[CX]->Type == EJson::String) ComponentMaterials.Add(FIntPoint(CX, CY), Resolve(Row[CX]->AsString()));
				}
			}
			if (!DefaultMaterial)
			{
				UE_LOG(LogWuwaTerrainImport, Error, TEXT("Terrain material instances are missing (run -run=WuwaTerrainMaterialSetup -Region=%s -Apply first)."), *Region);
				return 1;
			}
		}
	}

	TArray<uint8> RawBytes;
	if (!FFileHelper::LoadFileToArray(RawBytes, *HeightmapPath) || RawBytes.Num() != Size * Size * 2)
	{
		UE_LOG(LogWuwaTerrainImport, Error, TEXT("%s must hold %d x %d 16-bit heights (%d bytes)."), *HeightmapPath, Size, Size, Size * Size * 2);
		return 1;
	}
	TArray<uint16> Heights;
	Heights.SetNumUninitialized(Size * Size);
	FMemory::Memcpy(Heights.GetData(), RawBytes.GetData(), RawBytes.Num());
	const TArray<uint16> Expected = Heights;

	// Boss 点所在顶点的高度：PlayerStart 放在它旁边一点、地面以上
	const int32 BossX = FMath::Clamp(FMath::RoundToInt((Boss.X - Location.X) / Scale.X), 0, Size - 1);
	const int32 BossY = FMath::Clamp(FMath::RoundToInt((Boss.Y - Location.Y) / Scale.Y), 0, Size - 1);
	const double BossGround = Location.Z + (double(Heights[BossY * Size + BossX]) - 32768.0) / 128.0 * Scale.Z;

	UE_LOG(LogWuwaTerrainImport, Display, TEXT("%s: landscape %dx%d vertices (%d components of %d quads), %.0f m across, at %s scale %s; boss ground %.1f m."),
		*LevelPackage, Size, Size, (Size - 1) / ComponentQuads, ComponentQuads, (Size - 1) * Scale.X / 100.0, *Location.ToString(), *Scale.ToString(), BossGround / 100.0);
	if (!bApply)
	{
		UE_LOG(LogWuwaTerrainImport, Display, TEXT("Read-only; pass -Apply to write."));
		return 0;
	}
	if (!FApp::CanEverRender())
	{
		UE_LOG(LogWuwaTerrainImport, Error, TEXT("Landscape edit layers need rendering: run with -AllowCommandletRendering and without -NullRHI."));
		return 1;
	}

	const FString Filename = FPackageName::LongPackageNameToFilename(LevelPackage, FPackageName::GetMapPackageExtension());
	if (IFileManager::Get().FileExists(*Filename))
	{
		const FString BackupDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/TerrainImport")
			/ FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
		if (IFileManager::Get().Copy(*(BackupDir / FPaths::GetCleanFilename(Filename)), *Filename) != COPY_OK)
		{
			UE_LOG(LogWuwaTerrainImport, Error, TEXT("Failed to back up %s"), *Filename);
			return 1;
		}
		UE_LOG(LogWuwaTerrainImport, Display, TEXT("Backed up to %s"), *BackupDir);
	}

	// 和编辑器"新建空白关卡"一样建世界（编辑器世界上下文、渲染场景、子系统都齐全），整关重建后另存到目标路径
	UWorld* World = UEditorLoadingAndSavingUtils::NewBlankMap(false);
	if (!World)
	{
		UE_LOG(LogWuwaTerrainImport, Error, TEXT("Cannot create a blank map."));
		return 1;
	}

	ALandscape* Landscape = Spawn<ALandscape>(World, TEXT("Landscape_HuangLong"), Location);
	Landscape->SetActorRelativeScale3D(Scale);
	Landscape->LandscapeMaterial = DefaultMaterial;
	TMap<FGuid, TArray<uint16>> HeightData;
	HeightData.Add(FGuid(), MoveTemp(Heights));
	// 材质层：配方里的每个层（DataLayer 是原作的挖洞层，网格导入前不用）对应 <layerInfoFolder>/<层>_LayerInfo
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> LayerData;
	TArray<FLandscapeImportLayerInfo>& ImportLayers = LayerData.Add(FGuid());
	const TSharedPtr<FJsonObject>* LayerFiles = nullptr;
	if (MaterialRecipe.IsValid() && Recipe->TryGetObjectField(TEXT("layers"), LayerFiles))
	{
		const FString LayerInfoFolder = MaterialRecipe->GetStringField(TEXT("layerInfoFolder"));
		for (const auto& Pair : (*LayerFiles)->Values)
		{
			if (Pair.Key == TEXT("DataLayer")) continue;
			ULandscapeLayerInfoObject* Info = LoadObject<ULandscapeLayerInfoObject>(nullptr,
				*FString::Printf(TEXT("%s/%s_LayerInfo.%s_LayerInfo"), *LayerInfoFolder, *Pair.Key, *Pair.Key));
			TArray<uint8> Weights;
			if (!Info || !FFileHelper::LoadFileToArray(Weights, *Pair.Value->AsString()) || Weights.Num() != Size * Size)
			{
				UE_LOG(LogWuwaTerrainImport, Error, TEXT("Layer %s: missing layer info or %s is not %d x %d bytes."), *Pair.Key, *Pair.Value->AsString(), Size, Size);
				return 1;
			}
			FLandscapeImportLayerInfo& Layer = ImportLayers.AddDefaulted_GetRef();
			Layer.LayerName = *Pair.Key;
			Layer.LayerInfo = Info;
			Layer.LayerData = MoveTemp(Weights);
		}
	}
	Landscape->Import(FGuid::NewGuid(), 0, 0, Size - 1, Size - 1, Subsections, ComponentQuads / Subsections, HeightData,
		*HeightmapPath, LayerData, ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());
	// 和编辑器"导入地形"一样，导入后为每个层登记目标层，编辑器里才能看到/继续绘制这些层
	for (const FLandscapeImportLayerInfo& Layer : LayerData[FGuid()])
	{
		Landscape->AddTargetLayer(Layer.LayerName, FLandscapeTargetLayerSettings(Layer.LayerInfo));
	}
	// 每个组件用原作对应组件的材质实例（不同于默认材质时才覆盖）
	int32 Overrides = 0;
	for (ULandscapeComponent* Component : Landscape->LandscapeComponents)
	{
		const int32 CX = Component->SectionBaseX / ComponentQuads, CY = Component->SectionBaseY / ComponentQuads;
		UMaterialInterface* const* Wanted = ComponentMaterials.Find(FIntPoint(CX, CY));
		if (Wanted && *Wanted && *Wanted != DefaultMaterial)
		{
			Component->OverrideMaterial = *Wanted;
			++Overrides;
		}
	}
	Landscape->UpdateAllComponentMaterialInstances();
	Landscape->ForceUpdateLayersContent();
	FlushRenderingCommands();
	UE_LOG(LogWuwaTerrainImport, Display, TEXT("Material %s, %d layers, %d component overrides."), *GetNameSafe(DefaultMaterial), ImportLayers.Num(), Overrides);

	// 抽查：在若干顶点上读回 Landscape 的高度（碰撞高度场），与原作高度值换算出的世界高度比较。
	// 采样点避开组件接缝和地形最外圈（落在边界上时碰撞查询可能取到相邻组件或查不到）
	int32 Checked = 0, Mismatched = 0;
	for (int32 Y = 17; Y < Size - 1; Y += 124)
	{
		for (int32 X = 17; X < Size - 1; X += 124)
		{
			const FVector At(Location.X + X * Scale.X, Location.Y + Y * Scale.Y, 0.0);
			const double Want = Location.Z + (double(Expected[Y * Size + X]) - 32768.0) / 128.0 * Scale.Z;
			const TOptional<float> Got = Landscape->GetHeightAtLocation(At);
			++Checked;
			if (!Got.IsSet() || FMath::Abs(Got.GetValue() - Want) > 5.0)
			{
				++Mismatched;
				UE_LOG(LogWuwaTerrainImport, Error, TEXT("Vertex (%d, %d): landscape height %s, original %.1f cm."), X, Y,
					Got.IsSet() ? *FString::SanitizeFloat(Got.GetValue()) : TEXT("none"), Want);
			}
		}
	}
	UE_LOG(LogWuwaTerrainImport, Display, TEXT("Height check: %d/%d sampled vertices match the original within 5 cm."), Checked - Mismatched, Checked);
	if (Mismatched > 0) return 1;

	SpawnOutdoorLighting(World);
	Spawn<APlayerStart>(World, TEXT("PlayerStart_Boss"), FVector(Boss.X + 1500.0, Boss.Y, BossGround + 200.0), FRotator(0.f, 180.f, 0.f));

	if (!UEditorLoadingAndSavingUtils::SaveMap(World, LevelPackage))
	{
		UE_LOG(LogWuwaTerrainImport, Error, TEXT("Failed to save %s (is the editor open?)"), *Filename);
		return 1;
	}
	UE_LOG(LogWuwaTerrainImport, Display, TEXT("Saved %s"), *LevelPackage);
	return 0;
#else
	return 1;
#endif
}
