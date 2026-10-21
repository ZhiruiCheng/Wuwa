#include "Editor/Commandlets/WuwaHudAssetSetupCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Core/Actor/UIBaseActor.h"
#include "Core/Actor/UIContainerActor.h"
#include "Core/Actor/UITextActor.h"
#include "Core/Actor/UITextureActor.h"
#include "Core/ActorComponent/UIItem.h"
#include "Core/ActorComponent/UIText.h"
#include "Core/ActorComponent/UITexture.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Interaction/UIButtonComponent.h"
#include "LGUIBPLibrary.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PrefabSystem/LGUIPrefab.h"
#include "PrefabSystem/LGUIPrefabManager.h"
#include "TextureCompiler.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaHudSetup, Log, All);

UWuwaHudAssetSetupCommandlet::UWuwaHudAssetSetupCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

#if WITH_EDITOR
namespace WuwaHudSetup
{
	const FString TextureFolder = TEXT("/Game/LGUI/HUD/Textures");
	const FString PrefabFolder = TEXT("/Game/LGUI/HUD");

	struct FContext
	{
		bool bApply = false;
		bool bRebuild = false;
		int32 Errors = 0;
		int32 TexturesWritten = 0;
		int32 PrefabsWritten = 0;
		FString BackupDir;
		TMap<FString, UTexture2D*> Textures;
	};

