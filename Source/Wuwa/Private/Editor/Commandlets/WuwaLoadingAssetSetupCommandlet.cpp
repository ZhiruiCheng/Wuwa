#include "Editor/Commandlets/WuwaLoadingAssetSetupCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Core/Actor/UIContainerActor.h"
#include "Core/Actor/UITextActor.h"
#include "Core/Actor/UITextureActor.h"
#include "Core/ActorComponent/UIItem.h"
#include "Core/ActorComponent/UIText.h"
#include "Core/ActorComponent/UITexture.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Game/LGUI/Component/WuwaUiComponentsRegistry.h"
#include "Game/LGUI/Core/WuwaUISettings.h"
#include "Game/Module/Loading/WuwaLoadingController.h"
#include "Game/Module/Loading/WuwaLoadingTypes.h"
#include "Game/Module/Loading/WuwaLoadingView.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "LGUIBPLibrary.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PrefabSystem/LGUIPrefab.h"
#include "PrefabSystem/LGUIPrefabManager.h"
#include "TextureCompiler.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaLoadingSetup, Log, All);

UWuwaLoadingAssetSetupCommandlet::UWuwaLoadingAssetSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpDescription = TEXT("Imports the loading-screen art, builds /Game/LGUI/Loading/Loading_Prefab and DT_LoadingShow, and adds the LoadingView row. Read-only unless -Apply.");
	HelpUsage = TEXT("-run=WuwaLoadingAssetSetup [-Apply] -AllowCommandletRendering -RenderOffscreen");
}

#if WITH_EDITOR
namespace WuwaLoadingSetup
{
	const FString TextureFolder = TEXT("/Game/LGUI/Loading/Textures");
	const FString PrefabPackage = TEXT("/Game/LGUI/Loading/Loading_Prefab");
	const FString ShowTablePackage = TEXT("/Game/LGUI/Loading/DT_LoadingShow");
	const FString ViewTablePackage = TEXT("/Game/LGUI/DT_UiViews");
	const FString SquareTexturePath = TEXT("/Game/LGUI/HUD/Textures/T_HUD_Square.T_HUD_Square");

	struct FContext
	{
		bool bApply = false;
		int32 Errors = 0;
		FString BackupDir;
		TMap<FString, UTexture2D*> Textures;
	};

