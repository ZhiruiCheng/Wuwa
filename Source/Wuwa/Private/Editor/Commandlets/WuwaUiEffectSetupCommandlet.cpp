#include "Editor/Commandlets/WuwaUiEffectSetupCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Editor/Effects/WuwaReferenceAssetImport.h"
#include "Editor/Effects/WuwaUiNiagaraBuilder.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemFactoryNew.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaUiEffectSetup, Log, All);

UWuwaUiEffectSetupCommandlet::UWuwaUiEffectSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpUsage = TEXT("-run=WuwaUiEffectSetup -Recipe=\"<abs ui_effects.recipe.json>\" [-Apply]");
}

#if WITH_EDITOR
namespace WuwaUiEffectSetup
{
	struct FContext
	{
		bool bApply = false;
		FString BackupDir;
		int32 Errors = 0;
	};

	template <typename T>
	T* LoadAsset(const FString& PackageName)
	{
		return LoadObject<T>(nullptr, *(PackageName + TEXT(".") + FPackageName::GetShortName(PackageName)), nullptr, LOAD_NoWarn | LOAD_Quiet);
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
			UE_LOG(LogWuwaUiEffectSetup, Error, TEXT("Failed to save %s"), *Filename);
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
			Ctx.BackupDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/UiEffectSetup") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
		}
		if (IFileManager::Get().Copy(*(Ctx.BackupDir / FPaths::GetCleanFilename(Filename)), *Filename) != COPY_OK)
		{
			UE_LOG(LogWuwaUiEffectSetup, Error, TEXT("Failed to back up %s"), *Filename);
			return false;
		}
		return true;
	}

	template <class T>
	T* Expression(UMaterial* M)
	{
		T* E = NewObject<T>(M);
		M->GetExpressionCollection().AddExpression(E);
		return E;
	}

	void Input(UMaterialExpressionCustom* Code, const FName Name, UMaterialExpression* E, int32 Output = 0)
	{
		FCustomInput I;
		I.InputName = Name;
		I.Input.Connect(Output, E);
		Code->Inputs.Add(I);
	}

	/**
	 * 界面特效材质：Surface + Unlit（LGUI 画布只认这种或 UI 域）、双面；遮罩 = 贴图的某个通道（原作 MI 的 Base_AlphaSwitch），乘粒子顶点色。
	 * 叠加：自发光 = 顶点色 × 遮罩 × 顶点透明度；半透明：自发光 = 顶点色、不透明度 = 遮罩 × 顶点透明度。
	 * 同时标成 Niagara 网格 / 精灵可用，Niagara 系统编译时不会报材质用法的错（实际由 LGUI 画）。
	 */
	UMaterial* BuildMaterial(FContext& Ctx, const FString& Folder, const FJsonObject& Def, const TMap<FString, UTexture2D*>& Textures)
	{
		const FString PackageName = Folder / (TEXT("M_") + Def.GetStringField(TEXT("id")));
		UTexture2D* const* Texture = Textures.Find(Def.GetStringField(TEXT("texture")));
		if (!Texture || !*Texture)
		{
			UE_LOG(LogWuwaUiEffectSetup, Error, TEXT("%s: texture %s not imported"), *PackageName, *Def.GetStringField(TEXT("texture")));
			++Ctx.Errors;
			return nullptr;
		}
		UMaterial* M = LoadAsset<UMaterial>(PackageName);
		if (!M)
		{
			UPackage* Package = CreatePackage(*PackageName);
			M = NewObject<UMaterial>(Package, *FPackageName::GetShortName(PackageName), RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(M);
		}
		else if (!Backup(Ctx, PackageName))
		{
			return nullptr;
		}
		const bool bAdditive = Def.GetStringField(TEXT("blend")) == TEXT("additive");
		M->PreEditChange(nullptr);
		M->GetExpressionCollection().Empty();
		M->MaterialDomain = MD_Surface;
		M->BlendMode = bAdditive ? BLEND_Additive : BLEND_Translucent;
		M->SetShadingModel(MSM_Unlit);
		M->TwoSided = true;
		M->bUsedWithNiagaraSprites = true;
		M->bUsedWithNiagaraMeshParticles = true;
		auto* UV = Expression<UMaterialExpressionTextureCoordinate>(M);
		auto* Color = Expression<UMaterialExpressionVertexColor>(M);
		auto* Tex = Expression<UMaterialExpressionTextureObjectParameter>(M);
		Tex->ParameterName = TEXT("Base");
		Tex->Texture = *Texture;
		Tex->SamplerType = (*Texture)->SRGB ? SAMPLERTYPE_Color : SAMPLERTYPE_LinearColor;
		auto* Switch = Expression<UMaterialExpressionVectorParameter>(M);
		Switch->ParameterName = TEXT("Base_AlphaSwitch");
		const TArray<TSharedPtr<FJsonValue>>& Channel = Def.GetArrayField(TEXT("channel"));
		Switch->DefaultValue = FLinearColor(Channel[0]->AsNumber(), Channel[1]->AsNumber(), Channel[2]->AsNumber(), Channel[3]->AsNumber());
		auto* Code = Expression<UMaterialExpressionCustom>(M);
		Code->OutputType = CMOT_Float4;
		Code->Description = TEXT("UI effect: texture channel mask x particle vertex colour (rebuilt; the original master material graph is not in the cooked game)");
		Code->Inputs.Reset();
		Input(Code, TEXT("UV"), UV);
		Input(Code, TEXT("VC"), Color, 0);
		Input(Code, TEXT("VA"), Color, 4);
		Input(Code, TEXT("Tex"), Tex);
		Input(Code, TEXT("Switch"), Switch);
		// 叠加和半透明都是 自发光 = 顶点色、不透明度 = 遮罩 × 顶点 alpha（叠加时引擎 / LGUI 再把自发光乘上不透明度加到背景上）
		Code->Code = TEXT("float m = dot(Texture2DSample(Tex, TexSampler, UV), Switch);\nreturn float4(VC, m * VA);");
		auto* RGB = Expression<UMaterialExpressionComponentMask>(M);
		RGB->Input.Connect(0, Code);
		RGB->R = RGB->G = RGB->B = true;
		RGB->A = false;
		auto* Alpha = Expression<UMaterialExpressionComponentMask>(M);
		Alpha->Input.Connect(0, Code);
		Alpha->R = Alpha->G = Alpha->B = false;
		Alpha->A = true;
		M->GetEditorOnlyData()->EmissiveColor.Connect(0, RGB);
		M->GetEditorOnlyData()->Opacity.Connect(0, Alpha);
		// Substrate 开着时再接一个 SMF_UE4Unlit 到 Front Material（和 LGUI 自带的 LGUI_Base 一样）：
		// LGUI 屏幕空间着色器按这个函数的输出（自发光预乘覆盖率、透射 = 1 - 不透明度）还原颜色和 alpha；
		// 只接旧式引脚的话引擎自动转换出来的是另一套约定，LGUI 画出来是全黑 / 全透明
		if (UMaterialFunctionInterface* UnlitFunction = LoadObject<UMaterialFunctionInterface>(nullptr, TEXT("/Engine/Functions/Strata/SMF_UE4Unlit.SMF_UE4Unlit")))
		{
			auto* Unlit = Expression<UMaterialExpressionMaterialFunctionCall>(M);
			Unlit->SetMaterialFunction(UnlitFunction);
			int32 Connected = 0;
			TArray<FString> Names;
			for (FFunctionExpressionInput& FunctionInput : Unlit->FunctionInputs)
			{
				const FString Name = FunctionInput.ExpressionInput ? FunctionInput.ExpressionInput->InputName.ToString() : FString();
				Names.Add(Name);
				if (Name == TEXT("Color") || Name.Contains(TEXT("Emissive"))) { FunctionInput.Input.Connect(0, RGB); ++Connected; }
				else if (Name.Contains(TEXT("Opacity"))) { FunctionInput.Input.Connect(0, Alpha); ++Connected; }
			}
			UE_CLOG(Connected != 2, LogWuwaUiEffectSetup, Error, TEXT("%s: SMF_UE4Unlit inputs changed (connected %d of 2: %s)."), *PackageName, Connected, *FString::Join(Names, TEXT(", ")));
			M->GetEditorOnlyData()->FrontMaterial.Connect(0, Unlit);
		}
		M->PostEditChange();
		return SaveAsset(M) ? M : nullptr;
	}

	TArray<WuwaUiNiagaraBuilder::FAttribute> Attributes(const TArray<TSharedPtr<FJsonValue>>& Values)
	{
		TArray<WuwaUiNiagaraBuilder::FAttribute> Out;
		for (const TSharedPtr<FJsonValue>& Value : Values)
		{
			const TSharedPtr<FJsonObject> A = Value->AsObject();
			Out.Add({A->GetStringField(TEXT("name")), A->GetStringField(TEXT("type")), A->GetStringField(TEXT("hlsl"))});
		}
		return Out;
	}
}
#endif

