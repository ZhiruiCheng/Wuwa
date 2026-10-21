#include "Editor/Commandlets/WuwaSceneImportCommandlet.h"

#if WITH_EDITOR
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "InterchangeManager.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "MaterialShared.h"
#include "Materials/MaterialInstanceConstant.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/CollisionProfile.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaSceneImport, Log, All);

UWuwaSceneImportCommandlet::UWuwaSceneImportCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpDescription = TEXT("Imports a whole-scene glTF (exported from .blend by Tools/Blender/export_scene_gltf.py) into /Game/Environment/<Name> and a new level with outdoor lighting and a PlayerStart. Read-only unless -Apply.");
	HelpUsage = TEXT("-run=WuwaSceneImport -Source=<file.gltf> -Name=JiaoTing [-Additive=<material prefix,...>] [-Floor=Top] [-Apply] -AllowCommandletRendering");
}

#if WITH_EDITOR
namespace WuwaSceneImport
{
	template <typename T>
	T* Spawn(UWorld* World, const FString& Label, const FVector& Location = FVector::ZeroVector, const FRotator& Rotation = FRotator::ZeroRotator)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		T* Actor = World->SpawnActor<T>(Location, Rotation, Params);
		Actor->SetActorLabel(Label);
		return Actor;
	}
}
#endif

