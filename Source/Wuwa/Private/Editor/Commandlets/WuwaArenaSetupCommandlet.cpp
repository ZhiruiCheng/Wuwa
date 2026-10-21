#include "Editor/Commandlets/WuwaArenaSetupCommandlet.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"

#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Editor/Effects/WuwaReferenceAssetImport.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "UObject/SavePackage.h"
#include "PhysicsEngine/BodySetup.h"
#include "Dom/JsonObject.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkeletalMesh.h"
#include "SkinnedAssetCompiler.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaArenaSetup, Log, All);

UWuwaArenaSetupCommandlet::UWuwaArenaSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpUsage = TEXT("-run=WuwaArenaSetup -Recipe=<abs path>.arena.json [-Apply]");
}

#if WITH_EDITOR
namespace WuwaArenaSetup
{
	const FName BossTag = TEXT("WuwaArenaBoss");

	FVector Vec(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const FVector& Default = FVector::ZeroVector)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values) || Values->Num() < 2) return Default;
		return FVector((*Values)[0]->AsNumber(), (*Values)[1]->AsNumber(), Values->Num() > 2 ? (*Values)[2]->AsNumber() : 0.0);
	}

	double Num(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, double Default)
	{
		double Value = Default;
		if (Object.IsValid()) Object->TryGetNumberField(Field, Value);
		return Value;
	}

	FLinearColor Color(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const FLinearColor& Default)
	{
		const FVector V = Vec(Object, Field, FVector(Default.R, Default.G, Default.B));
		return FLinearColor(V.X, V.Y, V.Z);
	}

	TSharedPtr<FJsonObject> Child(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		const TSharedPtr<FJsonObject>* Out = nullptr;
		return Object.IsValid() && Object->TryGetObjectField(Field, Out) ? *Out : nullptr;
	}

	/** 一处可站的地面：位置和它上方到下一层遮挡（天花板 / 上一层楼板）的净空。 */
	struct FFloor
	{
		FVector Point;
		double Clearance = 0.0;
	};

	/** 从高处往下一层层打射线，收集所有朝上、上方净空至少 3 米的面（从上到下）。 */
	TArray<FFloor> FindFloors(UWorld* World, double X, double Y)
	{
		TArray<FFloor> Floors;
		FVector From(X, Y, 1.0e6);
		const FVector To(X, Y, -1.0e6);
		double CeilingZ = TNumericLimits<double>::Max();
		FHitResult Hit;
		for (int32 Layer = 0; Layer < 64 && World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility); ++Layer)
		{
			UE_LOG(LogWuwaArenaSetup, Verbose, TEXT("  trace (%.0f, %.0f) layer %d: Z=%.0f normal %s %s"), X, Y, Layer, Hit.ImpactPoint.Z, *Hit.ImpactNormal.ToString(), *GetNameSafe(Hit.GetActor()));
			if (Hit.ImpactNormal.Z > 0.7 && CeilingZ - Hit.ImpactPoint.Z >= 300.0)
			{
				Floors.Add({Hit.ImpactPoint, CeilingZ - Hit.ImpactPoint.Z});
			}
			CeilingZ = Hit.ImpactPoint.Z;
			From = Hit.ImpactPoint - FVector(0, 0, 1.0);
		}
		return Floors;
	}

	/** 按配方的 floor 取哪一层："top" = 最上面（户外高台），"lowest" = 最下面（室内大厅的地板），数字 = 从上往下第几层。 */
	bool PickFloor(UWorld* World, double X, double Y, const FString& Mode, FVector& Out)
	{
		const TArray<FFloor> Floors = FindFloors(World, X, Y);
		if (Floors.Num() == 0) return false;
		int32 Index = 0;
		if (Mode.Equals(TEXT("lowest"), ESearchCase::IgnoreCase)) Index = Floors.Num() - 1;
		else if (Mode.IsNumeric()) Index = FMath::Clamp(FCString::Atoi(*Mode), 0, Floors.Num() - 1);
		Out = Floors[Index].Point;
		return true;
	}

	bool SaveAsset(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Package, Asset, *Filename, Args);
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

	UMaterialExpressionScalarParameter* Scalar(UMaterial* M, const TCHAR* Name, double Value)
	{
		auto* E = Expression<UMaterialExpressionScalarParameter>(M);
		E->ParameterName = Name;
		E->DefaultValue = Value;
		return E;
	}

	/**
	 * 原作 M_BottomFlowing（"星空 / 深海"地面，MI_Kan_Flo_01A_01 等）的近似重建：原作母材质图不在包里，按材质实例的参数和贴图做：
	 * 表面接近镜面（Roughness 0、青色底色），自发光 = 星云贴图在地面"深处"的视差层（按视线方向偏移 UV，越斜看偏得越多）
	 * + 一层更浅、慢慢流动的星云 + 噪声贴图取亮点做闪烁的星星。UV 按世界 XY，和网格 UV 无关。参数都露出来，可以做材质实例调。
	 */
	UMaterial* BuildStarFloor(const TSharedPtr<FJsonObject>& Def, const FString& PackageName, UTexture2D* Nebula, UTexture2D* Noise, UTexture2D* Normal)
	{
		UMaterial* M = LoadObject<UMaterial>(nullptr, *(PackageName + TEXT(".") + FPackageName::GetShortName(PackageName)), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!M)
		{
			UPackage* Package = CreatePackage(*PackageName);
			M = NewObject<UMaterial>(Package, *FPackageName::GetShortName(PackageName), RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(M);
		}
		const TSharedPtr<FJsonObject> P = Child(Def, TEXT("params"));
		M->PreEditChange(nullptr);
		M->GetExpressionCollection().Empty();
		M->MaterialDomain = MD_Surface;
		M->BlendMode = BLEND_Opaque;
		M->SetShadingModel(MSM_DefaultLit);

		auto* NebulaTex = Expression<UMaterialExpressionTextureObjectParameter>(M);
		NebulaTex->ParameterName = TEXT("Nebula");
		NebulaTex->Texture = Nebula;
		NebulaTex->SamplerType = SAMPLERTYPE_Color;
		auto* NoiseTex = Expression<UMaterialExpressionTextureObjectParameter>(M);
		NoiseTex->ParameterName = TEXT("StarNoise");
		NoiseTex->Texture = Noise;
		NoiseTex->SamplerType = SAMPLERTYPE_Grayscale;
		auto* NormalTex = Expression<UMaterialExpressionTextureObjectParameter>(M);
		NormalTex->ParameterName = TEXT("Normal");
		NormalTex->Texture = Normal;
		NormalTex->SamplerType = SAMPLERTYPE_Normal;
		auto* WorldPos = Expression<UMaterialExpressionWorldPosition>(M);
		auto* Camera = Expression<UMaterialExpressionCameraVectorWS>(M);
		auto* Time = Expression<UMaterialExpressionTime>(M);
		auto* Tint = Expression<UMaterialExpressionVectorParameter>(M);
		Tint->ParameterName = TEXT("NebulaTint");
		Tint->DefaultValue = Color(P, TEXT("nebulaTint"), FLinearColor(0.6f, 0.85f, 1.f));
		auto* Base = Expression<UMaterialExpressionVectorParameter>(M);
		Base->ParameterName = TEXT("BaseColor");
		Base->DefaultValue = Color(P, TEXT("baseColor"), FLinearColor(0.146f, 0.404f, 0.5f));

		auto* Emissive = Expression<UMaterialExpressionCustom>(M);
		Emissive->OutputType = CMOT_Float3;
		Emissive->Description = TEXT("Star floor emissive (rebuilt M_BottomFlowing)");
		Emissive->Inputs.Reset();
		Input(Emissive, TEXT("Nebula"), NebulaTex);
		Input(Emissive, TEXT("Noise"), NoiseTex);
		Input(Emissive, TEXT("WP"), WorldPos);
		Input(Emissive, TEXT("V"), Camera);
		Input(Emissive, TEXT("T"), Time);
		Input(Emissive, TEXT("Tint"), Tint);
		Input(Emissive, TEXT("Tile"), Scalar(M, TEXT("NebulaTile"), Num(P, TEXT("nebulaTile"), 3000.0)));
		Input(Emissive, TEXT("Depth"), Scalar(M, TEXT("Depth"), Num(P, TEXT("depth"), 400.0)));
		Input(Emissive, TEXT("Intensity"), Scalar(M, TEXT("NebulaIntensity"), Num(P, TEXT("nebulaIntensity"), 1.5)));
		Input(Emissive, TEXT("StarTile"), Scalar(M, TEXT("StarTile"), Num(P, TEXT("starTile"), 250.0)));
		Input(Emissive, TEXT("StarIntensity"), Scalar(M, TEXT("StarIntensity"), Num(P, TEXT("starIntensity"), 6.0)));
		Input(Emissive, TEXT("FlickerSpeed"), Scalar(M, TEXT("FlickerSpeed"), Num(P, TEXT("flickerSpeed"), 0.1)));
		Emissive->Code = TEXT(
			"float2 xy = WP.xy;\n"
			"float3 v = normalize(V);\n"
			"float2 par = -v.xy / max(v.z, 0.2);\n"
			"float3 deep = Texture2DSample(Nebula, NebulaSampler, (xy + par * Depth) / Tile).rgb;\n"
			"float3 mid = Texture2DSample(Nebula, NebulaSampler, (xy + par * Depth * 0.4) / (Tile * 0.45) + float2(T, T * 0.6) * 0.002).rgb;\n"
			"float cells = Texture2DSample(Noise, NoiseSampler, (xy + par * Depth * 0.7) / StarTile).r;\n"
			"float star = pow(saturate((cells - 0.92) / 0.08), 6.0);\n"
			"float flick = 0.55 + 0.45 * sin(T * FlickerSpeed * 6.2832 + cells * 60.0);\n"
			"return (deep + mid * 0.35) * Intensity * Tint + star * flick * StarIntensity;");

		auto* NormalCode = Expression<UMaterialExpressionCustom>(M);
		NormalCode->OutputType = CMOT_Float3;
		NormalCode->Description = TEXT("Floor detail normal (world XY UV), flattened");
		NormalCode->Inputs.Reset();
		Input(NormalCode, TEXT("N"), NormalTex);
		Input(NormalCode, TEXT("WP"), WorldPos);
		Input(NormalCode, TEXT("Tile"), Scalar(M, TEXT("NormalTile"), Num(P, TEXT("normalTile"), 400.0)));
		Input(NormalCode, TEXT("Strength"), Scalar(M, TEXT("NormalIntensity"), Num(P, TEXT("normalIntensity"), 0.2)));
		NormalCode->Code = TEXT(
			"float3 n = Texture2DSample(N, NSampler, WP.xy / Tile).xyz;\n"
			"return normalize(lerp(float3(0, 0, 1), n, Strength));");

		auto* BaseCode = Expression<UMaterialExpressionCustom>(M);
		BaseCode->OutputType = CMOT_Float3;
		BaseCode->Inputs.Reset();
		Input(BaseCode, TEXT("C"), Base);
		Input(BaseCode, TEXT("S"), Scalar(M, TEXT("BaseColorScale"), Num(P, TEXT("baseColorScale"), 0.12)));
		BaseCode->Code = TEXT("return C.rgb * S;");

		M->GetEditorOnlyData()->BaseColor.Connect(0, BaseCode);
		M->GetEditorOnlyData()->EmissiveColor.Connect(0, Emissive);
		M->GetEditorOnlyData()->Normal.Connect(0, NormalCode);
		M->GetEditorOnlyData()->Roughness.Connect(0, Scalar(M, TEXT("Roughness"), Num(P, TEXT("roughness"), 0.06)));
		M->GetEditorOnlyData()->Metallic.Connect(0, Scalar(M, TEXT("Metallic"), 0.0));
		M->PostEditChange();
		return M;
	}

	template <typename T>
	T* FindOrSpawn(UWorld* World, const FString& Label)
	{
		for (TActorIterator<T> It(World); It; ++It) return *It;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		T* Actor = World->SpawnActor<T>(Params);
		Actor->SetActorLabel(Label);
		return Actor;
	}
}
#endif

