#include "Editor/Effects/WuwaReferenceAssetImport.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionLightVector.h"
#include "Materials/MaterialExpressionParticleDirection.h"
#include "Materials/MaterialExpressionParticlePositionWS.h"
#include "Materials/MaterialExpressionParticleRandom.h"
#include "Materials/MaterialExpressionParticleRelativeTime.h"
#include "Materials/MaterialExpressionParticleSpeed.h"
#include "Materials/MaterialExpressionPixelDepth.h"
#include "Materials/MaterialExpressionPreSkinnedPosition.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSceneDepth.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshAttributes.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaReferenceAssetImport, Log, All);

namespace WuwaReferenceAssetImport
{
#if WITH_EDITOR
	namespace
	{
		template <class T>
		T* FindAsset(const FString& PackageName)
		{
			const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
			return LoadObject<T>(nullptr, *(PackageName + TEXT(".") + AssetName), nullptr, LOAD_NoWarn);
		}

		template <class T>
		T* CreateAsset(const FString& PackageName)
		{
			if (FPackageName::DoesPackageExist(PackageName))
			{
				UE_LOG(LogWuwaReferenceAssetImport, Error, TEXT("%s exists but is not a %s."), *PackageName, *T::StaticClass()->GetName());
				return nullptr;
			}
			T* Asset = NewObject<T>(CreatePackage(*PackageName), *FPackageName::GetLongPackageAssetName(PackageName), RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Asset);
			return Asset;
		}

		TSharedPtr<FJsonObject> ReadJson(const FString& File)
		{
			FString Text;
			TSharedPtr<FJsonObject> Object;
			if (!FFileHelper::LoadFileToString(Text, *File) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object)) return nullptr;
			return Object;
		}

		FVector Vec(const TArray<TSharedPtr<FJsonValue>>& A) { return FVector(A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber()); }

		template <class T>
		T* Expression(UMaterial* Material)
		{
			T* E = NewObject<T>(Material, NAME_None, RF_Transactional);
			E->MaterialExpressionEditorX = -500;
			E->MaterialExpressionEditorY = Material->GetExpressions().Num() * 80;
			Material->GetExpressionCollection().AddExpression(E);
			return E;
		}

		void Input(UMaterialExpressionCustom* Code, FName Name, UMaterialExpression* E, int32 Output = 0)
		{
			FCustomInput I;
			I.InputName = Name;
			I.Input.Connect(Output, E);
			Code->Inputs.Add(I);
		}
	}
