#include "Editor/Commandlets/WuwaTerrainMaterialSetupCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "LandscapeLayerInfoObject.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaTerrainMaterial, Log, All);

UWuwaTerrainMaterialSetupCommandlet::UWuwaTerrainMaterialSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpDescription = TEXT("Builds the landscape textures, M_WuwaLandscape, per-original-instance material instances and layer infos from ArtSource/Terrain/<region>.material.json. Read-only unless -Apply.");
	HelpUsage = TEXT("-run=WuwaTerrainMaterialSetup -Region=crownless [-Apply]");
}

#if WITH_EDITOR
namespace WuwaTerrainMaterial
{
	struct FContext
	{
		bool bApply = false;
		int32 Errors = 0;
		FString BackupDir;
		TArray<UObject*> ToSave;
	};

	template <typename T>
	T* Find(const FString& Path)
	{
		return LoadObject<T>(nullptr, *(Path + TEXT(".") + FPackageName::GetShortName(Path)), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	void Backup(FContext& Ctx, const FString& Path)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension());
		if (!IFileManager::Get().FileExists(*Filename)) return;
		if (Ctx.BackupDir.IsEmpty())
		{
			Ctx.BackupDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/TerrainMaterialSetup")
				/ FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
		}
		if (IFileManager::Get().Copy(*(Ctx.BackupDir / FPaths::GetCleanFilename(Filename)), *Filename) != COPY_OK)
		{
			UE_LOG(LogWuwaTerrainMaterial, Error, TEXT("Failed to back up %s"), *Filename);
			++Ctx.Errors;
		}
	}

	/** 已有就备份后复用（保持引用不断），没有就新建。 */
	template <typename T>
	T* Obtain(FContext& Ctx, const FString& Path)
	{
		if (T* Existing = Find<T>(Path))
		{
			Backup(Ctx, Path);
			Ctx.ToSave.AddUnique(Existing);
			return Existing;
		}
		UPackage* Package = CreatePackage(*Path);
		T* Created = NewObject<T>(Package, *FPackageName::GetShortName(Path), RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Created);
		Ctx.ToSave.AddUnique(Created);
		return Created;
	}