int32 UWuwaUiEffectSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaUiEffectSetup;
	FString RecipePath;
	if (!FParse::Value(*Params, TEXT("Recipe="), RecipePath, false))
	{
		UE_LOG(LogWuwaUiEffectSetup, Error, TEXT("Usage: %s"), *HelpUsage);
		return 1;
	}
	FString Text;
	TSharedPtr<FJsonObject> Recipe;
	const FString RecipeFile = FPaths::IsRelative(RecipePath) ? FPaths::ProjectDir() / RecipePath : RecipePath;
	if (!FFileHelper::LoadFileToString(Text, *RecipeFile) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Recipe) || !Recipe)
	{
		UE_LOG(LogWuwaUiEffectSetup, Error, TEXT("Cannot read recipe %s"), *RecipeFile);
		return 1;
	}
	FContext Ctx;
	Ctx.bApply = FParse::Param(*Params, TEXT("Apply"));
	const FString Folder = Recipe->GetStringField(TEXT("folder"));
	const FString SourceRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / Recipe->GetStringField(TEXT("sourceRoot")));
	const TArray<TSharedPtr<FJsonValue>>& Systems = Recipe->GetArrayField(TEXT("systems"));
	UE_LOG(LogWuwaUiEffectSetup, Display, TEXT("%s: %d systems -> %s"), *RecipeFile, Systems.Num(), *Folder);
	if (!Ctx.bApply)
	{
		UE_LOG(LogWuwaUiEffectSetup, Display, TEXT("Dry run; pass -Apply to build."));
		return 0;
	}

	TMap<FString, UTexture2D*> Textures;
	for (const TSharedPtr<FJsonValue>& Value : Recipe->GetArrayField(TEXT("textures")))
	{
		bool bCreated = false;
		const TSharedPtr<FJsonObject> Def = Value->AsObject();
		UTexture2D* Texture = WuwaReferenceAssetImport::FindOrImportTexture(Folder, SourceRoot, *Def, bCreated);
		if (!Texture) { ++Ctx.Errors; continue; }
		if (bCreated && !SaveAsset(Texture)) ++Ctx.Errors;
		Textures.Add(Def->GetStringField(TEXT("name")), Texture);
	}
	TMap<FString, UStaticMesh*> Meshes;
	for (const TSharedPtr<FJsonValue>& Value : Recipe->GetArrayField(TEXT("meshes")))
	{
		bool bCreated = false;
		UStaticMesh* Mesh = WuwaReferenceAssetImport::FindOrImportMesh(Folder, SourceRoot, Value->AsString(), bCreated);
		if (!Mesh) { ++Ctx.Errors; continue; }
		// UWuwaUINiagara 在运行时读网格顶点（把网格粒子画进 LGUI 画布）
		if (!Mesh->bAllowCPUAccess || bCreated)
		{
			Mesh->bAllowCPUAccess = true;
			Mesh->PostEditChange();
			if (!SaveAsset(Mesh)) ++Ctx.Errors;
		}
		Meshes.Add(Value->AsString(), Mesh);
	}
	TMap<FString, UMaterial*> Materials;
	for (const TSharedPtr<FJsonValue>& Value : Recipe->GetArrayField(TEXT("materials")))
	{
		const TSharedPtr<FJsonObject> Def = Value->AsObject();
		if (UMaterial* M = BuildMaterial(Ctx, Folder, *Def, Textures)) Materials.Add(Def->GetStringField(TEXT("id")), M);
		else ++Ctx.Errors;
	}
	if (Ctx.Errors)
	{
		UE_LOG(LogWuwaUiEffectSetup, Error, TEXT("Assets failed; systems not built."));
		return 1;
	}

	for (const TSharedPtr<FJsonValue>& Value : Systems)
	{
		const TSharedPtr<FJsonObject> Def = Value->AsObject();
		const FString PackageName = Folder / Def->GetStringField(TEXT("name"));
		TArray<WuwaUiNiagaraBuilder::FEmitterSpec> Specs;
		for (const TSharedPtr<FJsonValue>& EmitterValue : Def->GetArrayField(TEXT("emitters")))
		{
			const TSharedPtr<FJsonObject> E = EmitterValue->AsObject();
			WuwaUiNiagaraBuilder::FEmitterSpec& Spec = Specs.AddDefaulted_GetRef();
			Spec.Name = FName(*E->GetStringField(TEXT("name")));
			FString MeshName;
			if (E->TryGetStringField(TEXT("mesh"), MeshName) && !MeshName.IsEmpty()) Spec.Mesh = Meshes.FindRef(MeshName);
			Spec.Material = Materials.FindRef(E->GetStringField(TEXT("material")));
			Spec.SpawnCount = static_cast<int32>(E->GetNumberField(TEXT("spawnCount")));
			Spec.SpawnTime = static_cast<float>(E->GetNumberField(TEXT("spawnTime")));
			Spec.LoopDuration = static_cast<float>(E->GetNumberField(TEXT("loopDuration")));
			Spec.SortOrder = static_cast<int32>(E->GetNumberField(TEXT("sortOrder")));
			Spec.RandomSeed = static_cast<int32>(E->GetNumberField(TEXT("randomSeed")));
			const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
			if (E->TryGetArrayField(TEXT("pivot"), Array)) Spec.Pivot = FVector2D((*Array)[0]->AsNumber(), (*Array)[1]->AsNumber());
			if (E->TryGetArrayField(TEXT("subImageSize"), Array)) Spec.SubImageSize = FVector2D((*Array)[0]->AsNumber(), (*Array)[1]->AsNumber());
			Spec.Spawn = Attributes(E->GetArrayField(TEXT("spawn")));
			Spec.Update = Attributes(E->GetArrayField(TEXT("update")));
		}
		UNiagaraSystem* System = LoadAsset<UNiagaraSystem>(PackageName);
		if (System && !Backup(Ctx, PackageName)) return 1;
		if (!System)
		{
			UPackage* Package = CreatePackage(*PackageName);
			System = NewObject<UNiagaraSystem>(Package, *FPackageName::GetShortName(PackageName), RF_Public | RF_Standalone | RF_Transactional);
			UNiagaraSystemFactoryNew::InitializeSystem(System, true);
			FAssetRegistryModule::AssetCreated(System);
		}
		if (!WuwaUiNiagaraBuilder::BuildSystem(System, Specs) || !SaveAsset(System))
		{
			++Ctx.Errors;
			continue;
		}
		UE_LOG(LogWuwaUiEffectSetup, Display, TEXT("Built %s (%d emitters)."), *PackageName, Specs.Num());
	}
	if (!Ctx.BackupDir.IsEmpty()) UE_LOG(LogWuwaUiEffectSetup, Display, TEXT("Backups in %s"), *Ctx.BackupDir);
	return Ctx.Errors ? 1 : 0;
#else
	return 1;
#endif
}