#endif

	UTexture2D* FindOrImportTexture(const FString& Folder, const FString& SourceRoot, const FJsonObject& Def, bool& bOutCreated)
	{
		bOutCreated = false;
#if WITH_EDITOR
		const FString PackageName = Folder / Def.GetStringField(TEXT("name"));
		if (UTexture2D* Existing = FindAsset<UTexture2D>(PackageName))
		{
			// 在编辑器里跑（Wuwa.Fx.Setup）时，上一次失败的运行可能已建好但没存盘：还不在磁盘上的当作新建，交给调用方保存
			if (!FPackageName::DoesPackageExist(PackageName)) bOutCreated = true;
			// 配方改了 sRGB 判定（例如手绘彩色贴图改成 sRGB）时同步到已导入的贴图，交给调用方保存。
			const bool bSRGB = Def.GetBoolField(TEXT("srgb"));
			if (Existing->SRGB != bSRGB)
			{
				Existing->PreEditChange(nullptr);
				Existing->SRGB = bSRGB;
				Existing->CompressionSettings = bSRGB ? TC_EditorIcon : TC_VectorDisplacementmap;
				Existing->PostEditChange();
				bOutCreated = true;
				UE_LOG(LogWuwaReferenceAssetImport, Display, TEXT("%s: sRGB -> %s"), *Existing->GetName(), bSRGB ? TEXT("on") : TEXT("off"));
			}
			return Existing;
		}
		TArray64<uint8> Compressed, Pixels;
		const FString File = SourceRoot / Def.GetStringField(TEXT("file"));
		if (!FFileHelper::LoadFileToArray(Compressed, *File))
		{
			UE_LOG(LogWuwaReferenceAssetImport, Error, TEXT("Cannot read %s"), *File);
			return nullptr;
		}
		FString Format;
		if (Def.TryGetStringField(TEXT("format"), Format) && Format == TEXT("rgba16f"))
		{
			// 顶点动画贴图（原作 TC_HDR，PF_FloatRGBA）：文件是原样的半精度像素，按像素读、不压缩、不做 mip，A 通道有数据（缩放 / 四元数 w）
			const int32 Width = static_cast<int32>(Def.GetNumberField(TEXT("width")));
			const int32 Height = static_cast<int32>(Def.GetNumberField(TEXT("height")));
			if (Compressed.Num() != int64(Width) * Height * 8)
			{
				UE_LOG(LogWuwaReferenceAssetImport, Error, TEXT("%s: %lld bytes, expected %dx%d RGBA16F"), *File, Compressed.Num(), Width, Height);
				return nullptr;
			}
			UTexture2D* Texture = CreateAsset<UTexture2D>(PackageName);
			if (!Texture) return nullptr;
			Texture->PreEditChange(nullptr);
			Texture->Source.Init(Width, Height, 1, 1, TSF_RGBA16F, Compressed.GetData());
			Texture->SRGB = false;
			Texture->CompressionSettings = TC_HDR;
			Texture->MipGenSettings = TMGS_NoMipmaps;
			Texture->Filter = TF_Nearest;
			Texture->AddressX = TA_Clamp;
			Texture->AddressY = TA_Clamp;
			Texture->NeverStream = true;
			Texture->LODGroup = TEXTUREGROUP_Effects;
			Texture->PostEditChange();
			bOutCreated = true;
			return Texture;
		}
		IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> PNG = Images.CreateImageWrapper(EImageFormat::PNG);
		if (!PNG->SetCompressed(Compressed.GetData(), Compressed.Num()) || !PNG->GetRaw(ERGBFormat::BGRA, 8, Pixels)) return nullptr;
		UTexture2D* Texture = CreateAsset<UTexture2D>(PackageName);
		if (!Texture) return nullptr;
		Texture->PreEditChange(nullptr);
		Texture->Source.Init(PNG->GetWidth(), PNG->GetHeight(), 1, 1, TSF_BGRA8, Pixels.GetData());
		Texture->SRGB = Def.GetBoolField(TEXT("srgb"));
		// 打包通道（很多羽毛轮廓在 RGB 而不是 alpha）不能被压缩糊掉。
		Texture->CompressionSettings = Texture->SRGB ? TC_EditorIcon : TC_VectorDisplacementmap;
		Texture->MipGenSettings = TMGS_FromTextureGroup;
		Texture->AddressX = TA_Wrap;
		Texture->AddressY = TA_Wrap;
		Texture->LODGroup = TEXTUREGROUP_Effects;
		if (!Texture->AssetImportData) Texture->AssetImportData = NewObject<UAssetImportData>(Texture);
		Texture->AssetImportData->Update(File);
		Texture->PostEditChange();
		bOutCreated = true;
		return Texture;
#else
		return nullptr;
#endif
	}

	UStaticMesh* FindOrImportMesh(const FString& Folder, const FString& SourceRoot, const FString& Name, bool& bOutCreated)
	{
		bOutCreated = false;
#if WITH_EDITOR
		const FString PackageName = Folder / Name;
		const TSharedPtr<FJsonObject> Json = ReadJson(SourceRoot / TEXT("Geometry") / (Name + TEXT(".json")));
		// 碎块顶点动画（刚体 VAT）的网格在 UV1 / UV2 里存碎块轴心和贴图列：其余 UV 通道也要导入，且用全精度（轴心是厘米坐标）
		const TArray<TSharedPtr<FJsonValue>>* UVChannels = nullptr;
		const int32 NumUVChannels = Json && Json->TryGetArrayField(TEXT("uvChannels"), UVChannels) ? FMath::Clamp(UVChannels->Num(), 1, 4) : 1;
		UStaticMesh* Mesh = FindAsset<UStaticMesh>(PackageName);
		if (Mesh && Mesh->GetNumUVChannels(0) >= NumUVChannels)
		{
			if (!FPackageName::DoesPackageExist(PackageName)) bOutCreated = true;  // 见 FindOrImportTexture
			return Mesh;
		}
		if (!Json)
		{
			UE_LOG(LogWuwaReferenceAssetImport, Error, TEXT("Missing geometry %s"), *Name);
			return nullptr;
		}
		const auto& Points = Json->GetArrayField(TEXT("positions"));
		const auto& Indices = Json->GetArrayField(TEXT("wedgePointIndices"));
		const auto& UVData = (*UVChannels)[0]->AsArray();
		const auto& NormalData = Json->GetArrayField(TEXT("normals"));
		const auto& ColorData = Json->GetArrayField(TEXT("colorsRGBA8"));
		if (Indices.Num() != UVData.Num() || Indices.Num() != NormalData.Num() || Indices.Num() != ColorData.Num()) return nullptr;
		for (int32 Channel = 1; Channel < NumUVChannels; ++Channel)
		{
			if ((*UVChannels)[Channel]->AsArray().Num() != Indices.Num()) return nullptr;
		}
		// 已导入但 UV 通道不够（旧版只导 UV0）：在原资产上重建
		if (!Mesh) Mesh = CreateAsset<UStaticMesh>(PackageName);
		if (!Mesh) return nullptr;
		Mesh->Modify();
		FMeshDescription Desc;
		FStaticMeshAttributes A(Desc);
		A.Register();
		auto Positions = A.GetVertexPositions();
		auto Normals = A.GetVertexInstanceNormals();
		auto Colors = A.GetVertexInstanceColors();
		auto UVs = A.GetVertexInstanceUVs();
		UVs.SetNumChannels(NumUVChannels);
		TArray<FVertexID> Verts;
		TArray<FVertexInstanceID> Wedges;
		for (const auto& P : Points)
		{
			const FVertexID V = Desc.CreateVertex();
			Positions[V] = FVector3f(Vec(P->AsArray()));
			Verts.Add(V);
		}
		for (int32 I = 0; I < Indices.Num(); ++I)
		{
			const int32 Point = static_cast<int32>(Indices[I]->AsNumber());
			if (!Verts.IsValidIndex(Point)) return nullptr;
			const FVertexInstanceID W = Desc.CreateVertexInstance(Verts[Point]);
			Wedges.Add(W);
			const auto& UV = UVData[I]->AsArray();
			const auto& C = ColorData[I]->AsArray();
			UVs.Set(W, 0, FVector2f(UV[0]->AsNumber(), UV[1]->AsNumber()));
			for (int32 Channel = 1; Channel < NumUVChannels; ++Channel)
			{
				const auto& Extra = (*UVChannels)[Channel]->AsArray()[I]->AsArray();
				UVs.Set(W, Channel, FVector2f(Extra[0]->AsNumber(), Extra[1]->AsNumber()));
			}
			Normals[W] = FVector3f(Vec(NormalData[I]->AsArray()));
			Colors[W] = FVector4f(C[0]->AsNumber() / 255.f, C[1]->AsNumber() / 255.f, C[2]->AsNumber() / 255.f, C[3]->AsNumber() / 255.f);
		}
		const FPolygonGroupID Group = Desc.CreatePolygonGroup();
		A.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Reference");
		for (const auto& T : Json->GetArrayField(TEXT("triangles")))
		{
			const auto& Tri = T->AsArray();
			FVertexInstanceID Face[3];
			for (int32 K = 0; K < 3; ++K)
			{
				const int32 W = static_cast<int32>(Tri[K]->AsNumber());
				if (!Wedges.IsValidIndex(W)) return nullptr;
				Face[K] = Wedges[W];
			}
			Desc.CreateTriangle(Group, MakeArrayView(Face));
		}
		Mesh->GetStaticMaterials().Reset();
		Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, TEXT("Reference")));
		Mesh->SetNumSourceModels(1);
		FMeshBuildSettings& BuildSettings = Mesh->GetSourceModel(0).BuildSettings;
		BuildSettings.bRecomputeNormals = false;
		BuildSettings.bRecomputeTangents = true;
		BuildSettings.bGenerateLightmapUVs = false;
		BuildSettings.bUseFullPrecisionUVs = NumUVChannels > 1;
		UStaticMesh::FBuildMeshDescriptionsParams Options;
		Options.bBuildSimpleCollision = false;
		Options.bUseHashAsGuid = true;
		Options.bAllowCpuAccess = true;
		if (!Mesh->BuildFromMeshDescriptions({&Desc}, Options)) return nullptr;
		UE_LOG(LogWuwaReferenceAssetImport, Display, TEXT("Imported %s: %d points / %d wedges / %d faces"), *Name, Points.Num(), Wedges.Num(), Desc.Triangles().Num());
		bOutCreated = true;
		return Mesh;