	UTexture2D* ImportTexture(FContext& Ctx, const FString& Path, const FString& File, bool bNormal, bool bClamp = false)
	{
		TArray64<uint8> Compressed, Pixels;
		IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> Png = Images.CreateImageWrapper(EImageFormat::PNG);
		if (!FFileHelper::LoadFileToArray(Compressed, *File) || !Png->SetCompressed(Compressed.GetData(), Compressed.Num())
			|| !Png->GetRaw(ERGBFormat::BGRA, 8, Pixels))
		{
			UE_LOG(LogWuwaTerrainMaterial, Error, TEXT("Cannot read %s"), *File);
			++Ctx.Errors;
			return nullptr;
		}
		UTexture2D* Texture = Obtain<UTexture2D>(Ctx, Path);
		Texture->PreEditChange(nullptr);
		Texture->Source.Init(Png->GetWidth(), Png->GetHeight(), 1, 1, TSF_BGRA8, Pixels.GetData());
		// 法线：原作 RG 是切线空间 XY（B/A 存了别的数据），BC5 只用 RG、Z 在采样时重建
		Texture->SRGB = !bNormal;
		Texture->CompressionSettings = bNormal ? TC_Normalmap : TC_Default;
		Texture->LODGroup = TEXTUREGROUP_World;
		Texture->AddressX = Texture->AddressY = bClamp ? TA_Clamp : TA_Wrap;
		if (!Texture->AssetImportData) Texture->AssetImportData = NewObject<UAssetImportData>(Texture);
		Texture->AssetImportData->Update(File);
		Texture->PostEditChange();
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

	FString LayerName(int32 N) { return FString::Printf(TEXT("Layer%d"), N); }

	/** 主材质：每层按世界坐标 XY / 平铺尺寸 采样颜色和法线，颜色乘色调；三路 LandscapeLayerBlend 按图层权重混合。 */
	UMaterial* BuildMaster(FContext& Ctx, const FString& Path, int32 Layers, UTexture2D* DefaultColor, UTexture2D* DefaultNormal,
		UTexture2D* ColorMap, const FLinearColor& ColorMapUVScaleBias, float ColorMapIntensity)
	{
		UMaterial* M = Obtain<UMaterial>(Ctx, Path);
		M->PreEditChange(nullptr);
		M->GetExpressionCollection().Empty();
		M->MaterialDomain = MD_Surface;
		M->BlendMode = BLEND_Opaque;
		M->SetShadingModel(MSM_DefaultLit);

		auto* World = Add<UMaterialExpressionWorldPosition>(M, -2200, 0);
		auto* WorldXY = Add<UMaterialExpressionComponentMask>(M, -2000, 0);
		WorldXY->R = WorldXY->G = true;
		WorldXY->B = WorldXY->A = false;
		WorldXY->Input.Connect(0, World);

		// 陡坡侧面投影（通用地形技法，原作的做法看不到）：平地用世界 XY 平铺，坡越陡越多地改用侧面投影
		// （按法线在 YZ / XZ 两个平面里选），竖直崖面上的贴图就不会被拉成竖条
		auto* NormalWS = Add<UMaterialExpressionVertexNormalWS>(M, -2200, 300);
		auto* SideUV = Add<UMaterialExpressionCustom>(M, -2000, 200);
		SideUV->Description = TEXT("SideUV");
		SideUV->OutputType = CMOT_Float2;
		SideUV->Code = TEXT("return abs(N.x) > abs(N.y) ? P.yz : P.xz;");
		SideUV->Inputs.Reset();
		{
			FCustomInput& InP = SideUV->Inputs.AddDefaulted_GetRef();
			InP.InputName = TEXT("P");
			InP.Input.Connect(0, World);
			FCustomInput& InN = SideUV->Inputs.AddDefaulted_GetRef();
			InN.InputName = TEXT("N");
			InN.Input.Connect(0, NormalWS);
		}
		auto* SlopeStart = Add<UMaterialExpressionScalarParameter>(M, -2200, 420);
		SlopeStart->ParameterName = TEXT("SideProjectionStart");
		SlopeStart->DefaultValue = 0.75f;   // 法线 Z 小于这个（坡度约 41° 以上）开始混入侧面投影
		SlopeStart->Group = TEXT("Side Projection");
		auto* SlopeEnd = Add<UMaterialExpressionScalarParameter>(M, -2200, 480);
		SlopeEnd->ParameterName = TEXT("SideProjectionEnd");
		SlopeEnd->DefaultValue = 0.5f;      // 法线 Z 小于这个（约 60° 以上）完全用侧面投影
		SlopeEnd->Group = TEXT("Side Projection");
		auto* SideWeight = Add<UMaterialExpressionCustom>(M, -2000, 400);
		SideWeight->Description = TEXT("SideWeight");
		SideWeight->OutputType = CMOT_Float1;
		SideWeight->Code = TEXT("return saturate((Start - abs(N.z)) / max(Start - End, 1e-3));");
		SideWeight->Inputs.Reset();
		{
			FCustomInput& InN = SideWeight->Inputs.AddDefaulted_GetRef();
			InN.InputName = TEXT("N");
			InN.Input.Connect(0, NormalWS);
			FCustomInput& InS = SideWeight->Inputs.AddDefaulted_GetRef();
			InS.InputName = TEXT("Start");
			InS.Input.Connect(0, SlopeStart);
			FCustomInput& InE = SideWeight->Inputs.AddDefaulted_GetRef();
			InE.InputName = TEXT("End");
			InE.Input.Connect(0, SlopeEnd);
		}

		auto* ColorBlend = Add<UMaterialExpressionLandscapeLayerBlend>(M, -300, -200);
		auto* NormalBlend = Add<UMaterialExpressionLandscapeLayerBlend>(M, -300, 200);
		auto* RoughBlend = Add<UMaterialExpressionLandscapeLayerBlend>(M, -300, 600);
		for (int32 N = 1; N <= Layers; ++N)
		{
			const int32 Y = (N - 1) * 420;
			const FString Prefix = FString::Printf(TEXT("L%d_"), N);
			const FName Group(*FString::Printf(TEXT("Layer %02d"), N));

			auto* Scale = Add<UMaterialExpressionScalarParameter>(M, -2000, Y + 120);
			Scale->ParameterName = *(Prefix + TEXT("Scale"));
			Scale->DefaultValue = 500.f;
			Scale->Group = Group;
			auto* UV = Add<UMaterialExpressionDivide>(M, -1800, Y);
			UV->A.Connect(0, WorldXY);
			UV->B.Connect(0, Scale);

			auto* SideLayerUV = Add<UMaterialExpressionDivide>(M, -1800, Y + 60);
			SideLayerUV->A.Connect(0, SideUV);
			SideLayerUV->B.Connect(0, Scale);

			auto* TopColor = Add<UMaterialExpressionTextureSampleParameter2D>(M, -1500, Y);
			TopColor->ParameterName = *(Prefix + TEXT("Color"));
			TopColor->Texture = DefaultColor;
			TopColor->SamplerType = SAMPLERTYPE_Color;
			TopColor->SamplerSource = SSM_Wrap_WorldGroupSettings;
			TopColor->Coordinates.Connect(0, UV);
			TopColor->Group = Group;
			// 同一个贴图参数名：侧面采样跟随材质实例里的同一张贴图
			auto* SideColor = Add<UMaterialExpressionTextureSampleParameter2D>(M, -1500, Y + 60);
			SideColor->ParameterName = *(Prefix + TEXT("Color"));
			SideColor->Texture = DefaultColor;
			SideColor->SamplerType = SAMPLERTYPE_Color;
			SideColor->SamplerSource = SSM_Wrap_WorldGroupSettings;
			SideColor->Coordinates.Connect(0, SideLayerUV);
			SideColor->Group = Group;
			auto* Color = Add<UMaterialExpressionLinearInterpolate>(M, -1300, Y);
			Color->A.Connect(0, TopColor);
			Color->B.Connect(0, SideColor);
			Color->Alpha.Connect(0, SideWeight);
			auto* Tint = Add<UMaterialExpressionVectorParameter>(M, -1500, Y + 260);
			Tint->ParameterName = *(Prefix + TEXT("Tint"));
			Tint->DefaultValue = FLinearColor::White;
			Tint->Group = Group;
			auto* Tinted = Add<UMaterialExpressionMultiply>(M, -1100, Y);
			Tinted->A.Connect(0, Color);
			Tinted->B.Connect(0, Tint);

			auto* TopNormal = Add<UMaterialExpressionTextureSampleParameter2D>(M, -1500, Y + 130);
			TopNormal->ParameterName = *(Prefix + TEXT("Normal"));
			TopNormal->Texture = DefaultNormal;
			TopNormal->SamplerType = SAMPLERTYPE_Normal;
			TopNormal->SamplerSource = SSM_Wrap_WorldGroupSettings;
			TopNormal->Coordinates.Connect(0, UV);
			TopNormal->Group = Group;
			auto* SideNormal = Add<UMaterialExpressionTextureSampleParameter2D>(M, -1500, Y + 190);
			SideNormal->ParameterName = *(Prefix + TEXT("Normal"));
			SideNormal->Texture = DefaultNormal;
			SideNormal->SamplerType = SAMPLERTYPE_Normal;
			SideNormal->SamplerSource = SSM_Wrap_WorldGroupSettings;
			SideNormal->Coordinates.Connect(0, SideLayerUV);
			SideNormal->Group = Group;
			auto* Normal = Add<UMaterialExpressionLinearInterpolate>(M, -1300, Y + 130);
			Normal->A.Connect(0, TopNormal);
			Normal->B.Connect(0, SideNormal);
			Normal->Alpha.Connect(0, SideWeight);

			auto* Rough = Add<UMaterialExpressionScalarParameter>(M, -1100, Y + 200);
			Rough->ParameterName = *(Prefix + TEXT("Roughness"));
			Rough->DefaultValue = 0.8f;
			Rough->Group = Group;

			const FName Layer(*LayerName(N));
			auto AddInput = [Layer](UMaterialExpressionLandscapeLayerBlend* Blend, UMaterialExpression* Source)
			{
				FLayerBlendInput& In = Blend->Layers.AddDefaulted_GetRef();
				In.LayerName = Layer;
				In.BlendType = LB_WeightBlend;
				In.PreviewWeight = 0.f;
				In.LayerInput.Connect(0, Source);
			};
			AddInput(ColorBlend, Tinted);
			AddInput(NormalBlend, Normal);
			AddInput(RoughBlend, Rough);
		}
		// 世界色彩图（原作 WorldLandscapeColorMap）：UV = (世界 XY + 偏移) / 尺寸；底色 = lerp(图层色, 图层色 × 色彩图 × 2, 强度)
		auto* MapST = Add<UMaterialExpressionVectorParameter>(M, -900, -900);
		MapST->ParameterName = TEXT("ColorMapUVScaleBias");
		MapST->DefaultValue = ColorMapUVScaleBias;
		MapST->Group = TEXT("Color Map");
		auto* MapScale = Add<UMaterialExpressionComponentMask>(M, -700, -950);
		MapScale->R = MapScale->G = true;
		MapScale->B = MapScale->A = false;
		MapScale->Input.Connect(0, MapST);
		// 向量参数的输出 0 只有 RGB；偏移取单独的 B（输出 3）、A（输出 4）两路拼成二维
		auto* MapBias = Add<UMaterialExpressionAppendVector>(M, -700, -850);
		MapBias->A.Connect(3, MapST);
		MapBias->B.Connect(4, MapST);
		auto* Shifted = Add<UMaterialExpressionAdd>(M, -500, -900);
		Shifted->A.Connect(0, WorldXY);
		Shifted->B.Connect(0, MapBias);
		auto* MapUV = Add<UMaterialExpressionDivide>(M, -350, -900);
		MapUV->A.Connect(0, Shifted);
		MapUV->B.Connect(0, MapScale);
		auto* Map = Add<UMaterialExpressionTextureSampleParameter2D>(M, -200, -900);
		Map->ParameterName = TEXT("ColorMap");
		Map->Texture = ColorMap ? ColorMap : DefaultColor;
		Map->SamplerType = SAMPLERTYPE_Color;
		Map->SamplerSource = SSM_Clamp_WorldGroupSettings;
		Map->Coordinates.Connect(0, MapUV);
		Map->Group = TEXT("Color Map");
		auto* Two = Add<UMaterialExpressionConstant>(M, -200, -700);
		Two->R = 2.f;
		auto* MapX2 = Add<UMaterialExpressionMultiply>(M, 0, -850);
		MapX2->A.Connect(0, Map);
		MapX2->B.Connect(0, Two);
		auto* Tinted = Add<UMaterialExpressionMultiply>(M, 150, -500);
		Tinted->A.Connect(0, ColorBlend);
		Tinted->B.Connect(0, MapX2);
		auto* Intensity = Add<UMaterialExpressionScalarParameter>(M, 150, -650);
		Intensity->ParameterName = TEXT("ColorMapIntensity");
		Intensity->DefaultValue = ColorMapIntensity;
		Intensity->Group = TEXT("Color Map");
		auto* FinalColor = Add<UMaterialExpressionLinearInterpolate>(M, 350, -400);
		FinalColor->A.Connect(0, ColorBlend);
		FinalColor->B.Connect(0, Tinted);
		FinalColor->Alpha.Connect(0, Intensity);
		M->GetEditorOnlyData()->BaseColor.Connect(0, FinalColor);
		M->GetEditorOnlyData()->Normal.Connect(0, NormalBlend);
		M->GetEditorOnlyData()->Roughness.Connect(0, RoughBlend);
		M->PostEditChange();
		return M;
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
		if (!UPackage::SavePackage(Package, Asset, *Filename, Args))
		{
			UE_LOG(LogWuwaTerrainMaterial, Error, TEXT("Failed to save %s (is the editor open?)"), *Filename);
			++Ctx.Errors;
			return false;
		}
		return true;
	}
}
#endif

int32 UWuwaTerrainMaterialSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaTerrainMaterial;
	FString Region;
	if (!FParse::Value(*Params, TEXT("Region="), Region) || Region.IsEmpty())
	{
		UE_LOG(LogWuwaTerrainMaterial, Error, TEXT("Usage: %s"), *HelpUsage);
		return 1;
	}
	FContext Ctx;
	Ctx.bApply = FParse::Param(*Params, TEXT("Apply"));

