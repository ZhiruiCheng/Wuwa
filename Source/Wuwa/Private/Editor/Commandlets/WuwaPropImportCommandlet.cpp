#include "Editor/Commandlets/WuwaPropImportCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "InterchangeGenericAssetsPipeline.h"
#include "InterchangeGenericAssetsPipelineSharedSettings.h"
#include "InterchangeGenericMaterialPipeline.h"
#include "InterchangeGenericMeshPipeline.h"
#include "InterchangeGenericTexturePipeline.h"
#include "InterchangeManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaPropImport, Log, All);

UWuwaPropImportCommandlet::UWuwaPropImportCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpDescription = TEXT("Imports the original props of a region (textures, materials, meshes) and places them as HISM instances in the region level. Read-only unless -Apply.");
	HelpUsage = TEXT("-run=WuwaPropImport -Region=crownless [-Limit=N] [-Reimport | -UpdateMaterials] [-Apply]");
}

#if WITH_EDITOR
namespace WuwaPropImport
{
	const TCHAR* CommonFolder = TEXT("/Game/Environment/Common/Props");
	const TCHAR* TempFolder = TEXT("/Game/__WuwaPropImport");
	const FName PropsTag(TEXT("WuwaProps"));
	// 原作岩石 / 植被主材质用的世界色彩图（WorldColorMap），UV 规则同地形：(世界 XY + 409600) / 819200
	const TCHAR* ColorMapAsset = TEXT("/Game/Environment/Common/Landscape/Textures/T_LandscapeColorMap_D");
	const TCHAR* ColorMapPng = TEXT("C:/GamePakExtractor/Output/Exports/Client/Content/Aki/Scene/Landscape/ColorMap/T_LandscapeColorMap_D.png");

	struct FContext
	{
		bool bReimport = false;
		int32 Errors = 0;
		TArray<UObject*> ToSave;
	};