#else
		return nullptr;
#endif
	}

	UMaterial* BuildMaterial(const FString& Folder, const FJsonObject& Def, const TMap<FString, UTexture2D*>& Textures)
	{
#if WITH_EDITOR
		const FString PackageName = Folder / (TEXT("M_") + Def.GetStringField(TEXT("id")));
		UMaterial* M = FindAsset<UMaterial>(PackageName);
		if (!M) M = CreateAsset<UMaterial>(PackageName);
		if (!M) return nullptr;
		M->PreEditChange(nullptr);
		M->GetExpressionCollection().Empty();
		M->GetEditorOnlyData()->WorldPositionOffset = FVectorMaterialInput();
		// 原作材质实例的混合方式（MI_Add_* 为叠加）；配方没写时按半透明
		FString Blend;
		Def.TryGetStringField(TEXT("blend"), Blend);
		M->BlendMode = Blend == TEXT("additive") ? BLEND_Additive : Blend == TEXT("masked") ? BLEND_Masked : BLEND_Translucent;
		double Clip = 0.3333;
		if (Def.TryGetNumberField(TEXT("clip"), Clip)) M->OpacityMaskClipValue = static_cast<float>(Clip);
		M->SetShadingModel(MSM_Unlit);
		M->TwoSided = true;
		const bool bSprite = Def.GetBoolField(TEXT("sprite"));
		// 丝带层（武器拖尾）：配方里有 ribbon 对象；粒子相对时间 = 丝带上各点的归一化年龄
		const bool bRibbon = Def.HasTypedField<EJson::Object>(TEXT("ribbon"));
		M->bUsedWithNiagaraSprites = bSprite;
		M->bUsedWithNiagaraMeshParticles = !bSprite && !bRibbon;
		M->bUsedWithNiagaraRibbons = bRibbon;
		auto* UV = Expression<UMaterialExpressionTextureCoordinate>(M);
		auto* Age = Expression<UMaterialExpressionParticleRelativeTime>(M);
		auto* Color = Expression<UMaterialExpressionVertexColor>(M);
		auto* Art = Expression<UMaterialExpressionCustom>(M);
		Art->OutputType = CMOT_Float4;
		Art->Description = TEXT("Reconstructed packed-channel material using exported textures and sampled color curve; not original master shader");
		Art->Inputs.Reset();
		Input(Art, TEXT("UV"), UV);
		Input(Art, TEXT("Age"), Age);
		Input(Art, TEXT("VertexAlpha"), Color, 4);
		// 粒子颜色 RGB：逐粒子表的层（原作出生时按发射器时间查颜色曲线）用它代替按粒子年龄采样的颜色曲线
		Input(Art, TEXT("VertexColor"), Color, 0);
		// 每粒子随机数（Niagara 的 Particles.MaterialRandom）：翻页图集按它选帧
		Input(Art, TEXT("ParticleRandom"), Expression<UMaterialExpressionParticleRandom>(M));
		// 原作母材质用到的两项：法线与视线夹角（CenterPower / Rim）和软粒子深度差（DepthFade）
		auto* Normal = Expression<UMaterialExpressionVertexNormalWS>(M);
		auto* Camera = Expression<UMaterialExpressionCameraVectorWS>(M);
		auto* NdotV = Expression<UMaterialExpressionDotProduct>(M);
		NdotV->A.Connect(0, Normal);
		NdotV->B.Connect(0, Camera);
		Input(Art, TEXT("NdotV"), NdotV);
		if (M->BlendMode == BLEND_Masked)
		{
			// 不透明 / 遮罩材质不能读场景深度：给一个很大的常量（相当于没有软粒子淡出）
			auto* Far = Expression<UMaterialExpressionConstant>(M);
			Far->R = 1.0e6f;
			Input(Art, TEXT("DepthDelta"), Far);
		}
		else
		{
			auto* SceneDepth = Expression<UMaterialExpressionSceneDepth>(M);
			auto* PixelDepth = Expression<UMaterialExpressionPixelDepth>(M);
			auto* DepthDelta = Expression<UMaterialExpressionSubtract>(M);
			DepthDelta->A.Connect(0, SceneDepth);
			DepthDelta->B.Connect(0, PixelDepth);
			Input(Art, TEXT("DepthDelta"), DepthDelta);
		}
		for (const auto& Pair : Def.GetObjectField(TEXT("textures"))->Values)
		{
			UTexture2D* const* Texture = Textures.Find(Pair.Value->AsString());
			if (!Texture)
			{
				UE_LOG(LogWuwaReferenceAssetImport, Error, TEXT("%s: texture %s not imported"), *PackageName, *Pair.Value->AsString());
				return nullptr;
			}
			auto* T = Expression<UMaterialExpressionTextureObjectParameter>(M);
			T->ParameterName = FName(*Pair.Key);
			T->Texture = *Texture;
			T->SamplerType = (*Texture)->SRGB ? SAMPLERTYPE_Color : SAMPLERTYPE_LinearColor;
			Input(Art, FName(*Pair.Key), T);
		}
		auto* Intensity = Expression<UMaterialExpressionScalarParameter>(M);
		Intensity->ParameterName = TEXT("Intensity");
		Intensity->DefaultValue = Def.GetNumberField(TEXT("intensity"));
		Intensity->SliderMin = 0;
		Intensity->SliderMax = 12;
		Input(Art, TEXT("Intensity"), Intensity);
		Art->Code = Def.GetStringField(TEXT("shader"));
		auto* RGB = Expression<UMaterialExpressionComponentMask>(M);
		RGB->Input.Connect(0, Art);
		RGB->R = RGB->G = RGB->B = true;
		RGB->A = false;
		auto* Alpha = Expression<UMaterialExpressionComponentMask>(M);
		Alpha->Input.Connect(0, Art);
		Alpha->R = Alpha->G = Alpha->B = false;
		Alpha->A = true;
		M->GetEditorOnlyData()->EmissiveColor.Connect(0, RGB);
		if (M->BlendMode == BLEND_Masked) M->GetEditorOnlyData()->OpacityMask.Connect(0, Alpha);
		else M->GetEditorOnlyData()->Opacity.Connect(0, Alpha);
		FString Wpo;
		const TSharedPtr<FJsonObject>* ScreenQuad = nullptr;
		if (!bSprite && Def.TryGetObjectField(TEXT("screenQuad"), ScreenQuad))
		{
			// 原作 M_Refraction_MP 的顶点着色器：网格顶点按 UV 放到相机前 Distance 处、铺满视锥的面片上（(UV-0.5)·2·Scale
			// × tan(半视场) × Distance，高度再乘视口高宽比；Fit 混向等比铺法），网格本身的大小 / 位置不起作用（长离大招黑白闪屏）。
			auto* QuadUV = Expression<UMaterialExpressionTextureCoordinate>(M);
			auto* Quad = Expression<UMaterialExpressionCustom>(M);
			Quad->OutputType = CMOT_Float3;
			Quad->Inputs.Reset();
			Input(Quad, TEXT("UV"), QuadUV);
			Quad->Code = FString::Printf(TEXT(
				// 视锥铺法用 ClipToView 反投影屏幕坐标 n（等价于原作的 tan(半视场)·D、高宽比；投影带 overscan / 偏心时仍正好铺满。
				// 直接取 ClipToView[0][0] 当 tan 时只铺到屏幕的 88%，边上露出贴图的重复）
				"float2 n = (UV - 0.5) * 2.0 * %.6f;\n"
				"float D = %.6f;\n"
				"float4 c = mul(float4(n, 0.5, 1.0), ResolvedView.ClipToView);\n"
				"float3 s = c.xyz / c.w;\n"
				"s *= D / s.z;\n"
				"float T = abs(s.x / max(abs(n.x), 1e-4)) / D;\n"
				"float A = ResolvedView.ViewSizeAndInvSize.y * ResolvedView.ViewSizeAndInvSize.z;\n"
				"float3 v = s + %.6f * float3(n * max(A, T * D) - s.xy, 0.0);\n"
				"float3 target = mul(float4(v, 1.0), ResolvedView.ViewToTranslatedWorld).xyz;\n"
				"return target - GetTranslatedWorldPosition(Parameters);\n"),
				(*ScreenQuad)->GetNumberField(TEXT("scale")), (*ScreenQuad)->GetNumberField(TEXT("distance")), (*ScreenQuad)->GetNumberField(TEXT("fit")));
			M->GetEditorOnlyData()->WorldPositionOffset.Connect(0, Quad);
		}
		else if (!bSprite && Def.TryGetStringField(TEXT("wpo"), Wpo) && !Wpo.IsEmpty())
		{
			auto* P = Expression<UMaterialExpressionPreSkinnedPosition>(M);
			auto* Warp = Expression<UMaterialExpressionCustom>(M);
			Warp->OutputType = CMOT_Float3;
			Warp->Inputs.Reset();
			Input(Warp, TEXT("P"), P);
			Input(Warp, TEXT("Age"), Age);
			// 刚体顶点动画（原作 MI_PBREffect_RigidBodyVAT_MP）：碎块轴心 / 贴图列在 UV1、UV2，每帧位置 / 旋转在两张贴图里
			const TSharedPtr<FJsonObject>* Vat = nullptr;
			if (Def.TryGetObjectField(TEXT("vat"), Vat))
			{
				for (int32 Channel : {1, 2})
				{
					auto* Coord = Expression<UMaterialExpressionTextureCoordinate>(M);
					Coord->CoordinateIndex = Channel;
					Input(Warp, FName(*FString::Printf(TEXT("UV%d"), Channel)), Coord);
				}
				for (const TCHAR* Key : {TEXT("LocationTex"), TEXT("RotationTex")})
				{
					UTexture2D* const* Texture = Textures.Find((*Vat)->GetStringField(Key));
					if (!Texture)
					{
						UE_LOG(LogWuwaReferenceAssetImport, Error, TEXT("%s: VAT texture %s not imported"), *PackageName, *(*Vat)->GetStringField(Key));
						return nullptr;
					}
					auto* T = Expression<UMaterialExpressionTextureObjectParameter>(M);
					T->ParameterName = Key;
					T->Texture = *Texture;
					T->SamplerType = SAMPLERTYPE_LinearColor;
					Input(Warp, Key, T);
				}
			}
			Warp->Code = Wpo;
			auto* ToWorld = Expression<UMaterialExpressionTransform>(M);
			ToWorld->TransformSourceType = TRANSFORMSOURCE_Instance;
			ToWorld->TransformType = TRANSFORM_World;
			ToWorld->Input.Connect(0, Warp);
			M->GetEditorOnlyData()->WorldPositionOffset.Connect(0, ToWorld);
		}
		else if (bSprite && Def.TryGetStringField(TEXT("wpo"), Wpo) && !Wpo.IsEmpty())
		{
			// Sprite 的尺寸随寿命曲线（原作更新脚本里 SpriteSize = Initial × 曲线）：以粒子中心为原点在世界空间缩放面片
			auto* P = Expression<UMaterialExpressionWorldPosition>(M);
			auto* Center = Expression<UMaterialExpressionParticlePositionWS>(M);
			auto* Warp = Expression<UMaterialExpressionCustom>(M);
			Warp->OutputType = CMOT_Float3;
			Warp->Inputs.Reset();
			Input(Warp, TEXT("P"), P);
			Input(Warp, TEXT("C"), Center);
			Input(Warp, TEXT("Age"), Age);
			// 原作“按速度缩放 Sprite 尺寸”模块要用粒子速度：速率与方向（速度朝向的 Sprite 的长边沿方向）
			Input(Warp, TEXT("Speed"), Expression<UMaterialExpressionParticleSpeed>(M));
			Input(Warp, TEXT("Dir"), Expression<UMaterialExpressionParticleDirection>(M));
			Warp->Code = Wpo;
			M->GetEditorOnlyData()->WorldPositionOffset.Connect(0, Warp);
		}
		M->PostEditChange();
		return M;
#else
		return nullptr;
#endif
	}

	UMaterial* BuildDecalMaterial(const FString& Folder, const FJsonObject& Def, const TMap<FString, UTexture2D*>& Textures)
	{
#if WITH_EDITOR
		const FString PackageName = Folder / (TEXT("M_") + Def.GetStringField(TEXT("id")));
		UMaterial* M = FindAsset<UMaterial>(PackageName);
		if (!M) M = CreateAsset<UMaterial>(PackageName);
		if (!M) return nullptr;
		M->PreEditChange(nullptr);
		M->GetExpressionCollection().Empty();
		M->MaterialDomain = MD_DeferredDecal;
		M->BlendMode = BLEND_Translucent;
		M->SetShadingModel(MSM_Unlit);
		auto* Art = Expression<UMaterialExpressionCustom>(M);
		Art->OutputType = CMOT_Float4;
		Art->Description = TEXT("Reconstructed decal shader (M_Decal_MP operation order); see ArtSource prepare_attack.py");
		Art->Inputs.Reset();
		// 贴花里 TexCoord0 = 贴花空间 (z, y) * 0.5 + 0.5，LightVector = (z, y, x) * 0.5 + 0.5（x 为投影方向）
		Input(Art, TEXT("UV"), Expression<UMaterialExpressionTextureCoordinate>(M));
		Input(Art, TEXT("DecalPos"), Expression<UMaterialExpressionLightVector>(M));
		for (const auto& Pair : Def.GetObjectField(TEXT("textures"))->Values)
		{
			UTexture2D* const* Texture = Textures.Find(Pair.Value->AsString());
			if (!Texture)
			{
				UE_LOG(LogWuwaReferenceAssetImport, Error, TEXT("%s: texture %s not imported"), *PackageName, *Pair.Value->AsString());
				return nullptr;
			}
			auto* T = Expression<UMaterialExpressionTextureObjectParameter>(M);
			T->ParameterName = FName(*Pair.Key);
			T->Texture = *Texture;
			T->SamplerType = (*Texture)->SRGB ? SAMPLERTYPE_Color : SAMPLERTYPE_LinearColor;
			Input(Art, FName(*Pair.Key), T);
		}
		const TSharedPtr<FJsonObject>* Scalars = nullptr;
		if (Def.TryGetObjectField(TEXT("scalars"), Scalars))
		{
			for (const auto& Pair : (*Scalars)->Values)
			{
				auto* P = Expression<UMaterialExpressionScalarParameter>(M);
				P->ParameterName = FName(*Pair.Key);
				P->DefaultValue = static_cast<float>(Pair.Value->AsNumber());
				Input(Art, FName(*Pair.Key), P);
			}
		}
		const TSharedPtr<FJsonObject>* Vectors = nullptr;
		if (Def.TryGetObjectField(TEXT("vectors"), Vectors))
		{
			for (const auto& Pair : (*Vectors)->Values)
			{
				const TArray<TSharedPtr<FJsonValue>>& V = Pair.Value->AsArray();
				auto* P = Expression<UMaterialExpressionVectorParameter>(M);
				P->ParameterName = FName(*Pair.Key);
				P->DefaultValue = FLinearColor(V[0]->AsNumber(), V[1]->AsNumber(), V[2]->AsNumber(), V.Num() > 3 ? V[3]->AsNumber() : 0.0);
				Input(Art, FName(*Pair.Key), P, 5);  // RGBA
			}
		}
		Art->Code = Def.GetStringField(TEXT("shader"));
		auto* RGB = Expression<UMaterialExpressionComponentMask>(M);
		RGB->Input.Connect(0, Art);
		RGB->R = RGB->G = RGB->B = true;
		RGB->A = false;
		auto* Alpha = Expression<UMaterialExpressionComponentMask>(M);
		Alpha->Input.Connect(0, Art);
		Alpha->R = Alpha->G = Alpha->B = false;
		Alpha->A = true;
		M->GetEditorOnlyData()->EmissiveColor.Connect(0, RGB);
		M->GetEditorOnlyData()->Opacity.Connect(0, Alpha);
		M->PostEditChange();
		return M;
#else
		return nullptr;
#endif
	}

	UMaterial* BuildGhostMaterial(const FString& Folder, const FJsonObject& Def)
	{
#if WITH_EDITOR
		const FString PackageName = Folder / (TEXT("M_") + Def.GetStringField(TEXT("id")));
		UMaterial* M = FindAsset<UMaterial>(PackageName);
		if (!M) M = CreateAsset<UMaterial>(PackageName);
		if (!M) return nullptr;
		M->PreEditChange(nullptr);
		M->GetExpressionCollection().Empty();
		// 原作 MI 的 BasePropertyOverrides：Masked（如 MI_Ghost_Yangyang）或 Additive（如 MI_Ghost_Changli_140002）、Unlit、裁剪值 0.3333，用于骨骼网格。
		FString Blend = TEXT("masked");
		Def.TryGetStringField(TEXT("blend"), Blend);
		const bool bAdditive = Blend == TEXT("additive");
		M->MaterialDomain = MD_Surface;
		M->BlendMode = bAdditive ? BLEND_Additive : BLEND_Masked;
		M->SetShadingModel(MSM_Unlit);
		M->OpacityMaskClipValue = static_cast<float>(Def.GetNumberField(TEXT("clip")));
		M->bUsedWithSkeletalMesh = true;
		auto* Art = Expression<UMaterialExpressionCustom>(M);
		Art->OutputType = CMOT_Float4;
		Art->Description = TEXT("Reconstructed ghost shader (M_GhostNew compiled pixel shader); see ArtSource prepare_attack.py");
		Art->Inputs.Reset();
		Input(Art, TEXT("N"), Expression<UMaterialExpressionVertexNormalWS>(M));
		Input(Art, TEXT("V"), Expression<UMaterialExpressionCameraVectorWS>(M));
		// 残影 Spec 把 AlphaCurve 写进 CustomPrimitiveData[0]。
		auto* Fade = Expression<UMaterialExpressionScalarParameter>(M);
		Fade->ParameterName = TEXT("GhostFade");
		Fade->DefaultValue = 1.f;
		Fade->bUseCustomPrimitiveData = true;
		Fade->PrimitiveDataIndex = 0;
		Input(Art, TEXT("Fade"), Fade);
		for (const auto& Pair : Def.GetObjectField(TEXT("scalars"))->Values)
		{
			auto* P = Expression<UMaterialExpressionScalarParameter>(M);
			P->ParameterName = FName(*Pair.Key);
			P->DefaultValue = static_cast<float>(Pair.Value->AsNumber());
			Input(Art, FName(*Pair.Key), P);
		}
		for (const auto& Pair : Def.GetObjectField(TEXT("vectors"))->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>& V = Pair.Value->AsArray();
			auto* P = Expression<UMaterialExpressionVectorParameter>(M);
			P->ParameterName = FName(*Pair.Key);
			P->DefaultValue = FLinearColor(V[0]->AsNumber(), V[1]->AsNumber(), V[2]->AsNumber(), V[3]->AsNumber());
			Input(Art, FName(*Pair.Key), P, 5);  // RGBA
		}
		Art->Code = Def.GetStringField(TEXT("shader"));
		auto* RGB = Expression<UMaterialExpressionComponentMask>(M);
		RGB->Input.Connect(0, Art);
		RGB->R = RGB->G = RGB->B = true;
		RGB->A = false;
		auto* Alpha = Expression<UMaterialExpressionComponentMask>(M);
		Alpha->Input.Connect(0, Art);
		Alpha->R = Alpha->G = Alpha->B = false;
		Alpha->A = true;
		M->GetEditorOnlyData()->EmissiveColor.Connect(0, RGB);
		// 叠加版的颜色已在着色器里乘过 alpha（原作 o0 = 颜色 × alpha、o0.w = 0）
		if (bAdditive) M->GetEditorOnlyData()->Opacity.Connect(0, Alpha);
		else M->GetEditorOnlyData()->OpacityMask.Connect(0, Alpha);
		M->PostEditChange();
		return M;
#else
		return nullptr;
#endif
	}

	UMaterial* BuildRadialBlurMaterial(const FString& PackageName)
	{
#if WITH_EDITOR
		UMaterial* M = FindAsset<UMaterial>(PackageName);
		if (!M) M = CreateAsset<UMaterial>(PackageName);
		if (!M) return nullptr;
		M->PreEditChange(nullptr);
		M->GetExpressionCollection().Empty();
		M->MaterialDomain = MD_PostProcess;
		// 在 DOF 之后、Bloom / 色调映射之前：模糊作用于含半透明特效的 HDR 场景色。
		M->BlendableLocation = BL_SceneColorAfterDOF;
		auto* Art = Expression<UMaterialExpressionCustom>(M);
		Art->OutputType = CMOT_Float3;
		Art->Description = TEXT("Radial blur (reconstruction; Kuro's radial blur shader is not in the export)");
		Art->Inputs.Reset();
		// 连上一个 PostProcessInput0 节点，材质才会绑定场景色输入；HLSL 里用 SceneTextureLookup 多次采样。
		auto* Scene = Expression<UMaterialExpressionSceneTexture>(M);
		Scene->SceneTextureId = PPI_PostProcessInput0;
		Input(Art, TEXT("SceneColor"), Scene);
		auto Scalar = [M, Art](const TCHAR* Name, float Default)
		{
			auto* P = Expression<UMaterialExpressionScalarParameter>(M);
			P->ParameterName = Name;
			P->DefaultValue = Default;
			Input(Art, Name, P);
		};
		Scalar(TEXT("Intensity"), 0.f);
		Scalar(TEXT("Radius"), 1.f);
		Scalar(TEXT("Hardness"), 1.f);
		auto* Center = Expression<UMaterialExpressionVectorParameter>(M);
		Center->ParameterName = TEXT("Center");
		Center->DefaultValue = FLinearColor(0.5f, 0.5f, 0.f, 0.f);
		Input(Art, TEXT("Center"), Center);
		// 重建公式：每个像素沿指向中心的方向取 16 个样本，采样跨度 = Intensity × 距离权重；
		// 距离权重 = (到中心的距离 / Radius)^Hardness（按屏幕宽高比校正），中心不模糊、越往外越强。
		Art->Code = TEXT(
			"float2 vuv=GetViewportUV(Parameters);\n"
			"float2 d=vuv-Center.xy;\n"
			"float aspect=View.ViewSizeAndInvSize.x*View.ViewSizeAndInvSize.w;\n"
			"float w=Intensity*pow(saturate(length(d*float2(aspect,1.0))/max(Radius,1e-4)),max(Hardness,1e-4));\n"
			"if(w<=1e-5) return SceneColor.rgb;\n"
			"float3 acc=0;\n"
			"const int N=16;\n"
			"for(int i=0;i<N;++i)\n"
			"{\n"
			"  float2 uv=ViewportUVToSceneTextureUV(Center.xy+d*(1.0-w*i/(N-1.0)),PPI_PostProcessInput0);\n"
			"  acc+=SceneTextureLookup(ClampSceneTextureUV(uv,PPI_PostProcessInput0),PPI_PostProcessInput0,true).rgb;\n"
			"}\n"
			"return acc/N;\n");
		M->GetEditorOnlyData()->EmissiveColor.Connect(0, Art);
		M->PostEditChange();
		return M;
#else
		return nullptr;
#endif
	}
}