	template <typename T>
	T* LoadAsset(const FString& PackageName)
	{
		const FString ObjectPath = PackageName + TEXT(".") + FPackageName::GetShortName(PackageName);
		return LoadObject<T>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	bool SaveAsset(UObject* Asset)
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
			UE_LOG(LogWuwaHudSetup, Error, TEXT("Failed to save %s"), *Filename);
			return false;
		}
		return true;
	}

	/** 覆盖已有资产前备份到 Saved/Backups/HudAssetSetup/<时间>/。 */
	bool Backup(FContext& Ctx, const FString& PackageName)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		if (!IFileManager::Get().FileExists(*Filename))
		{
			return true;
		}
		if (Ctx.BackupDir.IsEmpty())
		{
			Ctx.BackupDir = FPaths::ProjectSavedDir() / TEXT("Backups/HudAssetSetup") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
		}
		const FString Target = Ctx.BackupDir / FPaths::GetCleanFilename(Filename);
		if (IFileManager::Get().Copy(*Target, *Filename) != COPY_OK)
		{
			UE_LOG(LogWuwaHudSetup, Error, TEXT("Failed to back up %s"), *Filename);
			return false;
		}
		return true;
	}

	/** PNG → UI 贴图：sRGB、UserInterface2D 压缩、不生成 mip、Clamp、UI 贴图组。 */
	UTexture2D* ImportTexture(FContext& Ctx, const FString& File)
	{
		const FString Name = FPaths::GetBaseFilename(File);
		const FString PackageName = TextureFolder / Name;
		UTexture2D* Texture = LoadAsset<UTexture2D>(PackageName);
		if (Texture && !Ctx.bRebuild)
		{
			return Texture;
		}
		if (!Ctx.bApply)
		{
			UE_LOG(LogWuwaHudSetup, Display, TEXT("Texture %s: will %s."), *Name, Texture ? TEXT("reimport") : TEXT("import"));
			return Texture;
		}

		TArray64<uint8> Compressed, Pixels;
		IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> Png = Images.CreateImageWrapper(EImageFormat::PNG);
		if (!FFileHelper::LoadFileToArray(Compressed, *File) || !Png->SetCompressed(Compressed.GetData(), Compressed.Num())
			|| !Png->GetRaw(ERGBFormat::BGRA, 8, Pixels))
		{
			UE_LOG(LogWuwaHudSetup, Error, TEXT("Cannot read %s"), *File);
			++Ctx.Errors;
			return Texture;
		}

		if (Texture && !Backup(Ctx, PackageName))
		{
			++Ctx.Errors;
			return Texture;
		}
		if (!Texture)
		{
			UPackage* Package = CreatePackage(*PackageName);
			Texture = NewObject<UTexture2D>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Texture);
		}
		Texture->PreEditChange(nullptr);
		Texture->Source.Init(Png->GetWidth(), Png->GetHeight(), 1, 1, TSF_BGRA8, Pixels.GetData());
		Texture->SRGB = true;
		Texture->CompressionSettings = TC_EditorIcon;
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
		if (SaveAsset(Texture))
		{
			++Ctx.TexturesWritten;
		}
		else
		{
			++Ctx.Errors;
		}
		return Texture;
	}

	/** 在 LGUI 预制体预览世界里搭界面节点。坐标是 LGUI 的锚点坐标：中心为原点，Y 向上。 */
	struct FBuilder
	{
		FContext& Ctx;
		UWorld* World;

		UTexture2D* Tex(const FString& Name)
		{
			UTexture2D* Texture = Ctx.Textures.FindRef(Name);
			if (!Texture)
			{
				UE_LOG(LogWuwaHudSetup, Error, TEXT("Missing texture %s"), *Name);
				++Ctx.Errors;
			}
			return Texture;
		}

		/** 按贴图原始比例缩放到不超过 MaxSize。 */
		FVector2D Fit(const FString& Name, float MaxSize)
		{
			const UTexture2D* Texture = Tex(Name);
			if (!Texture || Texture->Source.GetSizeX() <= 0)
			{
				return FVector2D(MaxSize, MaxSize);
			}
			const FVector2D Native(Texture->Source.GetSizeX(), Texture->Source.GetSizeY());
			return Native * (MaxSize / FMath::Max(Native.X, Native.Y));
		}

		static void Place(UUIItem* Item, UUIItem* Parent, FVector2D Pos, FVector2D Size)
		{
			if (Parent)
			{
				Item->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform);
			}
			Item->SetWidth(Size.X);
			Item->SetHeight(Size.Y);
			Item->SetAnchoredPosition(Pos);
		}

		UUIItem* Container(UUIItem* Parent, const FString& Label, FVector2D Size, FVector2D Pos = FVector2D::ZeroVector)
		{
			AUIContainerActor* Actor = World->SpawnActor<AUIContainerActor>();
			Actor->SetActorLabel(Label);
			UUIItem* Item = Actor->GetUIItem();
			Place(Item, Parent, Pos, Size);
			return Item;
		}

		UUITexture* Image(UUIItem* Parent, const FString& Label, const FString& TexName, FVector2D Pos, FVector2D Size,
			FColor Color = FColor::White, bool bRaycastTarget = false)
		{
			AUITextureActor* Actor = World->SpawnActor<AUITextureActor>();
			Actor->SetActorLabel(Label);
			UUITexture* Item = Actor->GetUITexture();
			Item->SetTexture(Tex(TexName));
			Item->SetColor(Color);
			Item->SetRaycastTarget(bRaycastTarget);
			Place(Item, Parent, Pos, Size);
			return Item;
		}

		UUIText* Label(UUIItem* Parent, const FString& Label, const FString& Text, FVector2D Pos, FVector2D Size, float FontSize,
			FColor Color = FColor::White, EUITextParagraphHorizontalAlign Align = EUITextParagraphHorizontalAlign::Center)
		{
			AUITextActor* Actor = World->SpawnActor<AUITextActor>();
			Actor->SetActorLabel(Label);
			UUIText* Item = Actor->GetUIText();
			Item->SetText(FText::FromString(Text));
			Item->SetFontSize(FontSize);
			Item->SetColor(Color);
			Item->SetParagraphHorizontalAlignment(Align);
			Item->SetRaycastTarget(false);
			Place(Item, Parent, Pos, Size);
			return Item;
		}

		/**
		 * 把根节点做成按钮：根节点接收点击（整块区域都能点），过渡效果作用在 TransitionTarget（通常是图标）上。
		 * 过渡用 LGUI 自带的 ColorTint：平时略暗，悬停变亮，按下变暗，禁用半透明。
		 */
		static UUIButtonComponent* MakeButton(UUIItem* Root, UUIItem* TransitionTarget)
		{
			AActor* Owner = Root->GetOwner();
			UUIButtonComponent* Button = NewObject<UUIButtonComponent>(Owner, TEXT("UIButton"), RF_Transactional);
			Owner->AddInstanceComponent(Button);
			Button->RegisterComponent();
			Root->SetRaycastTarget(true);

			if (AUIBaseRenderableActor* Target = TransitionTarget ? Cast<AUIBaseRenderableActor>(TransitionTarget->GetOwner()) : nullptr)
			{
				// Transition 没有公开的 setter，用反射设成 ColorTint。
				if (FEnumProperty* Property = FindFProperty<FEnumProperty>(UUISelectableComponent::StaticClass(), TEXT("Transition")))
				{
					Property->GetUnderlyingProperty()->SetIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Button),
						static_cast<int64>(UISelectableTransitionType::ColorTint));
				}
				Button->SetTransitionTarget(Target);
				Button->SetNormalColor(FColor(222, 222, 222));
				Button->SetHighlightedColor(FColor::White);
				Button->SetPressedColor(FColor(160, 160, 160));
				Button->SetDisabledColor(FColor(140, 140, 140, 140));
			}
			return Button;
		}
	};

	void CollectHierarchy(AActor* Actor, TArray<AActor*>& Out)
	{
		Out.Add(Actor);
		TArray<AActor*> Children;
		Actor->GetAttachedActors(Children);
		for (AActor* Child : Children)
		{
			CollectHierarchy(Child, Out);
		}
	}

	/**
	 * UITexture 的 UV 存在序列化的 spriteData 里，由 LGUI 按贴图的实际尺寸（GetSurfaceWidth）算。
	 * 这个工具在 NullRHI 下运行，实际尺寸是 0，LGUI 在编辑器里自动重算（比如实例化子预制体时）会得到 NaN，
	 * 运行时整张图都采样到一个点上。所以保存前统一用源图尺寸重写一遍。
	 */
	void FixSpriteData(AActor* Root)
	{
		TArray<AActor*> Actors;
		CollectHierarchy(Root, Actors);
		for (AActor* Actor : Actors)
		{
			UUITexture* Item = Actor->FindComponentByClass<UUITexture>();
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

	// —— 各个 HUD 元素 ——

	/** 顶部功能按钮：根节点是按钮（64x64 都能点），下面是图标和红点（默认隐藏，有新内容时由业务打开）。 */
	UUIItem* BuildFuncButton(FBuilder& B, const FString& Name, const FString& IconTex)
	{
		UUIItem* Root = B.Container(nullptr, Name, FVector2D(64, 64));
		UUITexture* Icon = B.Image(Root, TEXT("Icon"), IconTex, FVector2D::ZeroVector, B.Fit(IconTex, 56.f));
		UUITexture* Dot = B.Image(Root, TEXT("RedDot"), TEXT("T_HUD_RedDot"), FVector2D(26, 26), FVector2D(18, 18));
		Dot->SetIsUIActive(false);
		FBuilder::MakeButton(Root, Icon);
		return Root;
	}

	UUIItem* BuildMiniMap(FBuilder& B)
	{
		UUIItem* Root = B.Container(nullptr, TEXT("HUD_MiniMap"), FVector2D(210, 210));
		B.Image(Root, TEXT("Bg"), TEXT("T_HUD_MiniMap_Bg"), FVector2D::ZeroVector, FVector2D(196, 196), FColor(255, 255, 255, 200));
		// 地图画面放这里（需要圆形遮罩，按地图系统的实现来接）。
		B.Container(Root, TEXT("MapContent"), FVector2D(190, 190));
		UUITexture* Frame = B.Image(Root, TEXT("Frame"), TEXT("T_HUD_MiniMap_Frame"), FVector2D::ZeroVector, FVector2D(210, 210));
		B.Image(Root, TEXT("PlayerArrow"), TEXT("T_HUD_MiniMap_PlayerArrow"), FVector2D::ZeroVector, B.Fit(TEXT("T_HUD_MiniMap_PlayerArrow"), 36.f));
		// 点小地图打开大地图。
		FBuilder::MakeButton(Root, Frame);
		return Root;
	}

	UUIItem* BuildQuestTracker(FBuilder& B)
	{
		UUIItem* Root = B.Container(nullptr, TEXT("HUD_QuestTracker"), FVector2D(380, 120));
		B.Image(Root, TEXT("TypeBg"), TEXT("T_HUD_Circle"), FVector2D(-168, 36), FVector2D(34, 34), FColor(104, 96, 228));
		B.Image(Root, TEXT("TypeIcon"), TEXT("T_HUD_QuestTrack_Icon"), FVector2D(-168, 36), B.Fit(TEXT("T_HUD_QuestTrack_Icon"), 22.f));
		const EUITextParagraphHorizontalAlign Left = EUITextParagraphHorizontalAlign::Left;
		UUIText* Title = B.Label(Root, TEXT("Title"), TEXT("Quest Title"), FVector2D(20, 36), FVector2D(330, 34), 26.f, FColor::White, Left);
		B.Label(Root, TEXT("Objective"), TEXT("Find the mysterious person"), FVector2D(20, -4), FVector2D(330, 30), 22.f, FColor::White, Left);
		B.Label(Root, TEXT("Hint"), TEXT("Go to the destination"), FVector2D(20, -38), FVector2D(330, 28), 20.f, FColor(240, 200, 90), Left);
		// 点任务追踪打开任务界面。
		FBuilder::MakeButton(Root, Title);
		return Root;
	}

	UUIItem* BuildNetworkPing(FBuilder& B)
	{
		UUIItem* Root = B.Container(nullptr, TEXT("HUD_NetworkPing"), FVector2D(150, 32));
		B.Image(Root, TEXT("Signal"), TEXT("T_HUD_Signal_2"), FVector2D(-58, 0), FVector2D(28, 28));
		B.Label(Root, TEXT("Ping"), TEXT("152ms"), FVector2D(16, 0), FVector2D(110, 30), 24.f, FColor(250, 205, 80), EUITextParagraphHorizontalAlign::Left);
		return Root;
	}

	UUIItem* BuildTeamSlot(FBuilder& B)
	{
		UUIItem* Root = B.Container(nullptr, TEXT("HUD_TeamSlot"), FVector2D(120, 130));
		UUITexture* Head = B.Image(Root, TEXT("Head"), TEXT("T_HUD_Team_Head_Changli"), FVector2D(0, 10), FVector2D(96, 96));
		B.Image(Root, TEXT("Frame"), TEXT("T_HUD_Team_Frame"), FVector2D(0, 10), FVector2D(112, 112));
		UUITexture* Tag = B.Image(Root, TEXT("NumberBg"), TEXT("T_HUD_Square"), FVector2D(-50, 52), FVector2D(26, 26), FColor(236, 236, 236));
		B.Label(Tag, TEXT("Number"), TEXT("1"), FVector2D::ZeroVector, FVector2D(26, 26), 20.f, FColor(40, 40, 40));
		B.Image(Root, TEXT("HpBg"), TEXT("T_HUD_Square"), FVector2D(0, -52), FVector2D(92, 7), FColor(30, 30, 30, 200));
		B.Image(Root, TEXT("HpFill"), TEXT("T_HUD_HpBar_Fill"), FVector2D(0, -52), FVector2D(88, 5), FColor(225, 225, 225));
		// 点头像切换角色。
		FBuilder::MakeButton(Root, Head);
		return Root;
	}

	UUIItem* BuildPlayerStatus(FBuilder& B)
	{
		UUIItem* Root = B.Container(nullptr, TEXT("HUD_PlayerStatus"), FVector2D(760, 96));
		B.Image(Root, TEXT("LineLeft"), TEXT("T_HUD_Status_Line"), FVector2D(-200, 16), FVector2D(300, 38));
		UUITexture* Right = B.Image(Root, TEXT("LineRight"), TEXT("T_HUD_Status_Line"), FVector2D(200, 16), FVector2D(300, 38));
		Right->SetRelativeScale3D(FVector(-1.f, 1.f, 1.f));
		B.Image(Root, TEXT("Emblem"), TEXT("T_HUD_Status_Emblem"), FVector2D(0, 24), B.Fit(TEXT("T_HUD_Status_Emblem"), 64.f));
		B.Image(Root, TEXT("HpFrame"), TEXT("T_HUD_HpBar_Frame"), FVector2D(0, -22), FVector2D(500, 52));
		B.Image(Root, TEXT("HpFill"), TEXT("T_HUD_HpBar_Fill"), FVector2D(0, -22), FVector2D(456, 18), FColor(236, 236, 236));
		B.Label(Root, TEXT("HpText"), TEXT("16947/16947"), FVector2D(0, -22), FVector2D(300, 30), 22.f, FColor(40, 40, 40));
		B.Label(Root, TEXT("Level"), TEXT("Lv.90"), FVector2D(310, -20), FVector2D(100, 34), 26.f, FColor::White, EUITextParagraphHorizontalAlign::Left);
		return Root;
	}

	/** 技能按钮：底圆 + 技能图标 + 就绪光圈（默认隐藏）+ 键位框和键位字母。 */
	UUIItem* BuildSkillButton(FBuilder& B, UUIItem* Parent, const FString& Name, const FString& Key, const FString& IconTex, FVector2D Pos)
	{
		UUIItem* Root = B.Container(Parent, Name, FVector2D(84, 104), Pos);
		B.Image(Root, TEXT("Bg"), TEXT("T_HUD_Skill_Bg"), FVector2D(0, 12), FVector2D(74, 74), FColor(255, 255, 255, 230));
		UUITexture* Icon = B.Image(Root, TEXT("Icon"), IconTex, FVector2D(0, 12), B.Fit(IconTex, 58.f));
		UUITexture* Ready = B.Image(Root, TEXT("ReadyRing"), TEXT("T_HUD_Skill_ReadyRing"), FVector2D(0, 12), FVector2D(84, 84));
		Ready->SetIsUIActive(false);
		B.Image(Root, TEXT("KeyFrame"), TEXT("T_HUD_Skill_KeyFrame"), FVector2D(0, -38), FVector2D(24, 24));
		B.Label(Root, TEXT("Key"), Key, FVector2D(0, -38), FVector2D(24, 24), 18.f);
		FBuilder::MakeButton(Root, Icon);
		return Root;
	}

	UUIItem* BuildSkillBar(FBuilder& B)
	{
		UUIItem* Root = B.Container(nullptr, TEXT("HUD_SkillBar"), FVector2D(430, 110));
		// 长离的技能图标和 T/E/Q/R 的对应关系是占位，按实际技能替换 Icon 的贴图。
		BuildSkillButton(B, Root, TEXT("Skill_T"), TEXT("T"), TEXT("T_HUD_Skill_Changli_00"), FVector2D(-150, 0));
		BuildSkillButton(B, Root, TEXT("Skill_E"), TEXT("E"), TEXT("T_HUD_Skill_Changli_01"), FVector2D(-50, 0));
		BuildSkillButton(B, Root, TEXT("Skill_Q"), TEXT("Q"), TEXT("T_HUD_Skill_Changli_02"), FVector2D(50, 0));
		BuildSkillButton(B, Root, TEXT("Skill_R"), TEXT("R"), TEXT("T_HUD_Skill_Changli_05"), FVector2D(150, 0));
		return Root;
	}

	UUIItem* BuildChat(FBuilder& B)
	{
		UUIItem* Root = B.Container(nullptr, TEXT("HUD_Chat"), FVector2D(52, 46));
		UUITexture* Icon = B.Image(Root, TEXT("Icon"), TEXT("T_HUD_Chat"), FVector2D::ZeroVector, B.Fit(TEXT("T_HUD_Chat"), 42.f));
		FBuilder::MakeButton(Root, Icon);
		return Root;
	}

	UUIItem* BuildSubtitle(FBuilder& B)
	{
		UUIItem* Root = B.Container(nullptr, TEXT("HUD_Subtitle"), FVector2D(980, 56));
		B.Image(Root, TEXT("Bg"), TEXT("T_HUD_Subtitle_Bg"), FVector2D::ZeroVector, FVector2D(980, 56), FColor(255, 255, 255, 210));
		B.Label(Root, TEXT("Text"), TEXT("Subtitle text"), FVector2D::ZeroVector, FVector2D(940, 44), 28.f);
		return Root;
	}

	/** 生成（或覆盖）一个预制体；Build 返回根节点。 */
	bool WritePrefab(FContext& Ctx, const FString& Name, TFunctionRef<UUIItem*(FBuilder&)> Build)
	{
		const FString PackageName = PrefabFolder / Name;
		ULGUIPrefab* Prefab = LoadAsset<ULGUIPrefab>(PackageName);
		if (Prefab && !Ctx.bRebuild)
		{
			UE_LOG(LogWuwaHudSetup, Display, TEXT("Prefab %s exists, skipped (use -Rebuild to overwrite)."), *Name);
			return true;
		}
		if (!Ctx.bApply)
		{
			UE_LOG(LogWuwaHudSetup, Display, TEXT("Prefab %s: will %s."), *Name, Prefab ? TEXT("rebuild") : TEXT("create"));
			return true;
		}
		if (Prefab && !Backup(Ctx, PackageName))
		{
			return false;
		}

		FBuilder Builder{Ctx, ULGUIPrefabManagerObject::GetPreviewWorldForPrefabPackage()};
		UUIItem* RootItem = Build(Builder);
		AActor* Root = RootItem ? RootItem->GetOwner() : nullptr;
		if (!Root)
		{
			return false;
		}
		if (!Prefab)
		{
			UPackage* Package = CreatePackage(*PackageName);
			Prefab = NewObject<ULGUIPrefab>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Prefab);
		}
		FixSpriteData(Root);
		TMap<UObject*, FGuid> ObjectToGuid;
		TMap<TObjectPtr<AActor>, FLGUISubPrefabData> SubPrefabMap;
		Prefab->SavePrefab(Root, ObjectToGuid, SubPrefabMap);
		ULGUIBPLibrary::DestroyActorWithHierarchy(Root, true);
		if (!SaveAsset(Prefab))
		{
			return false;
		}
		++Ctx.PrefabsWritten;
		UE_LOG(LogWuwaHudSetup, Display, TEXT("Saved %s"), *PackageName);
		return true;
	}

	// —— HUD_Main：把上面的预制体按截图位置拼起来（拷贝，不是嵌套引用；改了单个预制体后用 -Rebuild 重新拼）——

	template <typename T>
	T* FindInHierarchy(AActor* Root)
	{
		TArray<AActor*> Actors;
		CollectHierarchy(Root, Actors);
		for (AActor* Actor : Actors)
		{
			if (T* Component = Actor->FindComponentByClass<T>())
			{
				return Component;
			}
		}
		return nullptr;
	}

	/** 实例化一个 HUD 子预制体，锚到父节点的某个点上（Anchor：0~1，左下为 0,0）。 */
	AActor* AddInstance(FBuilder& B, UUIItem* Parent, const FString& Name, FVector2D Anchor, FVector2D Pos)
	{
		ULGUIPrefab* Prefab = LoadAsset<ULGUIPrefab>(PrefabFolder / Name);
		AActor* Actor = Prefab ? Prefab->LoadPrefabInEditor(B.World, Parent, true) : nullptr;
		UUIItem* Item = Actor ? Cast<UUIItem>(Actor->GetRootComponent()) : nullptr;
		if (!Item)
		{
			UE_LOG(LogWuwaHudSetup, Error, TEXT("Cannot instantiate %s into HUD_Main"), *Name);
			++B.Ctx.Errors;
			return nullptr;
		}
		// 直接写整组锚点数据（SetAnchorMin 这类接口会按父节点当前大小换算位置，结果不直观）。
		FUIAnchorData Data;
		Data.Pivot = FVector2D(0.5f, 0.5f);
		Data.AnchorMin = Anchor;
		Data.AnchorMax = Anchor;
		Data.AnchoredPosition = Pos;
		Data.SizeDelta = FVector2D(Item->GetWidth(), Item->GetHeight());
		Item->SetAnchorData(Data);
		return Actor;
	}

	/** 截图里亮着红点的按钮：在 HUD_Main 里把红点打开，方便对照效果。 */
	void ShowRedDot(AActor* Button)
	{
		TArray<AActor*> Actors;
		if (Button) CollectHierarchy(Button, Actors);
		for (AActor* Actor : Actors)
		{
			UUITexture* Texture = Actor->FindComponentByClass<UUITexture>();
			if (Actor != Button && Texture && Texture->GetTexture() && Texture->GetTexture()->GetName() == TEXT("T_HUD_RedDot"))
			{
				Texture->SetIsUIActive(true);
			}
		}
	}

	UUIItem* BuildMain(FBuilder& B)
	{
		// 根节点铺满所在的层（1920x1080 参考分辨率）。
		UUIItem* Root = B.Container(nullptr, TEXT("HUD_Main"), FVector2D(1920, 1080));
		FUIAnchorData Stretch;
		Stretch.AnchorMin = FVector2D(0, 0);
		Stretch.AnchorMax = FVector2D(1, 1);
		Stretch.SizeDelta = FVector2D::ZeroVector;
		Root->SetAnchorData(Stretch);

		const FVector2D TopLeft(0, 1), TopRight(1, 1), Right(1, 0.5f), Bottom(0.5f, 0), BottomRight(1, 0), BottomLeft(0, 0), Center(0.5f, 0.5f);
		AddInstance(B, Root, TEXT("HUD_MiniMap"), TopLeft, FVector2D(170, -160));
		AddInstance(B, Root, TEXT("HUD_Btn_Terminal"), TopLeft, FVector2D(42, -50));
		AddInstance(B, Root, TEXT("HUD_Btn_Chess"), TopLeft, FVector2D(240, -50));
		ShowRedDot(AddInstance(B, Root, TEXT("HUD_Btn_Cards"), TopLeft, FVector2D(326, -50)));
		AddInstance(B, Root, TEXT("HUD_Btn_Social"), TopLeft, FVector2D(413, -50));
		ShowRedDot(AddInstance(B, Root, TEXT("HUD_Btn_Quest"), TopLeft, FVector2D(36, -232)));
		AddInstance(B, Root, TEXT("HUD_QuestTracker"), TopLeft, FVector2D(222, -328));

		ShowRedDot(AddInstance(B, Root, TEXT("HUD_Btn_Gacha"), TopRight, FVector2D(-516, -50)));
		ShowRedDot(AddInstance(B, Root, TEXT("HUD_Btn_Event"), TopRight, FVector2D(-432, -50)));
		AddInstance(B, Root, TEXT("HUD_Btn_Guide"), TopRight, FVector2D(-348, -52));
		ShowRedDot(AddInstance(B, Root, TEXT("HUD_Btn_Radio"), TopRight, FVector2D(-264, -50)));
		AddInstance(B, Root, TEXT("HUD_Btn_Resonator"), TopRight, FVector2D(-181, -52));
		ShowRedDot(AddInstance(B, Root, TEXT("HUD_Btn_Menu"), TopRight, FVector2D(-88, -50)));
		AddInstance(B, Root, TEXT("HUD_NetworkPing"), TopRight, FVector2D(-92, -95));

		for (int32 i = 0; i < 3; ++i)
		{
			if (AActor* Slot = AddInstance(B, Root, TEXT("HUD_TeamSlot"), Right, FVector2D(-120, 280 - i * 140)))
			{
				if (UUIText* Number = FindInHierarchy<UUIText>(Slot))
				{
					Number->SetText(FText::AsNumber(i + 1));
				}
			}
		}

		AddInstance(B, Root, TEXT("HUD_PlayerStatus"), Bottom, FVector2D(-10, 56));
		AddInstance(B, Root, TEXT("HUD_SkillBar"), BottomRight, FVector2D(-250, 68));
		AddInstance(B, Root, TEXT("HUD_Chat"), BottomLeft, FVector2D(56, 38));
		AddInstance(B, Root, TEXT("HUD_Subtitle"), Center, FVector2D(0, 146));
		return Root;
	}

	/** 重新加载 HUD_Main，确认每张图都有贴图。 */
	bool Verify(FContext& Ctx)
	{
		ULGUIPrefab* Prefab = LoadAsset<ULGUIPrefab>(PrefabFolder / TEXT("HUD_Main"));
		AActor* Root = Prefab ? Prefab->LoadPrefabInEditor(ULGUIPrefabManagerObject::GetPreviewWorldForPrefabPackage(), nullptr, true) : nullptr;
		if (!Root)
		{
			UE_LOG(LogWuwaHudSetup, Error, TEXT("HUD_Main cannot be loaded."));
			return false;
		}
		TArray<AActor*> Actors;
		CollectHierarchy(Root, Actors);
		TArray<AActor*> TopLevel;
		Root->GetAttachedActors(TopLevel);
		for (AActor* Child : TopLevel)
		{
			if (const UUIItem* Item = Cast<UUIItem>(Child->GetRootComponent()))
			{
				UE_LOG(LogWuwaHudSetup, Verbose, TEXT("  %s anchor min=%s max=%s pos=%s size=%.0fx%.0f"), *Child->GetActorLabel(),
					*Item->GetAnchorMin().ToString(), *Item->GetAnchorMax().ToString(), *Item->GetAnchoredPosition().ToString(), Item->GetWidth(), Item->GetHeight());
			}
		}
		int32 Images = 0, Missing = 0, BadUV = 0, Texts = 0, Buttons = 0, ButtonsWithTransition = 0;
		for (AActor* Actor : Actors)
		{
			if (const UUITexture* Texture = Actor->FindComponentByClass<UUITexture>())
			{
				++Images;
				Missing += Texture->GetTexture() ? 0 : 1;
				const FLGUISpriteInfo Info = Texture->GetSpriteData();
				BadUV += (FMath::IsFinite(Info.uv0X) && FMath::IsFinite(Info.uv3Y) && Info.width > 0) ? 0 : 1;
			}
			Texts += Actor->FindComponentByClass<UUIText>() ? 1 : 0;
			if (const UUIButtonComponent* Button = Actor->FindComponentByClass<UUIButtonComponent>())
			{
				++Buttons;
				ButtonsWithTransition += Button->GetTransitionTarget() ? 1 : 0;
			}
		}
		ULGUIBPLibrary::DestroyActorWithHierarchy(Root, true);
		UE_LOG(LogWuwaHudSetup, Display, TEXT("HUD_Main: %d actors, %d images (%d without texture, %d with bad UV), %d texts, %d buttons (%d with hover/press transition)."),
			Actors.Num(), Images, Missing, BadUV, Texts, Buttons, ButtonsWithTransition);
		return Missing == 0 && BadUV == 0 && Buttons == ButtonsWithTransition;
	}
}
#endif