int32 UWuwaArenaSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaArenaSetup;
	FString RecipePath, Text;
	TSharedPtr<FJsonObject> Recipe;
	if (!FParse::Value(*Params, TEXT("Recipe="), RecipePath) || !FFileHelper::LoadFileToString(Text, *RecipePath)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Recipe) || !Recipe.IsValid())
	{
		UE_LOG(LogWuwaArenaSetup, Error, TEXT("Usage: %s (recipe must be readable JSON)"), *HelpUsage);
		return 1;
	}
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	// -BossOnly：只替换 Boss，不动地面材质、PlayerStart、光照、后处理（关卡里手调过的保留）
	const bool bBossOnly = FParse::Param(*Params, TEXT("BossOnly"));
	const FString Level = Recipe->GetStringField(TEXT("level"));
	const FString LevelFile = FPackageName::LongPackageNameToFilename(Level, FPackageName::GetMapPackageExtension());
	if (!IFileManager::Get().FileExists(*LevelFile))
	{
		UE_LOG(LogWuwaArenaSetup, Error, TEXT("Level %s does not exist (run WuwaSceneImport first)."), *Level);
		return 1;
	}
	if (bApply)
	{
		const FString BackupDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/ArenaSetup") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
		IFileManager::Get().Copy(*(BackupDir / FPaths::GetCleanFilename(LevelFile)), *LevelFile);
		UE_LOG(LogWuwaArenaSetup, Display, TEXT("Backed up level to %s"), *BackupDir);
	}
	UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(Level);
	if (!World)
	{
		UE_LOG(LogWuwaArenaSetup, Error, TEXT("Cannot load %s"), *Level);
		return 1;
	}

	// 载入关卡后复杂碰撞是异步烹制的，射线先打不到：同步建好物理网格、重建物理状态
	int32 Bodies = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TInlineComponentArray<UStaticMeshComponent*> Components(*It);
		for (UStaticMeshComponent* Component : Components)
		{
			UStaticMesh* Mesh = Component->GetStaticMesh();
			if (UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr)
			{
				Body->CreatePhysicsMeshes();
				Component->RecreatePhysicsState();
				++Bodies;
			}
		}
	}
	UE_LOG(LogWuwaArenaSetup, Display, TEXT("Physics meshes ready on %d components."), Bodies);

	// 场地在哪一层（见 PickFloor）；大卡的场地在宫殿大厅里面，是最下面那层
	FString FloorMode = TEXT("top");
	Recipe->TryGetStringField(TEXT("floor"), FloorMode);

	if (FParse::Param(*Params, TEXT("ProbeFloor")))
	{
		// -ProbeFloor（排查用）：中心处所有可站的层；再沿 X / Y 轴每 2 米取配方指定的那一层，打印高度，不改关卡
		for (const FFloor& Floor : FindFloors(World, 0.0, 0.0))
		{
			UE_LOG(LogWuwaArenaSetup, Display, TEXT("Floor layer at center: Z=%.0f clearance %.0f"), Floor.Point.Z, Floor.Clearance);
		}
		for (int32 Axis = 0; Axis < 2; ++Axis)
		{
			FString Line;
			for (int32 Step = -10; Step <= 10; ++Step)
			{
				FVector Floor(0, 0, -99999);
				const double D = Step * 200.0;
				PickFloor(World, Axis == 0 ? D : 0.0, Axis == 0 ? 0.0 : D, FloorMode, Floor);
				Line += FString::Printf(TEXT("%.0f:%.0f "), D, Floor.Z);
			}
			UE_LOG(LogWuwaArenaSetup, Display, TEXT("Floor along %s: %s"), Axis == 0 ? TEXT("X") : TEXT("Y"), *Line);
		}
		return 0;
	}

	// ---- 星空地面（原作 M_BottomFlowing 的近似重建），替换场景里用到原材质的网格槽位 ----
	if (const TSharedPtr<FJsonObject> StarFloor = Child(Recipe, TEXT("starFloor")); StarFloor && bApply && !bBossOnly)
	{
		const FString Folder = StarFloor->GetStringField(TEXT("folder"));
		const FString SourceRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / StarFloor->GetStringField(TEXT("sourceRoot")));
		TMap<FString, UTexture2D*> Textures;
		for (const TSharedPtr<FJsonValue>& Value : StarFloor->GetArrayField(TEXT("textures")))
		{
			const TSharedPtr<FJsonObject> TexDef = Value->AsObject();
			bool bCreated = false;
			UTexture2D* Texture = WuwaReferenceAssetImport::FindOrImportTexture(Folder, SourceRoot, *TexDef, bCreated);
			if (!Texture)
			{
				UE_LOG(LogWuwaArenaSetup, Error, TEXT("Cannot import texture %s"), *TexDef->GetStringField(TEXT("name")));
				return 1;
			}
			// 导入工具默认不压缩（给特效的打包通道用）；场景贴图：颜色走默认压缩，法线走法线压缩，噪声走灰度
			const FString Role = TexDef->GetStringField(TEXT("role"));
			const TextureCompressionSettings Wanted = Role == TEXT("normal") ? TC_Normalmap : (Role == TEXT("mask") ? TC_Grayscale : TC_Default);
			if (bCreated || Texture->CompressionSettings != Wanted)
			{
				Texture->PreEditChange(nullptr);
				Texture->CompressionSettings = Wanted;
				Texture->SRGB = Role == TEXT("color");
				Texture->LODGroup = TEXTUREGROUP_World;
				Texture->PostEditChange();
				SaveAsset(Texture);
			}
			Textures.Add(Role, Texture);
		}
		UMaterial* Material = BuildStarFloor(StarFloor, Folder / StarFloor->GetStringField(TEXT("name")),
			Textures.FindRef(TEXT("color")), Textures.FindRef(TEXT("mask")), Textures.FindRef(TEXT("normal")));
		if (!Material || !SaveAsset(Material))
		{
			UE_LOG(LogWuwaArenaSetup, Error, TEXT("Failed to build/save the star floor material."));
			return 1;
		}
		// 用到原材质（按名字前缀）的静态网格资产：槽位换成新材质并保存（网格资产本身改了，编辑器里直接可见）
		const FString Replace = StarFloor->GetStringField(TEXT("replace"));
		TSet<UStaticMesh*> Done;
		int32 Slots = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			TInlineComponentArray<UStaticMeshComponent*> Components(*It);
			for (UStaticMeshComponent* Component : Components)
			{
				UStaticMesh* Mesh = Component->GetStaticMesh();
				if (!Mesh || Done.Contains(Mesh)) continue;
				Done.Add(Mesh);
				bool bChanged = false;
				for (int32 Index = 0; Index < Mesh->GetStaticMaterials().Num(); ++Index)
				{
					UMaterialInterface* Slot = Mesh->GetMaterial(Index);
					if (!Slot || !(Slot == Material || Slot->GetName().StartsWith(Replace))) continue;
					if (Slot != Material)
					{
						Mesh->SetMaterial(Index, Material);
						bChanged = true;
					}
					++Slots;
				}
				if (bChanged)
				{
					Mesh->PostEditChange();
					SaveAsset(Mesh);
				}
			}
		}
		UE_LOG(LogWuwaArenaSetup, Display, TEXT("Star floor %s on %d mesh slots (replacing %s*)."), *Material->GetPathName(), Slots, *Replace);
	}

	// ---- 场地中心 / 玩家位置 ----
	const FVector CenterXY = Vec(Recipe, TEXT("center"));
	FVector Center;
	if (!PickFloor(World, CenterXY.X, CenterXY.Y, FloorMode, Center))
	{
		UE_LOG(LogWuwaArenaSetup, Error, TEXT("No floor under the arena center %s."), *CenterXY.ToString());
		return 1;
	}
	// 玩家站在 Boss 的哪个方向（偏航角，度）、多远
	const double PlayerYaw = Num(Recipe, TEXT("playerYaw"), 180.0);
	const double PlayerDistance = Num(Recipe, TEXT("playerDistance"), 1200.0);
	const FVector Direction = FRotator(0.0, PlayerYaw, 0.0).Vector();
	FVector PlayerFloor = Center + Direction * PlayerDistance;
	PickFloor(World, PlayerFloor.X, PlayerFloor.Y, FloorMode, PlayerFloor);
	UE_LOG(LogWuwaArenaSetup, Display, TEXT("Arena floor at %s; player floor at %s"), *Center.ToString(), *PlayerFloor.ToString());

	// ---- Boss 预览 ----
	if (const TSharedPtr<FJsonObject> Boss = Child(Recipe, TEXT("boss")))
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->Tags.Contains(BossTag)) World->DestroyActor(*It);
		}
		// 配了 class：放怪物蓝图（角色，有阵营 / 子弹表，能被锁定和打），胶囊底贴地、面朝玩家
		FString BossClassPath;
		if (Boss->TryGetStringField(TEXT("class"), BossClassPath) && !BossClassPath.IsEmpty())
		{
			UClass* BossClass = LoadClass<ACharacter>(nullptr, *BossClassPath);
			if (!BossClass)
			{
				UE_LOG(LogWuwaArenaSetup, Error, TEXT("Boss class %s not found (run WuwaMonsterSetup first)."), *BossClassPath);
				return 1;
			}
			FSkinnedAssetCompilingManager::Get().FinishAllCompilation();
			const float HalfHeight = BossClass->GetDefaultObject<ACharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* Actor = World->SpawnActor<AActor>(BossClass, Center + FVector(0.0, 0.0, HalfHeight), FRotator(0.0, PlayerYaw, 0.0), SpawnParams);
			if (!Actor)
			{
				UE_LOG(LogWuwaArenaSetup, Error, TEXT("Could not spawn %s."), *BossClassPath);
				return 1;
			}
			Actor->SetActorLabel(Boss->GetStringField(TEXT("label")));
			Actor->Tags.Add(BossTag);
			UE_LOG(LogWuwaArenaSetup, Display, TEXT("Boss %s (%s) at %s facing yaw %.0f."), *Actor->GetActorLabel(), *BossClass->GetName(), *Actor->GetActorLocation().ToString(), PlayerYaw);
		}
		else
		{
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *Boss->GetStringField(TEXT("mesh")));
		UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr, *Boss->GetStringField(TEXT("idle")));
		if (!Mesh)
		{
			UE_LOG(LogWuwaArenaSetup, Error, TEXT("Boss mesh %s not found."), *Boss->GetStringField(TEXT("mesh")));
			return 1;
		}
		// 命令行里骨骼网格是异步编译的，没编译完就用会崩
		FSkinnedAssetCompilingManager::Get().FinishAllCompilation();
		// 面朝玩家；角色网格相对角色前方偏航 meshYaw（项目和原作都是 -90），网格就是这个 Actor 的根
		const double FacingYaw = PlayerYaw;
		const FRotator MeshRotation(0.0, FacingYaw + Num(Boss, TEXT("meshYaw"), -90.0), 0.0);
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASkeletalMeshActor* Actor = World->SpawnActor<ASkeletalMeshActor>(Center, MeshRotation, SpawnParams);
		Actor->SetActorLabel(Boss->GetStringField(TEXT("label")));
		Actor->Tags.Add(BossTag);
		USkeletalMeshComponent* Body = Actor->GetSkeletalMeshComponent();
		Body->SetSkeletalMesh(Mesh);
		// 只写关卡里保存的单节点播放设置（进关卡 / 编辑器里组件按它们循环播待机）；命令行里调 SetAnimationMode 会初始化动画实例而崩溃
		if (FByteProperty* ModeProperty = FindFProperty<FByteProperty>(USkeletalMeshComponent::StaticClass(), TEXT("AnimationMode")))
		{
			ModeProperty->SetPropertyValue_InContainer(Body, static_cast<uint8>(EAnimationMode::AnimationSingleNode));
		}
		if (Idle)
		{
			Body->AnimationData.AnimToPlay = Idle;
			Body->AnimationData.bSavedLooping = true;
			Body->AnimationData.bSavedPlaying = true;
		}
		else
		{
			UE_LOG(LogWuwaArenaSetup, Warning, TEXT("Idle anim %s not found; boss stays in reference pose."), *Boss->GetStringField(TEXT("idle")));
		}
		int32 Weapons = 0;
		for (const TSharedPtr<FJsonValue>& Value : Boss->GetArrayField(TEXT("weapons")))
		{
			const TSharedPtr<FJsonObject> Weapon = Value->AsObject();
			USkeletalMesh* WeaponMesh = LoadObject<USkeletalMesh>(nullptr, *Weapon->GetStringField(TEXT("mesh")));
			if (!WeaponMesh)
			{
				UE_LOG(LogWuwaArenaSetup, Warning, TEXT("Weapon %s not found."), *Weapon->GetStringField(TEXT("mesh")));
				continue;
			}
			USkeletalMeshComponent* Component = NewObject<USkeletalMeshComponent>(Actor, *FPackageName::GetShortName(WeaponMesh->GetPathName()), RF_Transactional);
			Component->SetSkeletalMesh(WeaponMesh);
			Component->SetupAttachment(Body, FName(*Weapon->GetStringField(TEXT("socket"))));
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			// 实例组件随 Actor 存进关卡，载入关卡时注册；命令行里注册骨骼网格组件会初始化动画而崩溃，这里不注册
			Actor->AddInstanceComponent(Component);
			++Weapons;
		}
		UE_LOG(LogWuwaArenaSetup, Display, TEXT("Boss %s at %s facing yaw %.0f, %d weapons."), *Actor->GetActorLabel(), *Center.ToString(), FacingYaw, Weapons);
		}
	}

	if (bBossOnly)
	{
		if (!bApply)
		{
			UE_LOG(LogWuwaArenaSetup, Display, TEXT("Read-only; pass -Apply to save %s."), *Level);
			return 0;
		}
		if (!UEditorLoadingAndSavingUtils::SaveMap(World, Level))
		{
			UE_LOG(LogWuwaArenaSetup, Error, TEXT("Failed to save %s"), *Level);
			return 1;
		}
		UE_LOG(LogWuwaArenaSetup, Display, TEXT("Saved %s (boss only)"), *Level);
		return 0;
	}

	// ---- PlayerStart：Boss 正前方，面朝 Boss ----
	APlayerStart* Start = FindOrSpawn<APlayerStart>(World, TEXT("PlayerStart"));
	Start->SetActorLocationAndRotation(PlayerFloor + FVector(0, 0, 100.0), FRotator(0.0, PlayerYaw + 180.0, 0.0));

	// ---- 光照 ----
	const TSharedPtr<FJsonObject> Lighting = Child(Recipe, TEXT("lighting"));
	if (const TSharedPtr<FJsonObject> Sun = Child(Lighting, TEXT("sun")))
	{
		ADirectionalLight* Light = FindOrSpawn<ADirectionalLight>(World, TEXT("Sun"));
		const FVector Rotation = Vec(Sun, TEXT("rotation"), FVector(-35.0, 25.0, 0.0));
		Light->SetActorRotation(FRotator(Rotation.X, Rotation.Y, Rotation.Z));
		UDirectionalLightComponent* Component = CastChecked<UDirectionalLightComponent>(Light->GetLightComponent());
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetAtmosphereSunLight(true);
		Component->SetIntensity(Num(Sun, TEXT("intensity"), 10.0));
		Component->SetUseTemperature(true);
		Component->SetTemperature(Num(Sun, TEXT("temperature"), 6000.0));
		Component->SetLightSourceAngle(Num(Sun, TEXT("sourceAngle"), 0.5357));
	}
	if (const TSharedPtr<FJsonObject> Sky = Child(Lighting, TEXT("sky")))
	{
		ASkyLight* Light = FindOrSpawn<ASkyLight>(World, TEXT("SkyLight"));
		Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		Light->GetLightComponent()->bRealTimeCapture = true;
		Light->GetLightComponent()->SetIntensity(Num(Sky, TEXT("intensity"), 1.0));
	}
	if (const TSharedPtr<FJsonObject> Fog = Child(Lighting, TEXT("fog")))
	{
		AExponentialHeightFog* Actor = FindOrSpawn<AExponentialHeightFog>(World, TEXT("HeightFog"));
		// 雾的基准高度相对场地地面（负数 = 场地以下）
		Actor->SetActorLocation(FVector(Center.X, Center.Y, Center.Z + Num(Fog, TEXT("heightOffset"), -3000.0)));
		UExponentialHeightFogComponent* Component = Actor->GetComponent();
		Component->SetFogDensity(Num(Fog, TEXT("density"), 0.02));
		Component->SetFogHeightFalloff(Num(Fog, TEXT("heightFalloff"), 0.2));
		Component->SetStartDistance(Num(Fog, TEXT("startDistance"), 0.0));
		Component->SetFogInscatteringColor(Color(Fog, TEXT("inscatterColor"), FLinearColor(0.45f, 0.55f, 0.75f)));
	}
	if (const TSharedPtr<FJsonObject> Post = Child(Lighting, TEXT("post")))
	{
		APostProcessVolume* Volume = nullptr;
		for (TActorIterator<APostProcessVolume> It(World); It; ++It)
		{
			if (It->bUnbound) { Volume = *It; break; }
		}
		if (!Volume)
		{
			Volume = World->SpawnActor<APostProcessVolume>();
			Volume->SetActorLabel(TEXT("ArenaPostProcess"));
			Volume->bUnbound = true;
		}
		FPostProcessSettings& S = Volume->Settings;
		S.bOverride_AutoExposureMinBrightness = S.bOverride_AutoExposureMaxBrightness = S.bOverride_AutoExposureBias = true;
		S.AutoExposureMinBrightness = Num(Post, TEXT("exposureMinEV"), 8.0);
		S.AutoExposureMaxBrightness = Num(Post, TEXT("exposureMaxEV"), 12.0);
		S.AutoExposureBias = Num(Post, TEXT("exposureBias"), 0.0);
		S.bOverride_BloomIntensity = true;
		S.BloomIntensity = Num(Post, TEXT("bloom"), 0.4);
		S.bOverride_ColorSaturation = S.bOverride_ColorContrast = true;
		const double Saturation = Num(Post, TEXT("saturation"), 1.0), Contrast = Num(Post, TEXT("contrast"), 1.0);
		S.ColorSaturation = FVector4(Saturation, Saturation, Saturation, 1.0);
		S.ColorContrast = FVector4(Contrast, Contrast, Contrast, 1.0);
		S.bOverride_VignetteIntensity = true;
		S.VignetteIntensity = Num(Post, TEXT("vignette"), 0.3);
		S.bOverride_WhiteTemp = true;
		S.WhiteTemp = Num(Post, TEXT("whiteTemp"), 6500.0);
	}

	if (!bApply)
	{
		UE_LOG(LogWuwaArenaSetup, Display, TEXT("Read-only; pass -Apply to save %s."), *Level);
		return 0;
	}
	if (!UEditorLoadingAndSavingUtils::SaveMap(World, Level))
	{
		UE_LOG(LogWuwaArenaSetup, Error, TEXT("Failed to save %s"), *Level);
		return 1;
	}
	UE_LOG(LogWuwaArenaSetup, Display, TEXT("Saved %s"), *Level);
	return 0;
#else
	return 1;
#endif
}