int32 UWuwaSceneImportCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaSceneImport;
	FString Source, Name;
	if (!FParse::Value(*Params, TEXT("Source="), Source) || !FParse::Value(*Params, TEXT("Name="), Name) || !FPaths::FileExists(Source))
	{
		UE_LOG(LogWuwaSceneImport, Error, TEXT("Usage: %s (Source must exist)"), *HelpUsage);
		return 1;
	}
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	// -Additive=<材质名前缀,...>：glTF 表达不了加色混合，原作里加色的材质（例如瀑布：黑底 + 发光贴图）导进来是不透明的白板
	FString AdditiveList;
	FParse::Value(*Params, TEXT("Additive="), AdditiveList);
	TArray<FString> AdditivePrefixes;
	AdditiveList.ParseIntoArray(AdditivePrefixes, TEXT(","));
	// -Floor=Top：PlayerStart 放在中心最上面那层地面（户外高台，例如大卡的战斗场地在塔顶）；默认取最低一层（室内大殿）
	FString FloorMode;
	FParse::Value(*Params, TEXT("Floor="), FloorMode);
	const bool bTopFloor = FloorMode.Equals(TEXT("Top"), ESearchCase::IgnoreCase);
	const FString Dest = FString(TEXT("/Game/Environment")) / Name;
	const FString Level = Dest / TEXT("Maps") / (TEXT("L_") + Name);
	UE_LOG(LogWuwaSceneImport, Display, TEXT("%s -> assets %s, level %s"), *Source, *Dest, *Level);
	if (!bApply)
	{
		UE_LOG(LogWuwaSceneImport, Display, TEXT("Read-only; pass -Apply to write."));
		return 0;
	}

	const FString LevelFile = FPackageName::LongPackageNameToFilename(Level, FPackageName::GetMapPackageExtension());
	if (IFileManager::Get().FileExists(*LevelFile))
	{
		const FString BackupDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/SceneImport") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
		IFileManager::Get().Copy(*(BackupDir / FPaths::GetCleanFilename(LevelFile)), *LevelFile);
		UE_LOG(LogWuwaSceneImport, Display, TEXT("Backed up level to %s"), *BackupDir);
	}
	UWorld* World = UEditorLoadingAndSavingUtils::NewBlankMap(false);
	if (!World)
	{
		UE_LOG(LogWuwaSceneImport, Error, TEXT("Cannot create a blank map."));
		return 1;
	}
	TSet<AActor*> Before;
	for (TActorIterator<AActor> It(World); It; ++It) Before.Add(*It);

	// 场景导入（项目默认的场景管线：资产 + 关卡里的 Actor），同步完成
	FImportAssetParameters ImportParams;
	ImportParams.bIsAutomated = true;
	ImportParams.bReplaceExisting = true;
	ImportParams.ImportLevel = World->PersistentLevel;
	const double Start = FPlatformTime::Seconds();
	const bool bImported = UInterchangeManager::GetInterchangeManager().ImportScene(Dest, UInterchangeManager::CreateSourceData(Source), ImportParams);
	UE_LOG(LogWuwaSceneImport, Display, TEXT("Interchange scene import %s in %.0fs"), bImported ? TEXT("succeeded") : TEXT("failed"), FPlatformTime::Seconds() - Start);
	if (!bImported) return 1;

	// 导入的 Actor（网格、灯光……）都算场景本身：打标签。范围只按网格算（灯光可能摆得很远，例如教廷有一盏聚光灯在 180 米外）
	int32 Imported = 0;
	FBox Bounds(ForceInit);
	TMap<UStaticMesh*, TArray<UStaticMeshComponent*>> Meshes;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (Before.Contains(*It) || It->IsA<AWorldSettings>()) continue;
		It->Tags.AddUnique(TEXT("WuwaProps"));
		++Imported;
		TInlineComponentArray<UStaticMeshComponent*> Components(*It);
		for (UStaticMeshComponent* Component : Components)
		{
			if (UStaticMesh* Mesh = Component->GetStaticMesh(); Mesh && Mesh->GetPathName().StartsWith(Dest))
			{
				Meshes.FindOrAdd(Mesh).Add(Component);
				Bounds += Component->Bounds.GetBox();
			}
		}
	}
	UE_LOG(LogWuwaSceneImport, Display, TEXT("Imported %d actors; mesh bounds %s (size %s cm)."), Imported, *Bounds.ToString(), *Bounds.GetSize().ToString());

	// 指定的材质实例覆盖成加色混合（在下面判断半透明、关 Nanite 之前）
	TSet<UMaterialInstanceConstant*> Additive;
	for (const TPair<UStaticMesh*, TArray<UStaticMeshComponent*>>& Pair : Meshes)
	{
		for (const FStaticMaterial& Slot : Pair.Key->GetStaticMaterials())
		{
			UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Slot.MaterialInterface);
			if (Instance && !Additive.Contains(Instance) && AdditivePrefixes.ContainsByPredicate([Instance](const FString& Prefix) { return Instance->GetName().StartsWith(Prefix); }))
			{
				Instance->PreEditChange(nullptr);
				Instance->BasePropertyOverrides.bOverride_BlendMode = true;
				Instance->BasePropertyOverrides.BlendMode = BLEND_Additive;
				Instance->PostEditChange();
				Instance->MarkPackageDirty();
				Additive.Add(Instance);
				UE_LOG(LogWuwaSceneImport, Display, TEXT("Additive: %s"), *Instance->GetPathName());
			}
		}
	}
	if (Additive.Num() < AdditivePrefixes.Num()) UE_LOG(LogWuwaSceneImport, Warning, TEXT("-Additive matched %d material instances for %d prefixes."), Additive.Num(), AdditivePrefixes.Num());

	// 网格资产的修正：
	//  - Nanite 只画不透明/遮罩：用到半透明材质（玻璃）的网格关掉 Nanite，否则半透明部分不显示；
	//  - 碰撞：Interchange 默认生成凸包，房屋、大殿的凸包会把门和内部整个堵死，改成直接用三角形（complex as simple）；
	//    只用遮罩材质的网格（叶片、藤蔓）不要碰撞（关卡里的组件设成 NoCollision）。
	int32 NaniteOff = 0, NoCollision = 0;
	for (const TPair<UStaticMesh*, TArray<UStaticMeshComponent*>>& Pair : Meshes)
	{
		UStaticMesh* Mesh = Pair.Key;
		const TArray<FStaticMaterial>& Slots = Mesh->GetStaticMaterials();
		const bool bTranslucent = Slots.ContainsByPredicate([](const FStaticMaterial& Slot) { return Slot.MaterialInterface && IsTranslucentBlendMode(*Slot.MaterialInterface); });
		const bool bFoliage = Slots.Num() > 0 && !Slots.ContainsByPredicate([](const FStaticMaterial& Slot) { return !Slot.MaterialInterface || !IsMaskedBlendMode(*Slot.MaterialInterface); });
		if (bTranslucent && Mesh->IsNaniteEnabled())
		{
			FMeshNaniteSettings Settings = Mesh->GetNaniteSettings();
			Settings.bEnabled = false;
			Mesh->SetNaniteSettings(Settings);
			Mesh->Build();
			++NaniteOff;
		}
		if (UBodySetup* Body = Mesh->GetBodySetup())
		{
			Body->CollisionTraceFlag = CTF_UseComplexAsSimple;
			// 网友整理的网格经常有一部分面朝里（材质都是双面，画面看不出来）；单面的三角形碰撞从背面打不到，
			// 大卡场地的地面就这样，人会直接掉下去。碰撞也按双面算
			Body->bDoubleSidedGeometry = true;
			Body->InvalidatePhysicsData();
			Body->CreatePhysicsMeshes();
		}
		Mesh->MarkPackageDirty();
		for (UStaticMeshComponent* Component : Pair.Value)
		{
			if (bFoliage) Component->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			Component->RecreatePhysicsState();
		}
		NoCollision += bFoliage ? 1 : 0;
	}
	UE_LOG(LogWuwaSceneImport, Display, TEXT("%d static meshes: double-sided complex-as-simple collision; Nanite disabled on %d (translucent); no collision on %d (masked only)."), Meshes.Num(), NaniteOff, NoCollision);

	// 户外光照（不带标签，预制体里不要）
	ADirectionalLight* Sun = Spawn<ADirectionalLight>(World, TEXT("Sun"), FVector(0, 0, 50000), FRotator(-45.f, 35.f, 0.f));
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

	// PlayerStart：从网格范围中心的正上方一层层往下打射线（屋顶、拱顶、楼板……），取最低的一处朝上、
	// 上方至少 3 米净空的面（室内场景就是一层地板）
	FVector StartAt = Bounds.IsValid ? Bounds.GetCenter() : FVector::ZeroVector;
	if (Bounds.IsValid)
	{
		FVector From(StartAt.X, StartAt.Y, Bounds.Max.Z + 100.0);
		const FVector To(StartAt.X, StartAt.Y, Bounds.Min.Z - 100.0);
		// 最上面一层上方是天空，不用量净空
		double CeilingZ = bTopFloor ? TNumericLimits<double>::Max() : From.Z;
		bool bFound = false;
		FHitResult Hit;
		for (int32 Layer = 0; Layer < 64 && World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility); ++Layer)
		{
			if (Hit.ImpactNormal.Z > 0.7 && CeilingZ - Hit.ImpactPoint.Z >= 300.0)
			{
				StartAt = Hit.ImpactPoint + FVector(0, 0, 100.0);
				bFound = true;
				if (bTopFloor) break;
			}
			UE_LOG(LogWuwaSceneImport, Verbose, TEXT("  layer %d at Z=%.0f normal %s"), Layer, Hit.ImpactPoint.Z, *Hit.ImpactNormal.ToString());
			CeilingZ = Hit.ImpactPoint.Z;
			From = Hit.ImpactPoint - FVector(0, 0, 1.0);
		}
		if (!bFound) UE_LOG(LogWuwaSceneImport, Warning, TEXT("No floor found under the scene center; PlayerStart left at the center."));
	}
	Spawn<APlayerStart>(World, TEXT("PlayerStart"), StartAt);

	UEditorLoadingAndSavingUtils::SaveDirtyPackages(true, true);
	if (!UEditorLoadingAndSavingUtils::SaveMap(World, Level))
	{
		UE_LOG(LogWuwaSceneImport, Error, TEXT("Failed to save %s"), *Level);
		return 1;
	}
	UEditorLoadingAndSavingUtils::SaveDirtyPackages(true, true);
	UE_LOG(LogWuwaSceneImport, Display, TEXT("Saved %s (PlayerStart at %s)"), *Level, *StartAt.ToString());
	return 0;
#else
	return 1;
#endif
}