	template <typename T>
	T* Find(const FString& Path)
	{
		return LoadObject<T>(nullptr, *(Path + TEXT(".") + FPackageName::GetShortName(Path)), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	template <typename T>
	T* Obtain(FContext& Ctx, const FString& Path, bool& bCreated)
	{
		bCreated = false;
		if (T* Existing = Find<T>(Path)) return Existing;
		UPackage* Package = CreatePackage(*Path);
		T* Created = NewObject<T>(Package, *FPackageName::GetShortName(Path), RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Created);
		bCreated = true;
		return Created;
	}

	bool Save(FContext& Ctx, UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		Args.Error = GWarn;
		if (UPackage::SavePackage(Package, Asset, *Filename, Args)) return true;
		UE_LOG(LogWuwaPropImport, Error, TEXT("Failed to save %s (is the editor open?)"), *Filename);
		++Ctx.Errors;
		return false;
	}

	UTexture2D* ImportTexture(FContext& Ctx, const FString& Path, const FString& File, bool bNormal, bool bClamp = false)
	{
		bool bCreated = false;
		UTexture2D* Texture = Obtain<UTexture2D>(Ctx, Path, bCreated);
		if (!bCreated && !Ctx.bReimport) return Texture;
		TArray64<uint8> Compressed, Pixels;
		IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> Png = Images.CreateImageWrapper(EImageFormat::PNG);
		if (!FFileHelper::LoadFileToArray(Compressed, *File) || !Png->SetCompressed(Compressed.GetData(), Compressed.Num())
			|| !Png->GetRaw(ERGBFormat::BGRA, 8, Pixels))
		{
			UE_LOG(LogWuwaPropImport, Error, TEXT("Cannot read %s"), *File);
			++Ctx.Errors;
			return nullptr;
		}
		Texture->PreEditChange(nullptr);
		Texture->Source.Init(Png->GetWidth(), Png->GetHeight(), 1, 1, TSF_BGRA8, Pixels.GetData());
		Texture->SRGB = !bNormal;
		Texture->CompressionSettings = bNormal ? TC_Normalmap : TC_Default;
		Texture->LODGroup = TEXTUREGROUP_World;
		Texture->AddressX = Texture->AddressY = bClamp ? TA_Clamp : TA_Wrap;
		if (!Texture->AssetImportData) Texture->AssetImportData = NewObject<UAssetImportData>(Texture);
		Texture->AssetImportData->Update(File);
		Texture->PostEditChange();
		Ctx.ToSave.AddUnique(Texture);
		return Texture;
	}

	template <typename T>
	T* Add(UMaterial* M, int32 X, int32 Y)
	{
		T* E = NewObject<T>(M, NAME_None, RF_Transactional);
		E->Material = M;
		E->MaterialExpressionEditorX = X;
		E->MaterialExpressionEditorY = Y;
		M->GetExpressionCollection().AddExpression(E);
		return E;
	}

	/** 通用主材质：BaseColor = 漫反射 × Tint，法线贴图，粗糙度参数；遮罩版用漫反射 alpha 裁剪、双面。 */
	UMaterial* BuildMaster(FContext& Ctx, const FString& Path, bool bMasked, UTexture2D* ColorMap)
	{
		bool bCreated = false;
		UMaterial* M = Obtain<UMaterial>(Ctx, Path, bCreated);
		M->PreEditChange(nullptr);
		M->GetExpressionCollection().Empty();
		M->MaterialDomain = MD_Surface;
		M->BlendMode = bMasked ? BLEND_Masked : BLEND_Opaque;
		M->TwoSided = bMasked;
		M->OpacityMaskClipValue = 0.3333f;
		M->SetShadingModel(MSM_DefaultLit);
		M->bUsedWithInstancedStaticMeshes = true;
		M->bUsedWithNanite = !bMasked;

		auto* Base = Add<UMaterialExpressionTextureSampleParameter2D>(M, -800, -200);
		Base->ParameterName = TEXT("BaseColor");
		Base->Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
		Base->SamplerType = SAMPLERTYPE_Color;
		auto* Tint = Add<UMaterialExpressionVectorParameter>(M, -800, 50);
		Tint->ParameterName = TEXT("Tint");
		Tint->DefaultValue = FLinearColor::White;
		auto* Color = Add<UMaterialExpressionMultiply>(M, -400, -150);
		Color->A.Connect(0, Base);
		Color->B.Connect(0, Tint);

		// 世界色彩图着色：底色 = lerp(颜色, 颜色 × 色彩图 × 2, ColorMapIntensity)（混合公式同地形，为推测）
		auto* World = Add<UMaterialExpressionWorldPosition>(M, -1400, -600);
		auto* XY = Add<UMaterialExpressionComponentMask>(M, -1250, -600);
		XY->R = XY->G = true;
		XY->B = XY->A = false;
		XY->Input.Connect(0, World);
		auto* MapST = Add<UMaterialExpressionVectorParameter>(M, -1400, -450);
		MapST->ParameterName = TEXT("ColorMapUVScaleBias");
		MapST->DefaultValue = FLinearColor(819200.f, 819200.f, 409600.f, 409600.f);
		auto* Scale = Add<UMaterialExpressionComponentMask>(M, -1200, -500);
		Scale->R = Scale->G = true;
		Scale->B = Scale->A = false;
		Scale->Input.Connect(0, MapST);
		auto* Bias = Add<UMaterialExpressionAppendVector>(M, -1200, -400);
		Bias->A.Connect(3, MapST);
		Bias->B.Connect(4, MapST);
		auto* Shifted = Add<UMaterialExpressionAdd>(M, -1050, -550);
		Shifted->A.Connect(0, XY);
		Shifted->B.Connect(0, Bias);
		auto* MapUV = Add<UMaterialExpressionDivide>(M, -900, -550);
		MapUV->A.Connect(0, Shifted);
		MapUV->B.Connect(0, Scale);
		auto* Map = Add<UMaterialExpressionTextureSampleParameter2D>(M, -750, -550);
		Map->ParameterName = TEXT("ColorMap");
		Map->Texture = ColorMap ? ColorMap : Base->Texture.Get();
		Map->SamplerType = SAMPLERTYPE_Color;
		Map->SamplerSource = SSM_Clamp_WorldGroupSettings;
		Map->Coordinates.Connect(0, MapUV);
		auto* Two = Add<UMaterialExpressionConstant>(M, -750, -380);
		Two->R = 2.f;
		auto* MapX2 = Add<UMaterialExpressionMultiply>(M, -550, -500);
		MapX2->A.Connect(0, Map);
		MapX2->B.Connect(0, Two);
		auto* Mapped = Add<UMaterialExpressionMultiply>(M, -350, -400);
		Mapped->A.Connect(0, Color);
		Mapped->B.Connect(0, MapX2);
		auto* Intensity = Add<UMaterialExpressionScalarParameter>(M, -350, -300);
		Intensity->ParameterName = TEXT("ColorMapIntensity");
		Intensity->DefaultValue = 0.f;
		auto* Final = Add<UMaterialExpressionLinearInterpolate>(M, -150, -250);
		Final->A.Connect(0, Color);
		Final->B.Connect(0, Mapped);
		Final->Alpha.Connect(0, Intensity);
		M->GetEditorOnlyData()->BaseColor.Connect(0, Final);

		auto* Normal = Add<UMaterialExpressionTextureSampleParameter2D>(M, -800, 200);
		Normal->ParameterName = TEXT("Normal");
		Normal->Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineMaterials/DefaultNormal.DefaultNormal"));
		Normal->SamplerType = SAMPLERTYPE_Normal;
		M->GetEditorOnlyData()->Normal.Connect(0, Normal);

		auto* Rough = Add<UMaterialExpressionScalarParameter>(M, -400, 100);
		Rough->ParameterName = TEXT("Roughness");
		Rough->DefaultValue = 0.85f;
		M->GetEditorOnlyData()->Roughness.Connect(0, Rough);
		if (bMasked) M->GetEditorOnlyData()->OpacityMask.Connect(4, Base);  // 输出 4 = A
		M->PostEditChange();
		Ctx.ToSave.AddUnique(M);
		return M;
	}

	UInterchangeGenericAssetsPipeline* MakePipeline()
	{
		UInterchangeGenericAssetsPipeline* Pipeline = NewObject<UInterchangeGenericAssetsPipeline>(GetTransientPackage());
		Pipeline->bUseSourceNameForAsset = true;
		Pipeline->MeshPipeline->bImportStaticMeshes = true;
		Pipeline->MeshPipeline->bImportSkeletalMeshes = false;
		Pipeline->MeshPipeline->bCombineStaticMeshes = true;
		Pipeline->MeshPipeline->bCollision = false;
		Pipeline->MeshPipeline->bBuildNanite = false;
		Pipeline->MeshPipeline->bGenerateLightmapUVs = false;
		Pipeline->CommonMeshesProperties->bRecomputeNormals = false;
		Pipeline->CommonMeshesProperties->bRecomputeTangents = false;
		Pipeline->CommonMeshesProperties->bImportLods = false;
		Pipeline->MaterialPipeline->bImportMaterials = false;
		Pipeline->MaterialPipeline->TexturePipeline->bImportTextures = false;
		return Pipeline;
	}

	/** glb → 临时目录 → 目标路径（同名已有资产时先删掉，Interchange 直接导进有资产的目录会静默失败）。 */
	UStaticMesh* ImportMesh(FContext& Ctx, UInterchangeGenericAssetsPipeline* Pipeline, const FString& Glb, const FString& Target, int32 Index)
	{
		const FString Temp = FString::Printf(TEXT("%s/%d"), TempFolder, Index);
		FImportAssetParameters Params;
		Params.bIsAutomated = true;
		Params.bReplaceExisting = true;
		Params.OverridePipelines.Add(FSoftObjectPath(Pipeline));
		TArray<UObject*> Imported;
		if (!UInterchangeManager::GetInterchangeManager().ImportAsset(Temp, UInterchangeManager::CreateSourceData(Glb), Params, Imported))
		{
			UE_LOG(LogWuwaPropImport, Error, TEXT("Interchange failed on %s"), *Glb);
			++Ctx.Errors;
			return nullptr;
		}
		UStaticMesh* Mesh = nullptr;
		for (UObject* Object : Imported) if (UStaticMesh* M = Cast<UStaticMesh>(Object)) { Mesh = M; break; }
		if (!Mesh)
		{
			UE_LOG(LogWuwaPropImport, Error, TEXT("%s produced no static mesh (%d objects)."), *Glb, Imported.Num());
			++Ctx.Errors;
			return nullptr;
		}
		IAssetTools& Tools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		if (UStaticMesh* Old = Find<UStaticMesh>(Target))
		{
			// 重新导入：把新网格的几何数据复制进旧资产（保持引用），而不是删掉重建。
			// 不能直接搬 SourceModel：它的批量数据对象属于临时包，保存时会报跨包引用
			const FMeshDescription* Source = Mesh->GetMeshDescription(0);
			if (!Source)
			{
				UE_LOG(LogWuwaPropImport, Error, TEXT("%s has no mesh description to copy."), *Mesh->GetPathName());
				++Ctx.Errors;
				return nullptr;
			}
			Old->PreEditChange(nullptr);
			Old->CreateMeshDescription(0, *Source);
			Old->CommitMeshDescription(0);
			Old->SetStaticMaterials(Mesh->GetStaticMaterials());
			Old->PostEditChange();
			return Old;
		}
		TArray<FAssetRenameData> Rename;
		Rename.Emplace(Mesh, FPackageName::GetLongPackagePath(Target), FPackageName::GetShortName(Target));
		if (!Tools.RenameAssets(Rename))
		{
			UE_LOG(LogWuwaPropImport, Error, TEXT("Cannot move %s to %s"), *Mesh->GetPathName(), *Target);
			++Ctx.Errors;
			return nullptr;
		}
		return Mesh;
	}

	FString Category(const FString& Mesh)
	{
		if (Mesh.Contains(TEXT("/Foliage/"))) return TEXT("Foliage");
		if (Mesh.Contains(TEXT("_Roc_")) || Mesh.Contains(TEXT("_Cli_")) || Mesh.Contains(TEXT("_Mou_")) || Mesh.Contains(TEXT("_Ter_"))) return TEXT("Rocks");
		return TEXT("Structures");
	}
}
#endif

int32 UWuwaPropImportCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaPropImport;
	FString Region;
	if (!FParse::Value(*Params, TEXT("Region="), Region) || Region.IsEmpty())
	{
		UE_LOG(LogWuwaPropImport, Error, TEXT("Usage: %s"), *HelpUsage);
		return 1;
	}
	FContext Ctx;
	Ctx.bReimport = FParse::Param(*Params, TEXT("Reimport"));
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	// -UpdateMaterials：只按配方刷新材质实例的参数（贴图、色彩图开关、颜色），不导网格、不动关卡
	const bool bMaterialsOnly = FParse::Param(*Params, TEXT("UpdateMaterials"));
	int32 Limit = 0;
	FParse::Value(*Params, TEXT("Limit="), Limit);

	const FString RecipePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("ArtSource/Terrain") / (Region + TEXT(".props.json")));
	FString Text;
	TSharedPtr<FJsonObject> Recipe;
	if (!FFileHelper::LoadFileToString(Text, *RecipePath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Recipe) || !Recipe.IsValid())
	{
		UE_LOG(LogWuwaPropImport, Error, TEXT("Cannot read %s (run ArtSource/Terrain/prepare_props.py --region %s first)."), *RecipePath, *Region);
		return 1;
	}
	const FString Level = Recipe->GetStringField(TEXT("level"));
	const TSharedPtr<FJsonObject> Meshes = Recipe->GetObjectField(TEXT("meshes"));
	const TSharedPtr<FJsonObject> Materials = Recipe->GetObjectField(TEXT("materials"));
	const TSharedPtr<FJsonObject> Textures = Recipe->GetObjectField(TEXT("textures"));
	const TArray<TSharedPtr<FJsonValue>>& Placements = Recipe->GetArrayField(TEXT("placements"));