	const FString RecipePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("ArtSource/Terrain") / (Region + TEXT(".material.json")));
	FString Text;
	TSharedPtr<FJsonObject> Recipe;
	if (!FFileHelper::LoadFileToString(Text, *RecipePath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Recipe) || !Recipe.IsValid())
	{
		UE_LOG(LogWuwaTerrainMaterial, Error, TEXT("Cannot read %s (run ArtSource/Terrain/prepare_terrain_material.py --region %s first)."), *RecipePath, *Region);
		return 1;
	}
	const FString MasterPath = Recipe->GetStringField(TEXT("master"));
	const FString TextureFolder = Recipe->GetStringField(TEXT("textureFolder"));
	const FString InstanceFolder = Recipe->GetStringField(TEXT("instanceFolder"));
	const FString LayerInfoFolder = Recipe->GetStringField(TEXT("layerInfoFolder"));
	const int32 Layers = Recipe->GetIntegerField(TEXT("layers"));
	const TSharedPtr<FJsonObject> Textures = Recipe->GetObjectField(TEXT("textures"));
	const TSharedPtr<FJsonObject> Instances = Recipe->GetObjectField(TEXT("instances"));

	UE_LOG(LogWuwaTerrainMaterial, Display, TEXT("%s: %d textures -> %s, master %s (%d layers), %d instances -> %s, layer infos -> %s"),
		*Region, Textures->Values.Num(), *TextureFolder, *MasterPath, Layers, Instances->Values.Num(), *InstanceFolder, *LayerInfoFolder);
	if (!Ctx.bApply)
	{
		UE_LOG(LogWuwaTerrainMaterial, Display, TEXT("Read-only; pass -Apply to write."));
		return 0;
	}

	TMap<FString, UTexture2D*> Imported;
	for (const auto& Pair : Textures->Values)
	{
		const TSharedPtr<FJsonObject> Entry = Pair.Value->AsObject();
		bool bClamp = false;
		Entry->TryGetBoolField(TEXT("clamp"), bClamp);
		if (UTexture2D* Texture = ImportTexture(Ctx, TextureFolder / Pair.Key, Entry->GetStringField(TEXT("file")), Entry->GetBoolField(TEXT("normal")), bClamp))
		{
			Imported.Add(Pair.Key, Texture);
		}
	}
	if (Ctx.Errors > 0) return 1;

	UTexture2D* DefaultColor = nullptr;
	UTexture2D* DefaultNormal = nullptr;
	for (const auto& Pair : Imported)
	{
		if (!DefaultColor && Pair.Key.StartsWith(TEXT("T_Landscape_")) && Pair.Key.EndsWith(TEXT("_D"))) DefaultColor = Pair.Value;
		if (!DefaultNormal && Pair.Key.EndsWith(TEXT("_N"))) DefaultNormal = Pair.Value;
	}
	FString ColorMapName;
	Recipe->TryGetStringField(TEXT("colorMap"), ColorMapName);
	UTexture2D* const* ColorMap = Imported.Find(ColorMapName);
	FLinearColor MapST(819200.f, 819200.f, 409600.f, 409600.f);
	const TArray<TSharedPtr<FJsonValue>>* STValues = nullptr;
	if (Recipe->TryGetArrayField(TEXT("colorMapUVScaleBias"), STValues) && STValues->Num() == 4)
	{
		MapST = FLinearColor((*STValues)[0]->AsNumber(), (*STValues)[1]->AsNumber(), (*STValues)[2]->AsNumber(), (*STValues)[3]->AsNumber());
	}
	double MapIntensity = 1.0;
	Recipe->TryGetNumberField(TEXT("colorMapIntensity"), MapIntensity);
	UMaterial* Master = BuildMaster(Ctx, MasterPath, Layers, DefaultColor, DefaultNormal, ColorMap ? *ColorMap : nullptr, MapST, MapIntensity);

	for (const auto& Pair : Instances->Values)
	{
		UMaterialInstanceConstant* Instance = Obtain<UMaterialInstanceConstant>(Ctx, InstanceFolder / Pair.Key);
		Instance->PreEditChange(nullptr);
		Instance->SetParentEditorOnly(Master);
		Instance->ClearParameterValuesEditorOnly();
		const TSharedPtr<FJsonObject> LayerParams = Pair.Value->AsObject()->GetObjectField(TEXT("layers"));
		for (const auto& Layer : LayerParams->Values)
		{
			const TSharedPtr<FJsonObject> P = Layer.Value->AsObject();
			const FString Prefix = FString::Printf(TEXT("L%s_"), *Layer.Key);
			const int32 Slice = P->GetIntegerField(TEXT("slice"));
			UTexture2D* const* Color = Imported.Find(FString::Printf(TEXT("T_Landscape_%02d_D"), Slice));
			UTexture2D* const* Normal = Imported.Find(FString::Printf(TEXT("T_Landscape_%02d_N"), Slice));
			if (!Color || !Normal)
			{
				UE_LOG(LogWuwaTerrainMaterial, Error, TEXT("%s layer %s: slice %d was not imported."), *Pair.Key, *Layer.Key, Slice);
				++Ctx.Errors;
				continue;
			}
			const TArray<TSharedPtr<FJsonValue>>& Tint = P->GetArrayField(TEXT("tint"));
			Instance->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(*(Prefix + TEXT("Color"))), *Color);
			Instance->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(*(Prefix + TEXT("Normal"))), *Normal);
			Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(*(Prefix + TEXT("Scale"))), P->GetNumberField(TEXT("scale")));
			Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(*(Prefix + TEXT("Roughness"))), P->GetNumberField(TEXT("roughness")));
			Instance->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(*(Prefix + TEXT("Tint"))),
				FLinearColor(Tint[0]->AsNumber(), Tint[1]->AsNumber(), Tint[2]->AsNumber()));
		}
		Instance->PostEditChange();
	}

	for (int32 N = 1; N <= Layers; ++N)
	{
		ULandscapeLayerInfoObject* Info = Obtain<ULandscapeLayerInfoObject>(Ctx, LayerInfoFolder / (LayerName(N) + TEXT("_LayerInfo")));
		Info->SetLayerName(*LayerName(N), false);
	}

	for (UObject* Asset : Ctx.ToSave) Save(Ctx, Asset);
	if (!Ctx.BackupDir.IsEmpty()) UE_LOG(LogWuwaTerrainMaterial, Display, TEXT("Backed up to %s"), *Ctx.BackupDir);
	UE_LOG(LogWuwaTerrainMaterial, Display, TEXT("Saved %d assets, %d errors."), Ctx.ToSave.Num(), Ctx.Errors);
	return Ctx.Errors == 0 ? 0 : 1;
#else
	return 1;
#endif
}