	template <typename T>
	T* LoadAsset(const FString& PackageName)
	{
		return LoadObject<T>(nullptr, *(PackageName + TEXT(".") + FPackageName::GetShortName(PackageName)), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	bool Backup(FContext& Ctx, const FString& PackageName)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		if (!IFileManager::Get().FileExists(*Filename))
		{
			return true;
		}
		if (Ctx.BackupDir.IsEmpty())
		{
			Ctx.BackupDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/LoadingAssetSetup") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
		}
		if (IFileManager::Get().Copy(*(Ctx.BackupDir / FPaths::GetCleanFilename(Filename)), *Filename) != COPY_OK)
		{
			UE_LOG(LogWuwaLoadingSetup, Error, TEXT("Failed to back up %s"), *Filename);
			++Ctx.Errors;
			return false;
		}
		return true;
	}

	bool SaveAsset(FContext& Ctx, UObject* Asset)
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
			UE_LOG(LogWuwaLoadingSetup, Error, TEXT("Failed to save %s (is the editor open?)"), *Filename);
			++Ctx.Errors;
			return false;
		}
		UE_LOG(LogWuwaLoadingSetup, Display, TEXT("Saved %s"), *Package->GetName());
		return true;
	}

	/** PNG → UI 贴图。背景图很大（2560x1440），用默认的块压缩；渐变遮罩很小，保留原始 RGBA 保证过渡平滑。 */
	UTexture2D* ImportTexture(FContext& Ctx, const FString& File)
	{
		const FString Name = FPaths::GetBaseFilename(File);
		const FString PackageName = TextureFolder / Name;
		UTexture2D* Texture = LoadAsset<UTexture2D>(PackageName);
		if (!Ctx.bApply)
		{
			UE_LOG(LogWuwaLoadingSetup, Display, TEXT("Texture %s: will %s."), *Name, Texture ? TEXT("reimport") : TEXT("import"));
			return Texture;
		}

		TArray64<uint8> Compressed, Pixels;
		IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> Png = Images.CreateImageWrapper(EImageFormat::PNG);
		if (!FFileHelper::LoadFileToArray(Compressed, *File) || !Png->SetCompressed(Compressed.GetData(), Compressed.Num())
			|| !Png->GetRaw(ERGBFormat::BGRA, 8, Pixels))
		{
			UE_LOG(LogWuwaLoadingSetup, Error, TEXT("Cannot read %s"), *File);
			++Ctx.Errors;
			return Texture;
		}
		if (Texture && !Backup(Ctx, PackageName))
		{
			return Texture;
		}
		if (!Texture)
		{
			UPackage* Package = CreatePackage(*PackageName);
			Texture = NewObject<UTexture2D>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Texture);
		}
		const bool bShade = Name.Contains(TEXT("Shade"));
		Texture->PreEditChange(nullptr);
		Texture->Source.Init(Png->GetWidth(), Png->GetHeight(), 1, 1, TSF_BGRA8, Pixels.GetData());
		Texture->SRGB = true;
		Texture->CompressionSettings = bShade ? TC_EditorIcon : TC_Default;
		Texture->MipGenSettings = TMGS_NoMipmaps;
		Texture->LODGroup = TEXTUREGROUP_UI;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		if (!Texture->AssetImportData)
		{
			Texture->AssetImportData = NewObject<UAssetImportData>(Texture);
		}
		Texture->AssetImportData->Update(File);
		Texture->PostEditChange();
		SaveAsset(Ctx, Texture);
		return Texture;
	}

	/** 在 LGUI 预制体预览世界里搭节点。锚点坐标：AnchoredPosition 是 Pivot 相对锚点的偏移，Y 向上。 */
	struct FBuilder
	{
		FContext& Ctx;
		UWorld* World;

		static FUIAnchorData Anchor(FVector2D Min, FVector2D Max, FVector2D Pivot, FVector2D Pos, FVector2D SizeDelta)
		{
			FUIAnchorData Data;
			Data.AnchorMin = Min;
			Data.AnchorMax = Max;
			Data.Pivot = Pivot;
			Data.AnchoredPosition = Pos;
			Data.SizeDelta = SizeDelta;
			return Data;
		}

		static void Place(UUIItem* Item, UUIItem* Parent, const FUIAnchorData& Data)
		{
			if (Parent)
			{
				Item->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform);
			}
			Item->SetAnchorData(Data);
		}

		UUIItem* Container(UUIItem* Parent, const FString& Label, const FUIAnchorData& Data)
		{
			AUIContainerActor* Actor = World->SpawnActor<AUIContainerActor>();
			Actor->SetActorLabel(Label);
			Place(Actor->GetUIItem(), Parent, Data);
			return Actor->GetUIItem();
		}

		UUITexture* Image(UUIItem* Parent, const FString& Label, UTexture2D* Texture, const FUIAnchorData& Data, FColor Color = FColor::White,
			bool bRaycastTarget = false)
		{
			AUITextureActor* Actor = World->SpawnActor<AUITextureActor>();
			Actor->SetActorLabel(Label);
			UUITexture* Item = Actor->GetUITexture();
			Item->SetTexture(Texture);
			Item->SetColor(Color);
			Item->SetRaycastTarget(bRaycastTarget);
			Place(Item, Parent, Data);
			return Item;
		}

		UUIText* Label(UUIItem* Parent, const FString& Label, const FText& Text, const FUIAnchorData& Data, float FontSize, FColor Color,
			EUITextParagraphHorizontalAlign Align, EUITextOverflowType Overflow = EUITextOverflowType::HorizontalOverflow)
		{
			AUITextActor* Actor = World->SpawnActor<AUITextActor>();
			Actor->SetActorLabel(Label);
			UUIText* Item = Actor->GetUIText();
			Item->SetText(Text);
			Item->SetFontSize(FontSize);
			Item->SetColor(Color);
			Item->SetParagraphHorizontalAlignment(Align);
			Item->SetOverflowType(Overflow);
			Item->SetRaycastTarget(false);
			Place(Item, Parent, Data);
			return Item;
		}
	};

	/** UITexture 的 UV 按源图尺寸重写一遍（NullRHI 下 LGUI 自己算会得到 NaN，见 HUD 资产的同名处理）。 */
	void FixSpriteData(AActor* Root)
	{
		TArray<AActor*> Actors{Root};
		Root->GetAttachedActors(Actors, false, true);
		for (AActor* Actor : Actors)
		{
			UUITexture* Item = Actor ? Actor->FindComponentByClass<UUITexture>() : nullptr;
			const UTexture2D* Texture = Item ? Cast<UTexture2D>(Item->GetTexture()) : nullptr;
			if (!Texture || Texture->Source.GetSizeX() <= 0)
			{
				continue;
			}
			const int32 Width = Texture->Source.GetSizeX();
			const int32 Height = Texture->Source.GetSizeY();
			FLGUISpriteInfo SpriteData;
			SpriteData.width = Width;
			SpriteData.height = Height;
			SpriteData.ApplyUV(0, 0, Width, Height, 1.f / Width, 1.f / Height, FVector4(0, 0, 1, 1));
			SpriteData.ApplyBorderUV(1.f / Width, 1.f / Height);
			Item->SetSpriteData(SpriteData);
		}
	}

	/** 版式参照鸣潮加载界面：全屏背景、底部渐变压暗，左下标题+描述，最底部一条细进度条，右侧百分比。按 1920x1080 设计。 */
	bool BuildPrefab(FContext& Ctx)
	{
		ULGUIPrefab* Prefab = LoadAsset<ULGUIPrefab>(PrefabPackage);
		UE_LOG(LogWuwaLoadingSetup, Display, TEXT("Prefab %s: will %s."), *PrefabPackage, Prefab ? TEXT("rebuild") : TEXT("create"));
		if (!Ctx.bApply)
		{
			return true;
		}
		if (Prefab && !Backup(Ctx, PrefabPackage))
		{
			return false;
		}
		UTexture2D* Square = LoadObject<UTexture2D>(nullptr, *SquareTexturePath);
		UTexture2D* Background = Ctx.Textures.FindRef(TEXT("T_Loading_Bg_01"));
		UTexture2D* Shade = Ctx.Textures.FindRef(TEXT("T_Loading_BottomShade"));
		if (!Square || !Background || !Shade)
		{
			UE_LOG(LogWuwaLoadingSetup, Error, TEXT("Missing textures (square=%d background=%d shade=%d)."), Square != nullptr, Background != nullptr, Shade != nullptr);
			++Ctx.Errors;
			return false;
		}

		FBuilder B{Ctx, ULGUIPrefabManagerObject::GetPreviewWorldForPrefabPackage()};
		const FVector2D Zero = FVector2D::ZeroVector;
		const FVector2D Center(0.5f, 0.5f);
		const FVector2D BottomLeft(0.f, 0.f);
		const FVector2D BottomRight(1.f, 0.f);
		const FVector2D Full(1.f, 1.f);

		UUIItem* Root = B.Container(nullptr, TEXT("Loading_Prefab"), FBuilder::Anchor(Zero, Full, Center, Zero, Zero));
		// 背景铺满，同时接收点击：加载期间挡住下面 HUD 的按钮。
		UUITexture* Bg = B.Image(Root, TEXT("Background"), Background, FBuilder::Anchor(Zero, Full, Center, Zero, Zero), FColor::White, true);
		B.Image(Root, TEXT("BottomShade"), Shade, FBuilder::Anchor(BottomLeft, BottomRight, FVector2D(0.5f, 0.f), Zero, FVector2D(0.f, 460.f)));
		UUIText* Title = B.Label(Root, TEXT("Title"), FText::FromString(TEXT("标题")),
			FBuilder::Anchor(BottomLeft, BottomLeft, BottomLeft, FVector2D(100.f, 185.f), FVector2D(1200.f, 56.f)),
			40.f, FColor(233, 211, 160), EUITextParagraphHorizontalAlign::Left);
		UUIText* Description = B.Label(Root, TEXT("Description"), FText::FromString(TEXT("描述")),
			FBuilder::Anchor(BottomLeft, BottomLeft, FVector2D(0.f, 1.f), FVector2D(100.f, 172.f), FVector2D(1400.f, 90.f)),
			22.f, FColor(235, 235, 235), EUITextParagraphHorizontalAlign::Left, EUITextOverflowType::VerticalOverflow);
		UUITexture* Track = B.Image(Root, TEXT("BarTrack"), Square,
			FBuilder::Anchor(BottomLeft, BottomRight, Center, FVector2D(0.f, 60.f), FVector2D(-200.f, 4.f)), FColor(255, 255, 255, 60));
		UUITexture* Fill = B.Image(Track, TEXT("BarFill"), Square,
			FBuilder::Anchor(BottomLeft, FVector2D(0.f, 1.f), FVector2D(0.f, 0.5f), Zero, Zero), FColor(245, 245, 245));
		UUIText* Percent = B.Label(Root, TEXT("Percent"), FText::FromString(TEXT("0 %")),
			FBuilder::Anchor(BottomRight, BottomRight, BottomRight, FVector2D(-100.f, 76.f), FVector2D(200.f, 36.f)),
			26.f, FColor(245, 245, 245), EUITextParagraphHorizontalAlign::Right);

		// 注册表的下标就是 UWuwaLoadingView 里的编号。
		AActor* RootActor = Root->GetOwner();
		UWuwaUiComponentsRegistry* Registry = NewObject<UWuwaUiComponentsRegistry>(RootActor, TEXT("WuwaUiComponentsRegistry"), RF_Transactional);
		RootActor->AddInstanceComponent(Registry);
		Registry->RegisterComponent();
		Registry->SetComponents({Bg->GetOwner(), Title->GetOwner(), Description->GetOwner(), Fill->GetOwner(), Percent->GetOwner(), Track->GetOwner()});

		if (!Prefab)
		{
			UPackage* Package = CreatePackage(*PrefabPackage);
			Prefab = NewObject<ULGUIPrefab>(Package, *FPackageName::GetShortName(PrefabPackage), RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Prefab);
		}
		FixSpriteData(RootActor);
		TMap<UObject*, FGuid> ObjectToGuid;
		TMap<TObjectPtr<AActor>, FLGUISubPrefabData> SubPrefabMap;
		Prefab->SavePrefab(RootActor, ObjectToGuid, SubPrefabMap);
		ULGUIBPLibrary::DestroyActorWithHierarchy(RootActor, true);
		return SaveAsset(Ctx, Prefab);
	}

	struct FShow
	{
		const TCHAR* Texture;
		const TCHAR* Title;
		const TCHAR* Description;
	};

	/** 展示内容：背景图配一段原创短文（不照搬游戏文本）。之后在编辑器里直接改 DT_LoadingShow。 */
	bool BuildShowTable(FContext& Ctx)
	{
		const FShow Shows[] = {
			{TEXT("T_Loading_Bg_01"), TEXT("云海港湾"), TEXT("晨光照进环形的港湾，远航的船只在潮声里次第归来。\n人们说，这里的风总带着别处听不到的回声。")},
			{TEXT("T_Loading_Bg_02"), TEXT("今州晴空"), TEXT("楼阁倚着流云筑在山崖上，钟声越过层层屋檐。\n若在此停下脚步，能看见整座城慢慢醒来。")},
			{TEXT("T_Loading_Bg_03"), TEXT("雾隐城郭"), TEXT("旧城墙被薄雾笼着，只露出飞檐的轮廓。\n巡夜人留下的灯还亮着，像是在等谁回来。")},
			{TEXT("T_Loading_Bg_04"), TEXT("黄昏庭院"), TEXT("落日把庭院染成一片暖色，花瓣随风落上石阶。\n漂泊者在这里歇了口气，又一次整理好行装。")},
			{TEXT("T_Loading_Bg_05"), TEXT("金色原野"), TEXT("穿过云层的光铺满辽阔的原野，远处塔影若隐若现。\n前路还长，但脚下的路是清楚的。")},
		};
		UDataTable* Table = LoadAsset<UDataTable>(ShowTablePackage);
		UE_LOG(LogWuwaLoadingSetup, Display, TEXT("Show table %s: will %s with %d rows."), *ShowTablePackage, Table ? TEXT("update") : TEXT("create"), UE_ARRAY_COUNT(Shows));
		if (!Ctx.bApply)
		{
			return true;
		}
		if (Table && !Backup(Ctx, ShowTablePackage))
		{
			return false;
		}
		if (!Table)
		{
			UPackage* Package = CreatePackage(*ShowTablePackage);
			Table = NewObject<UDataTable>(Package, *FPackageName::GetShortName(ShowTablePackage), RF_Public | RF_Standalone | RF_Transactional);
			Table->RowStruct = FWuwaLoadingShowRow::StaticStruct();
			FAssetRegistryModule::AssetCreated(Table);
		}
		int32 Index = 1;
		for (const FShow& Show : Shows)
		{
			FWuwaLoadingShowRow Row;
			Row.Background = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(FString::Printf(TEXT("%s/%s.%s"), *TextureFolder, Show.Texture, Show.Texture)));
			Row.Title = FText::FromString(Show.Title);
			Row.Description = FText::FromString(Show.Description);
			Row.Weight = 1.f;
			Table->AddRow(*FString::Printf(TEXT("Show_%02d"), Index++), Row);
		}
		return SaveAsset(Ctx, Table);
	}

	/** DT_UiViews 的 LoadingView 行：加载层；其他行不动。 */
	bool AddViewRow(FContext& Ctx)
	{
		UDataTable* Table = LoadAsset<UDataTable>(ViewTablePackage);
		if (!Table || Table->GetRowStruct() != FWuwaUiViewConfig::StaticStruct())
		{
			UE_LOG(LogWuwaLoadingSetup, Error, TEXT("%s is missing or is not a FWuwaUiViewConfig table."), *ViewTablePackage);
			++Ctx.Errors;
			return false;
		}
		UE_LOG(LogWuwaLoadingSetup, Display, TEXT("View table: will %s row %s."), Table->FindRowUnchecked(UWuwaLoadingController::LoadingViewName) ? TEXT("overwrite") : TEXT("add"),
			*UWuwaLoadingController::LoadingViewName.ToString());
		if (!Ctx.bApply)
		{
			return true;
		}
		if (!Backup(Ctx, ViewTablePackage))
		{
			return false;
		}
		FWuwaUiViewConfig Row;
		Row.ViewClass = UWuwaLoadingView::StaticClass();
		Row.Prefab = TSoftObjectPtr<ULGUIPrefab>(FSoftObjectPath(PrefabPackage + TEXT(".") + FPackageName::GetShortName(PrefabPackage)));
		Row.Layer = EWuwaUiLayer::Loading;
		Table->AddRow(UWuwaLoadingController::LoadingViewName, Row);
		return SaveAsset(Ctx, Table);
	}
}
#endif

int32 UWuwaLoadingAssetSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaLoadingSetup;
	FContext Ctx;
	Ctx.bApply = FParse::Param(*Params, TEXT("Apply"));

	const FString SourceDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("ArtSource/UI/Loading/Textures"));
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(SourceDir / TEXT("*.png")), true, false);
	if (Files.IsEmpty())
	{
		UE_LOG(LogWuwaLoadingSetup, Error, TEXT("No PNG in %s. Run ArtSource/UI/Loading/extract_loading_art.py first."), *SourceDir);
		return 1;
	}
	for (const FString& File : Files)
	{
		if (UTexture2D* Texture = ImportTexture(Ctx, SourceDir / File))
		{
			Ctx.Textures.Add(FPaths::GetBaseFilename(File), Texture);
		}
	}
	if (Ctx.bApply)
	{
		TArray<UTexture*> ToCompile;
		for (const TPair<FString, UTexture2D*>& Pair : Ctx.Textures)
		{
			ToCompile.Add(Pair.Value);
		}
		FTextureCompilingManager::Get().FinishCompilation(ToCompile);
		const UTexture2D* Sample = Ctx.Textures.IsEmpty() ? nullptr : Ctx.Textures.CreateConstIterator()->Value;
		if (!Sample || Sample->GetSurfaceWidth() <= 0.f)
		{
			UE_LOG(LogWuwaLoadingSetup, Error, TEXT("Texture sizes are unavailable (NullRHI). Run with -AllowCommandletRendering -RenderOffscreen."));
			return 1;
		}
	}

	BuildPrefab(Ctx);
	BuildShowTable(Ctx);
	AddViewRow(Ctx);

	if (!Ctx.bApply)
	{
		UE_LOG(LogWuwaLoadingSetup, Display, TEXT("Dry run: %d source PNGs. Pass -Apply to write assets."), Files.Num());
	}
	else
	{
		UE_LOG(LogWuwaLoadingSetup, Display, TEXT("Loading assets done with %d errors.%s"), Ctx.Errors,
			Ctx.BackupDir.IsEmpty() ? TEXT("") : *(TEXT(" Backups in ") + Ctx.BackupDir));
	}
	return Ctx.Errors == 0 ? 0 : 1;
#else
	return 1;
#endif
}