	// 配方里网格已按摆放数量排序；-Limit 只取前 N 种
	TArray<FString> MeshKeys;
	Meshes->Values.GetKeys(MeshKeys);
	MeshKeys.Sort([&Meshes](const FString& A, const FString& B)
		{ return Meshes->GetObjectField(A)->GetIntegerField(TEXT("count")) > Meshes->GetObjectField(B)->GetIntegerField(TEXT("count")); });
	if (Limit > 0 && MeshKeys.Num() > Limit) MeshKeys.SetNum(Limit);
	const TSet<FString> Selected(MeshKeys);
	int32 Instances = 0;
	for (const TSharedPtr<FJsonValue>& P : Placements) Instances += Selected.Contains(P->AsArray()[0]->AsString()) ? 1 : 0;
	UE_LOG(LogWuwaPropImport, Display, TEXT("%s: %d of %d meshes, %d placements -> %s"), *Region, MeshKeys.Num(), Meshes->Values.Num(), Instances, *Level);
	if (!bApply)
	{
		UE_LOG(LogWuwaPropImport, Display, TEXT("Read-only; pass -Apply to write."));
		return 0;
	}

	// ---- 贴图、材质 ----
	UTexture2D* ColorMap = ImportTexture(Ctx, ColorMapAsset, ColorMapPng, false, true);
	UMaterial* Opaque = BuildMaster(Ctx, FString(CommonFolder) / TEXT("M_WuwaProp"), false, ColorMap);
	UMaterial* Masked = BuildMaster(Ctx, FString(CommonFolder) / TEXT("M_WuwaPropMasked"), true, ColorMap);
	TMap<FString, UMaterialInterface*> MaterialAssets;
	TSet<FString> NeededMaterials;
	for (const FString& Key : MeshKeys)
		for (const TSharedPtr<FJsonValue>& Slot : Meshes->GetObjectField(Key)->GetArrayField(TEXT("materials")))
			if (Slot->Type == EJson::String) NeededMaterials.Add(Slot->AsString());
	for (const FString& MatKey : NeededMaterials)
	{
		const TSharedPtr<FJsonObject>* Entry = nullptr;
		if (!Materials->TryGetObjectField(MatKey, Entry)) continue;
		auto TextureFor = [&](const TCHAR* Field) -> UTexture2D*
		{
			FString TexKey;
			if (!(*Entry)->TryGetStringField(Field, TexKey)) return nullptr;
			const TSharedPtr<FJsonObject>* Tex = nullptr;
			if (!Textures->TryGetObjectField(TexKey, Tex)) return nullptr;
			return ImportTexture(Ctx, (*Tex)->GetStringField(TEXT("asset")), (*Tex)->GetStringField(TEXT("png")), (*Tex)->GetBoolField(TEXT("normal")));
		};
		UTexture2D* Diffuse = TextureFor(TEXT("diffuse"));
		UTexture2D* Normal = TextureFor(TEXT("normal"));
		bool bCreated = false;
		UMaterialInstanceConstant* Instance = Obtain<UMaterialInstanceConstant>(Ctx, (*Entry)->GetStringField(TEXT("asset")), bCreated);
		if (bCreated || Ctx.bReimport || bMaterialsOnly)
		{
			Instance->PreEditChange(nullptr);
			Instance->SetParentEditorOnly((*Entry)->GetBoolField(TEXT("masked")) ? Masked : Opaque);
			Instance->ClearParameterValuesEditorOnly();
			if (Diffuse) Instance->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(TEXT("BaseColor")), Diffuse);
			if (Normal) Instance->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Normal")), Normal);
			bool bColorMap = false;
			(*Entry)->TryGetBoolField(TEXT("colorMap"), bColorMap);
			Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("ColorMapIntensity")), bColorMap ? 1.f : 0.f);
			const TArray<TSharedPtr<FJsonValue>>* Tint = nullptr;
			if ((*Entry)->TryGetArrayField(TEXT("tint"), Tint) && Tint->Num() == 3)
			{
				Instance->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Tint")),
					FLinearColor((*Tint)[0]->AsNumber(), (*Tint)[1]->AsNumber(), (*Tint)[2]->AsNumber()));
			}
			Instance->PostEditChange();
			Ctx.ToSave.AddUnique(Instance);
		}
		MaterialAssets.Add(MatKey, Instance);
	}

	if (bMaterialsOnly)
	{
		for (UObject* Asset : Ctx.ToSave) Save(Ctx, Asset);
		UE_LOG(LogWuwaPropImport, Display, TEXT("Updated materials only: saved %d assets, %d errors."), Ctx.ToSave.Num(), Ctx.Errors);
		return Ctx.Errors == 0 ? 0 : 1;
	}

	// ---- 网格 ----
	UInterchangeGenericAssetsPipeline* Pipeline = MakePipeline();
	Pipeline->AddToRoot();
	TMap<FString, UStaticMesh*> MeshAssets;
	int32 Index = 0, Imported = 0;
	for (const FString& Key : MeshKeys)
	{
		const TSharedPtr<FJsonObject> Entry = Meshes->GetObjectField(Key);
		const FString Target = Entry->GetStringField(TEXT("asset"));
		UStaticMesh* Mesh = Find<UStaticMesh>(Target);
		if (!Mesh || Ctx.bReimport)
		{
			Mesh = ImportMesh(Ctx, Pipeline, Entry->GetStringField(TEXT("glb")), Target, Index++);
			if (!Mesh) continue;
			++Imported;
			// 槽位按顺序对应配方里的原作材质
			const TArray<TSharedPtr<FJsonValue>>& Slots = Entry->GetArrayField(TEXT("materials"));
			TArray<FStaticMaterial> StaticMaterials = Mesh->GetStaticMaterials();
			for (int32 S = 0; S < StaticMaterials.Num() && S < Slots.Num(); ++S)
			{
				if (Slots[S]->Type != EJson::String) continue;
				if (UMaterialInterface* const* M = MaterialAssets.Find(Slots[S]->AsString())) StaticMaterials[S].MaterialInterface = *M;
			}
			Mesh->PreEditChange(nullptr);
			Mesh->SetStaticMaterials(StaticMaterials);
			const bool bMasked = Entry->GetBoolField(TEXT("masked"));
			FMeshNaniteSettings Nanite = Mesh->GetNaniteSettings();
			Nanite.bEnabled = !bMasked;
			Mesh->SetNaniteSettings(Nanite);
			if (Entry->GetBoolField(TEXT("collision")))
			{
				Mesh->CreateBodySetup();
				Mesh->GetBodySetup()->CollisionTraceFlag = CTF_UseComplexAsSimple;
			}
			Mesh->PostEditChange();
			Ctx.ToSave.AddUnique(Mesh);
			if (Imported % 25 == 0) UE_LOG(LogWuwaPropImport, Display, TEXT("Imported %d meshes ..."), Imported);
		}
		MeshAssets.Add(Key, Mesh);
	}
	Pipeline->RemoveFromRoot();
	for (UObject* Asset : Ctx.ToSave) Save(Ctx, Asset);
	UE_LOG(LogWuwaPropImport, Display, TEXT("Imported %d meshes, saved %d assets."), Imported, Ctx.ToSave.Num());

	// ---- 摆放 ----
	const FString LevelFile = FPackageName::LongPackageNameToFilename(Level, FPackageName::GetMapPackageExtension());
	const FString BackupDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/PropImport") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	IFileManager::Get().Copy(*(BackupDir / FPaths::GetCleanFilename(LevelFile)), *LevelFile);
	UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(Level);
	if (!World)
	{
		UE_LOG(LogWuwaPropImport, Error, TEXT("Cannot load %s"), *Level);
		return 1;
	}
	TArray<AActor*> Old;
	for (TActorIterator<AActor> It(World); It; ++It) if (It->Tags.Contains(PropsTag)) Old.Add(*It);
	for (AActor* Actor : Old) World->DestroyActor(Actor);

	TMap<FString, TMap<UStaticMesh*, TArray<FTransform>>> ByCategory;
	for (const TSharedPtr<FJsonValue>& P : Placements)
	{
		const TArray<TSharedPtr<FJsonValue>>& A = P->AsArray();
		UStaticMesh* const* Mesh = MeshAssets.Find(A[0]->AsString());
		if (!Mesh || !*Mesh) continue;
		const FTransform T(FQuat(A[4]->AsNumber(), A[5]->AsNumber(), A[6]->AsNumber(), A[7]->AsNumber()),
			FVector(A[1]->AsNumber(), A[2]->AsNumber(), A[3]->AsNumber()), FVector(A[8]->AsNumber(), A[9]->AsNumber(), A[10]->AsNumber()));
		ByCategory.FindOrAdd(Category(A[0]->AsString())).FindOrAdd(*Mesh).Add(T);
	}
	int32 Placed = 0;
	for (const auto& Pair : ByCategory)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = MakeUniqueObjectName(World->PersistentLevel, AActor::StaticClass(), *FString::Printf(TEXT("Props_%s"), *Pair.Key));
		AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
		Actor->SetActorLabel(FString::Printf(TEXT("Props_%s"), *Pair.Key));
		Actor->Tags.Add(PropsTag);
		USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"));
		Actor->SetRootComponent(Root);
		Actor->AddInstanceComponent(Root);
		Root->RegisterComponent();
		for (const auto& MeshPair : Pair.Value)
		{
			UStaticMesh* Mesh = MeshPair.Key;
			auto* HISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(Actor, *FString::Printf(TEXT("HISM_%s"), *Mesh->GetName()));
			HISM->SetStaticMesh(Mesh);
			HISM->SetupAttachment(Root);
			const bool bCollision = Mesh->GetBodySetup() && Mesh->GetBodySetup()->CollisionTraceFlag == CTF_UseComplexAsSimple;
			HISM->SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
			// 小植被远处剔除、不投影，省开销
			const float Size = Mesh->GetBounds().SphereRadius * 2.f;
			if (Pair.Key == TEXT("Foliage") && Size < 400.f)
			{
				HISM->SetCullDistances(6000, 9000);
				HISM->SetCastShadow(Size >= 150.f);
			}
			Actor->AddInstanceComponent(HISM);
			HISM->RegisterComponent();
			HISM->AddInstances(MeshPair.Value, false, true);
			Placed += MeshPair.Value.Num();
		}
	}
	if (!UEditorLoadingAndSavingUtils::SaveMap(World, Level))
	{
		UE_LOG(LogWuwaPropImport, Error, TEXT("Failed to save %s"), *Level);
		return 1;
	}
	UE_LOG(LogWuwaPropImport, Display, TEXT("Placed %d instances in %d categories (replaced %d old prop actors); level backup %s; %d errors."),
		Placed, ByCategory.Num(), Old.Num(), *BackupDir, Ctx.Errors);
	return Ctx.Errors == 0 ? 0 : 1;
#else
	return 1;
#endif
}