int32 UWuwaHudAssetSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaHudSetup;
	FContext Ctx;
	Ctx.bApply = FParse::Param(*Params, TEXT("Apply"));
	Ctx.bRebuild = FParse::Param(*Params, TEXT("Rebuild"));

	// 1. 贴图
	const FString SourceDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("ArtSource/UI/HUD/Textures"));
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(SourceDir / TEXT("*.png")), true, false);
	if (Files.IsEmpty())
	{
		UE_LOG(LogWuwaHudSetup, Error, TEXT("No PNG in %s. Run ArtSource/UI/HUD/extract_hud_sprites.py first."), *SourceDir);
		return 1;
	}
	for (const FString& File : Files)
	{
		if (UTexture2D* Texture = ImportTexture(Ctx, SourceDir / File))
		{
			Ctx.Textures.Add(FPaths::GetBaseFilename(File), Texture);
		}
	}
	if (!Ctx.bApply)
	{
		UE_LOG(LogWuwaHudSetup, Display, TEXT("Dry run: %d source PNGs. Pass -Apply to import and build prefabs."), Files.Num());
		return 0;
	}

	// 贴图编译完，GetSurfaceWidth 才是实际尺寸：LGUI 在编辑器里（实例化、保存预制体时）会按它重算 UITexture 的 UV，
	// 编译完之前是 0，算出来就是 NaN，运行时整张图只采样到一个点。
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
			UE_LOG(LogWuwaHudSetup, Error, TEXT("Texture sizes are unavailable (NullRHI). Run with -AllowCommandletRendering -RenderOffscreen."));
			return 1;
		}
	}

	// 2. 单个元素的预制体
	const TPair<const TCHAR*, const TCHAR*> Buttons[] = {
		{TEXT("Terminal"), TEXT("T_HUD_Icon_Terminal")}, {TEXT("Chess"), TEXT("T_HUD_Icon_Chess")}, {TEXT("Cards"), TEXT("T_HUD_Icon_Cards")},
		{TEXT("Social"), TEXT("T_HUD_Icon_Social")}, {TEXT("Quest"), TEXT("T_HUD_Icon_Quest")}, {TEXT("Gacha"), TEXT("T_HUD_Icon_Gacha")},
		{TEXT("Event"), TEXT("T_HUD_Icon_Event")}, {TEXT("Guide"), TEXT("T_HUD_Icon_Guide")}, {TEXT("Radio"), TEXT("T_HUD_Icon_Radio")},
		{TEXT("Resonator"), TEXT("T_HUD_Icon_Resonator")}, {TEXT("Menu"), TEXT("T_HUD_Icon_Menu")},
	};
	bool bOk = true;
	for (const TPair<const TCHAR*, const TCHAR*>& Button : Buttons)
	{
		const FString Name = FString(TEXT("HUD_Btn_")) + Button.Key;
		const FString Icon = Button.Value;
		bOk &= WritePrefab(Ctx, Name, [&](FBuilder& B) { return BuildFuncButton(B, Name, Icon); });
	}
	bOk &= WritePrefab(Ctx, TEXT("HUD_MiniMap"), [](FBuilder& B) { return BuildMiniMap(B); });
	bOk &= WritePrefab(Ctx, TEXT("HUD_QuestTracker"), [](FBuilder& B) { return BuildQuestTracker(B); });
	bOk &= WritePrefab(Ctx, TEXT("HUD_NetworkPing"), [](FBuilder& B) { return BuildNetworkPing(B); });
	bOk &= WritePrefab(Ctx, TEXT("HUD_TeamSlot"), [](FBuilder& B) { return BuildTeamSlot(B); });
	bOk &= WritePrefab(Ctx, TEXT("HUD_PlayerStatus"), [](FBuilder& B) { return BuildPlayerStatus(B); });
	bOk &= WritePrefab(Ctx, TEXT("HUD_SkillButton"), [](FBuilder& B)
	{
		return BuildSkillButton(B, nullptr, TEXT("HUD_SkillButton"), TEXT("E"), TEXT("T_HUD_Skill_Changli_01"), FVector2D::ZeroVector);
	});
	bOk &= WritePrefab(Ctx, TEXT("HUD_SkillBar"), [](FBuilder& B) { return BuildSkillBar(B); });
	bOk &= WritePrefab(Ctx, TEXT("HUD_Chat"), [](FBuilder& B) { return BuildChat(B); });
	bOk &= WritePrefab(Ctx, TEXT("HUD_Subtitle"), [](FBuilder& B) { return BuildSubtitle(B); });

	// 3. 整体布局
	bOk &= WritePrefab(Ctx, TEXT("HUD_Main"), [](FBuilder& B) { return BuildMain(B); });
	bOk &= Verify(Ctx);

	UE_LOG(LogWuwaHudSetup, Display, TEXT("Done: %d textures written, %d prefabs written, %d errors.%s"),
		Ctx.TexturesWritten, Ctx.PrefabsWritten, Ctx.Errors, Ctx.BackupDir.IsEmpty() ? TEXT("") : *(TEXT(" Backups in ") + Ctx.BackupDir));
	return bOk && Ctx.Errors == 0 ? 0 : 1;
#else
	return 1;
#endif
}
