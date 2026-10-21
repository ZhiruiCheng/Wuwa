#include "Editor/Commandlets/WuwaKuroHudBuildCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Core/Actor/UIBaseActor.h"
#include "Core/Actor/UIContainerActor.h"
#include "Core/Actor/UISpriteActor.h"
#include "Core/Actor/UITextActor.h"
#include "Core/Actor/UITextureActor.h"
#include "Core/ActorComponent/UICanvasGroup.h"
#include "Core/ActorComponent/LGUICanvas.h"
#include "Curves/CurveFloat.h"
#include "Game/LGUI/Component/WuwaUiSlicedFill.h"
#include "Game/LGUI/Component/WuwaUiTweenGroup.h"
#include "GeometryModifier/UIEffectTextAnimation.h"
#include "Core/ActorComponent/UIItem.h"
#include "Core/ActorComponent/UISprite.h"
#include "Core/LGUISpriteData.h"
#include "Core/ActorComponent/UIText.h"
#include "Core/ActorComponent/UITexture.h"
#include "Core/LGUISDFFontData.h"
#include "Core/UIAnchorData.h"
#include "Dom/JsonObject.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Game/LGUI/Component/WuwaUINiagara.h"
#include "Game/LGUI/Component/WuwaUiComponentsRegistry.h"
#include "NiagaraSystem.h"
#include "Game/LGUI/Core/WuwaUISettings.h"
#include "GeometryModifier/UIEffectOutline.h"
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
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TextureCompiler.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaKuroHud, Log, All);

UWuwaKuroHudBuildCommandlet::UWuwaKuroHudBuildCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpUsage = TEXT("-run=WuwaKuroHudBuild -Layout=\"<abs layout.json>\" -Prefab=/Game/LGUI/HUD/HUD_Main [-Font=\"<ttf>\" -FontAsset=/Game/LGUI/Fonts/<Name>] [-AddView=<Name> -Layer=<Layer>] [-Apply] -AllowCommandletRendering -RenderOffscreen");
}

#if WITH_EDITOR
namespace WuwaKuroHud
{
	const TCHAR* ViewTablePackage = TEXT("/Game/LGUI/DT_UiViews");

	struct FContext
	{
		bool bApply = false;
		FString BackupDir;
		FString TextureFolder;
		FString SpriteFolder;
		TMap<FString, UTexture2D*> Textures;   // PNG 相对路径 → 贴图
		TMap<UTexture2D*, ULGUISpriteData*> Sprites;
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
			UE_LOG(LogWuwaKuroHud, Error, TEXT("Failed to save %s"), *Filename);
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
			Ctx.BackupDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/KuroHudBuild") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
		}
		if (IFileManager::Get().Copy(*(Ctx.BackupDir / FPaths::GetCleanFilename(Filename)), *Filename) != COPY_OK)
		{
			UE_LOG(LogWuwaKuroHud, Error, TEXT("Failed to back up %s"), *Filename);
			return false;
		}
		return true;
	}

	/** PNG → 贴图（UI 用：不压缩、不生成 mip，sRGB）；同名的直接覆盖源数据。 */
	UTexture2D* ImportTexture(FContext& Ctx, const FString& RelativeFile)
	{
		const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RelativeFile);
		const FString PackageName = Ctx.TextureFolder / FPaths::GetBaseFilename(File);
		IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> Png = Images.CreateImageWrapper(EImageFormat::PNG);
		TArray<uint8> Compressed, Pixels;
		if (!FFileHelper::LoadFileToArray(Compressed, *File) || !Png->SetCompressed(Compressed.GetData(), Compressed.Num())
			|| !Png->GetRaw(ERGBFormat::BGRA, 8, Pixels))
		{
			UE_LOG(LogWuwaKuroHud, Error, TEXT("Cannot read %s"), *File);
			++Ctx.Errors;
			return nullptr;
		}
		UTexture2D* Texture = LoadAsset<UTexture2D>(PackageName);
		if (!Ctx.bApply) return Texture;
		if (!Texture)
		{
			UPackage* Package = CreatePackage(*PackageName);
			Texture = NewObject<UTexture2D>(Package, *FPaths::GetBaseFilename(File), RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Texture);
		}
		Texture->PreEditChange(nullptr);
		Texture->Source.Init(Png->GetWidth(), Png->GetHeight(), 1, 1, TSF_BGRA8, Pixels.GetData());
		Texture->CompressionSettings = TC_EditorIcon;
		Texture->MipGenSettings = TMGS_NoMipmaps;
		Texture->LODGroup = TEXTUREGROUP_UI;
		Texture->SRGB = true;
		Texture->NeverStream = true;
		if (!Texture->AssetImportData) Texture->AssetImportData = NewObject<UAssetImportData>(Texture);
		Texture->AssetImportData->Update(File);
		Texture->PostEditChange();
		return SaveAsset(Texture) ? Texture : nullptr;
	}

	FVector2D Vec2(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		const TArray<TSharedPtr<FJsonValue>>& V = Object->GetArrayField(Field);
		return FVector2D(V[0]->AsNumber(), V[1]->AsNumber());
	}

	FColor Color(const TArray<TSharedPtr<FJsonValue>>& V)
	{
		return FColor(V[0]->AsNumber(), V[1]->AsNumber(), V[2]->AsNumber(), V[3]->AsNumber());
	}

	/** UITexture 的 UV（和九宫格边距）按贴图源尺寸写进 spriteData：NullRHI 或贴图没编译完时 LGUI 自己算会得到 NaN。 */
	void ApplySpriteData(UUITexture* Item, const UTexture2D* Texture, const TArray<TSharedPtr<FJsonValue>>* Slice)
	{
		const int32 Width = Texture->Source.GetSizeX(), Height = Texture->Source.GetSizeY();
		if (Width <= 0 || Height <= 0) return;
		FLGUISpriteInfo Data;
		Data.width = Width;
		Data.height = Height;
		if (Slice && Slice->Num() == 4)
		{
			Data.borderLeft = (*Slice)[0]->AsNumber();
			Data.borderRight = (*Slice)[1]->AsNumber();
			Data.borderTop = (*Slice)[2]->AsNumber();
			Data.borderBottom = (*Slice)[3]->AsNumber();
		}
		Data.ApplyUV(0, 0, Width, Height, 1.f / Width, 1.f / Height, FVector4(0, 0, 1, 1));
		Data.ApplyBorderUV(1.f / Width, 1.f / Height);
		Item->SetSpriteData(Data);
	}

	/**
	 * 填充型节点要用 UISprite（UITexture 不能按比例填充），需要一个 LGUI 精灵资产：从同一张贴图生成，存在 <预制体目录>/Kuro/Sprites。
	 * 已有就复用（贴图是原地覆盖导入的）。
	 */
	ULGUISpriteData* GetOrCreateSprite(FContext& Ctx, UTexture2D* Texture)
	{
		if (ULGUISpriteData* const* Found = Ctx.Sprites.Find(Texture)) return *Found;
		const FString Name = Texture->GetName();   // 和贴图同名，在另一个目录
		const FString PackageName = Ctx.SpriteFolder / Name;
		ULGUISpriteData* Sprite = LoadAsset<ULGUISpriteData>(PackageName);
		if (!Sprite)
		{
			UPackage* Package = CreatePackage(*PackageName);
			Sprite = ULGUISpriteData::CreateLGUISpriteData(Package, Texture);
			if (Sprite)
			{
				Sprite->Rename(*Name, Package, REN_DontCreateRedirectors | REN_NonTransactional);
				Sprite->SetFlags(RF_Public | RF_Standalone);
				FAssetRegistryModule::AssetCreated(Sprite);
				if (!SaveAsset(Sprite)) ++Ctx.Errors;
				// CreateLGUISpriteData 会改贴图的设置（精灵用的压缩、过滤），一起存下来
				SaveAsset(Texture);
			}
		}
		if (!Sprite) ++Ctx.Errors;
		Ctx.Sprites.Add(Texture, Sprite);
		return Sprite;
	}

	EUISpriteFillMethod FillMethod(const FString& Value)
	{
		const int64 Index = StaticEnum<EUISpriteFillMethod>()->GetValueByNameString(Value);
		return Index == INDEX_NONE ? EUISpriteFillMethod::Horizontal : static_cast<EUISpriteFillMethod>(Index);
	}

	EUITextParagraphHorizontalAlign HAlign(const FString& Value)
	{
		return Value == TEXT("Left") ? EUITextParagraphHorizontalAlign::Left
			: Value == TEXT("Right") ? EUITextParagraphHorizontalAlign::Right : EUITextParagraphHorizontalAlign::Center;
	}

	EUITextParagraphVerticalAlign VAlign(const FString& Value)
	{
		return Value == TEXT("Top") ? EUITextParagraphVerticalAlign::Top
			: Value == TEXT("Bottom") ? EUITextParagraphVerticalAlign::Bottom : EUITextParagraphVerticalAlign::Middle;
	}

	/** 根节点接收点击；颜色过渡作用在下面第一个图标上（和旧 HUD 一样：平时略暗、悬停变亮、按下变暗）。 */
	void MakeButton(UUIItem* Root, UUIItem* TransitionTarget)
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
			Button->SetNormalColor(FColor(235, 235, 235));
			Button->SetHighlightedColor(FColor::White);
			Button->SetPressedColor(FColor(170, 170, 170));
			Button->SetDisabledColor(FColor(140, 140, 140, 140));
		}
	}

	FVector4 Vec4(const TArray<TSharedPtr<FJsonValue>>& V)
	{
		FVector4 Out(0, 0, 0, 0);
		for (int32 Index = 0; Index < FMath::Min(4, V.Num()); ++Index) Out[Index] = V[Index]->AsNumber();
		return Out;
	}

	template <typename TEnum>
	TEnum EnumByName(const FString& Name, TEnum Default)
	{
		const int64 Value = StaticEnum<TEnum>()->GetValueByNameString(Name);
		return Value == INDEX_NONE ? Default : static_cast<TEnum>(Value);
	}

	/** 原作曲线（关键帧）→ CurveFloat 资产：路径同导出数据，已有的原地覆盖（先备份）。 */
	UCurveFloat* BuildCurve(FContext& Ctx, const TSharedPtr<FJsonObject>& Def)
	{
		const FString PackageName = Def->GetStringField(TEXT("path"));
		UCurveFloat* Curve = LoadAsset<UCurveFloat>(PackageName);
		if (Curve)
		{
			if (!Backup(Ctx, PackageName)) return nullptr;
		}
		else
		{
			UPackage* Package = CreatePackage(*PackageName);
			Curve = NewObject<UCurveFloat>(Package, *FPackageName::GetShortName(PackageName), RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Curve);
		}
		FRichCurve& Rich = Curve->FloatCurve;
		Rich.Reset();
		for (const TSharedPtr<FJsonValue>& Value : Def->GetArrayField(TEXT("keys")))
		{
			const TSharedPtr<FJsonObject> Key = Value->AsObject();
			const FKeyHandle Handle = Rich.AddKey(Key->GetNumberField(TEXT("time")), Key->GetNumberField(TEXT("value")));
			FRichCurveKey& Data = Rich.GetKey(Handle);
			Data.InterpMode = EnumByName<ERichCurveInterpMode>(Key->GetStringField(TEXT("interp")), RCIM_Linear);
			Data.TangentMode = EnumByName<ERichCurveTangentMode>(Key->GetStringField(TEXT("tangentMode")), RCTM_Auto);
			Data.ArriveTangent = Key->GetNumberField(TEXT("arrive"));
			Data.LeaveTangent = Key->GetNumberField(TEXT("leave"));
		}
		Rich.PreInfinityExtrap = EnumByName<ERichCurveExtrapolation>(Def->GetStringField(TEXT("preInfinity")), RCCE_Constant);
		Rich.PostInfinityExtrap = EnumByName<ERichCurveExtrapolation>(Def->GetStringField(TEXT("postInfinity")), RCCE_Constant);
		if (!SaveAsset(Curve)) ++Ctx.Errors;
		return Curve;
	}

	/** 按导出的字段名写对象属性（LGUI3 的选择器 / 逐字属性字段是 private，用反射）。 */
	bool SetByReflection(FContext& Ctx, UObject* Object, const FString& Name, const TSharedPtr<FJsonValue>& Value, const TMap<FString, UCurveFloat*>& Curves)
	{
		FProperty* Property = FindFProperty<FProperty>(Object->GetClass(), *Name);
		if (!Property)
		{
			UE_LOG(LogWuwaKuroHud, Error, TEXT("%s has no property %s"), *Object->GetClass()->GetName(), *Name);
			++Ctx.Errors;
			return false;
		}
		FString Text;
		if (Value->Type == EJson::Array)
		{
			const TArray<TSharedPtr<FJsonValue>>& V = Value->AsArray();
			Text = V.Num() == 4 ? FString::Printf(TEXT("(R=%d,G=%d,B=%d,A=%d)"), static_cast<int32>(V[0]->AsNumber()), static_cast<int32>(V[1]->AsNumber()),
				static_cast<int32>(V[2]->AsNumber()), static_cast<int32>(V[3]->AsNumber()))
				: FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), V[0]->AsNumber(), V.Num() > 1 ? V[1]->AsNumber() : 0.0, V.Num() > 2 ? V[2]->AsNumber() : 0.0);
		}
		else if (Value->Type == EJson::Boolean) Text = Value->AsBool() ? TEXT("True") : TEXT("False");
		else if (Value->Type == EJson::Number) Text = FString::SanitizeFloat(Value->AsNumber());
		else
		{
			Text = Value->AsString();
			if (UCurveFloat* Curve = Curves.FindRef(Text)) Text = Curve->GetPathName();
		}
		if (!Property->ImportText_InContainer(*Text, Object, Object, PPF_None))
		{
			UE_LOG(LogWuwaKuroHud, Error, TEXT("Cannot set %s.%s = %s"), *Object->GetClass()->GetName(), *Name, *Text);
			++Ctx.Errors;
			return false;
		}
		return true;
	}

	/** 原作逐字动画（UIEffectTextAnimation：选择器 + 属性）→ 同名的 LGUI3 组件，组件名保留（补间按名字找）。 */
	void AddTextAnimations(FContext& Ctx, AActor* Actor, const TArray<TSharedPtr<FJsonValue>>& Defs, const TMap<FString, UCurveFloat*>& Curves)
	{
		for (const TSharedPtr<FJsonValue>& Value : Defs)
		{
			const TSharedPtr<FJsonObject> Def = Value->AsObject();
			UUIEffectTextAnimation* Animation = NewObject<UUIEffectTextAnimation>(Actor, *Def->GetStringField(TEXT("name")), RF_Transactional);
			Actor->AddInstanceComponent(Animation);
			const TSharedPtr<FJsonObject> SelectorDef = Def->GetObjectField(TEXT("selector"));
			UClass* SelectorClass = FindObject<UClass>(nullptr, *(TEXT("/Script/LGUI.") + SelectorDef->GetStringField(TEXT("class"))));
			if (!SelectorClass)
			{
				UE_LOG(LogWuwaKuroHud, Error, TEXT("Unknown text animation selector %s"), *SelectorDef->GetStringField(TEXT("class")));
				++Ctx.Errors;
				continue;
			}
			UObject* Selector = NewObject<UObject>(Animation, SelectorClass, NAME_None, RF_Transactional);
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : SelectorDef->Values)
			{
				if (Field.Key != TEXT("class")) SetByReflection(Ctx, Selector, Field.Key, Field.Value, Curves);
			}
			CastFieldChecked<FObjectProperty>(FindFProperty<FProperty>(UUIEffectTextAnimation::StaticClass(), TEXT("selector")))->SetObjectPropertyValue_InContainer(Animation, Selector);
			FArrayProperty* PropertiesArray = CastFieldChecked<FArrayProperty>(FindFProperty<FProperty>(UUIEffectTextAnimation::StaticClass(), TEXT("properties")));
			FScriptArrayHelper Helper(PropertiesArray, PropertiesArray->ContainerPtrToValuePtr<void>(Animation));
			for (const TSharedPtr<FJsonValue>& PropertyValue : Def->GetArrayField(TEXT("properties")))
			{
				const TSharedPtr<FJsonObject> PropertyDef = PropertyValue->AsObject();
				UClass* PropertyClass = FindObject<UClass>(nullptr, *(TEXT("/Script/LGUI.") + PropertyDef->GetStringField(TEXT("class"))));
				if (!PropertyClass)
				{
					UE_LOG(LogWuwaKuroHud, Error, TEXT("Unknown text animation property %s"), *PropertyDef->GetStringField(TEXT("class")));
					++Ctx.Errors;
					continue;
				}
				UObject* Property = NewObject<UObject>(Animation, PropertyClass, NAME_None, RF_Transactional);
				for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : PropertyDef->Values)
				{
					if (Field.Key != TEXT("class")) SetByReflection(Ctx, Property, Field.Key, Field.Value, Curves);
				}
				const int32 Added = Helper.AddValue();
				CastFieldChecked<FObjectProperty>(PropertiesArray->Inner)->SetObjectPropertyValue(Helper.GetRawPtr(Added), Property);
			}
			Animation->RegisterComponent();
			Animation->SetSelectorOffset(SelectorDef->GetNumberField(TEXT("offset")));
		}
	}

	FWuwaUiTweenEvent MakeTweenEvent(const TSharedPtr<FJsonObject>& Def, const TArray<UUIItem*>& Items)
	{
		FWuwaUiTweenEvent Event;
		const int32 Target = Def->GetIntegerField(TEXT("target"));
		Event.Target = Items.IsValidIndex(Target) ? Items[Target]->GetOwner() : nullptr;
		Event.Component = FName(*Def->GetStringField(TEXT("component")));
		Event.Action = EnumByName<EWuwaUiTweenAction>(Def->GetStringField(TEXT("action")), EWuwaUiTweenAction::Alpha);
		Event.bUseTweenValue = Def->GetBoolField(TEXT("useTweenValue"));
		Event.FixedValue = Vec4(Def->GetArrayField(TEXT("fixed")));
		Event.ValueScale = Def->GetNumberField(TEXT("valueScale"));
		// 补间改容器（不是可渲染组件）的透明度：LGUI3 要靠 CanvasGroup（原作旧版 LGUI 的容器自带透明度）。没有就补一个（透明度 1）
		AActor* TargetActor = Event.Target.Get();
		if (Event.Action == EWuwaUiTweenAction::Alpha && TargetActor && !Cast<UUIBaseRenderable>(TargetActor->GetRootComponent())
			&& !TargetActor->FindComponentByClass<UUICanvasGroup>())
		{
			UUICanvasGroup* Group = NewObject<UUICanvasGroup>(TargetActor, TEXT("UICanvasGroup"), RF_Transactional);
			TargetActor->AddInstanceComponent(Group);
			Group->RegisterComponent();
		}
		return Event;
	}

	/** 原作动画节点上的补间（LGUIPlayTweenComponent）→ 一个 UWuwaUiTweenGroup。 */
	void AddTweenGroup(AActor* Actor, const TArray<TSharedPtr<FJsonValue>>& Defs, const TArray<UUIItem*>& Items, const TMap<FString, UCurveFloat*>& Curves)
	{
		UWuwaUiTweenGroup* Group = NewObject<UWuwaUiTweenGroup>(Actor, TEXT("WuwaUiTweenGroup"), RF_Transactional);
		Actor->AddInstanceComponent(Group);
		Group->RegisterComponent();
		for (const TSharedPtr<FJsonValue>& Value : Defs)
		{
			const TSharedPtr<FJsonObject> Def = Value->AsObject();
			FWuwaUiTween& Tween = Group->Tweens.AddDefaulted_GetRef();
			Tween.Kind = EnumByName<EWuwaUiTweenKind>(Def->GetStringField(TEXT("kind")), EWuwaUiTweenKind::Float);
			Tween.From = Vec4(Def->GetArrayField(TEXT("from")));
			Tween.To = Vec4(Def->GetArrayField(TEXT("to")));
			Tween.Ease = EnumByName<ELTweenEase>(Def->GetStringField(TEXT("easeType")), ELTweenEase::Linear);
			FString CurvePath;
			if (Def->TryGetStringField(TEXT("easeCurve"), CurvePath)) Tween.Curve = Curves.FindRef(CurvePath);
			Tween.Duration = Def->GetNumberField(TEXT("duration"));
			Tween.StartDelay = Def->GetNumberField(TEXT("startDelay"));
			Tween.Loop = EnumByName<ELTweenLoop>(Def->GetStringField(TEXT("loopType")), ELTweenLoop::Once);
			Tween.LoopCount = Def->GetIntegerField(TEXT("loopCount"));
			for (const TSharedPtr<FJsonValue>& E : Def->GetArrayField(TEXT("onUpdate"))) Tween.OnUpdate.Add(MakeTweenEvent(E->AsObject(), Items));
			for (const TSharedPtr<FJsonValue>& E : Def->GetArrayField(TEXT("onStart"))) Tween.OnStart.Add(MakeTweenEvent(E->AsObject(), Items));
			for (const TSharedPtr<FJsonValue>& E : Def->GetArrayField(TEXT("onComplete"))) Tween.OnComplete.Add(MakeTweenEvent(E->AsObject(), Items));
		}
	}

	void AddClipCanvas(AActor* Actor)
	{
		ULGUICanvas* Canvas = NewObject<ULGUICanvas>(Actor, TEXT("LGUICanvas"), RF_Transactional);
		Actor->AddInstanceComponent(Canvas);
		Canvas->RegisterComponent();
		Canvas->SetClipType(ELGUICanvasClipType::Rect);
		// 嵌套画布默认沿用父画布的裁剪方式；要自己裁剪必须勾 overrideParameters 里的 ClipType（原作数据里也是 0x08）
		if (FProperty* Override = FindFProperty<FProperty>(ULGUICanvas::StaticClass(), TEXT("overrideParameters")))
		{
			int8* Flags = Override->ContainerPtrToValuePtr<int8>(Canvas);
			*Flags |= static_cast<int8>(1 << static_cast<int32>(ELGUICanvasOverrideParameters::ClipType));
		}
	}

	void AddRegistry(AActor* Owner, const TArray<AActor*>& Entries)
	{
		UWuwaUiComponentsRegistry* Registry = NewObject<UWuwaUiComponentsRegistry>(Owner, TEXT("WuwaUiComponentsRegistry"), RF_Transactional);
		Owner->AddInstanceComponent(Registry);
		Registry->RegisterComponent();
		Registry->SetComponents(Entries);
	}

	/**
	 * 原作字体（TTF）→ LGUI 的 SDF 字体资产：复制 LGUI 默认的 SDF 字体（保留 SDF 参数），换成自定义字体文件并嵌进资产
	 * （CustomFontFile + 不用外部文件：编辑器加载时把文件读进 fontBinaryArray，随资产保存）。字体属性是 protected，用反射设。
	 */
	ULGUIFontData_BaseObject* BuildFont(FContext& Ctx, const FString& FontFile, const FString& FontPackage)
	{
		ULGUISDFFontData* Font = LoadAsset<ULGUISDFFontData>(FontPackage);
		if (!Font)
		{
			ULGUISDFFontData* Template = LoadObject<ULGUISDFFontData>(nullptr, TEXT("/LGUI/DefaultSDFFont.DefaultSDFFont"));
			if (!Template)
			{
				UE_LOG(LogWuwaKuroHud, Error, TEXT("Missing /LGUI/DefaultSDFFont"));
				++Ctx.Errors;
				return nullptr;
			}
			UPackage* Package = CreatePackage(*FontPackage);
			Font = DuplicateObject<ULGUISDFFontData>(Template, Package, *FPackageName::GetShortName(FontPackage));
			Font->SetFlags(RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(Font);
		}
		auto SetEnum = [Font](const TCHAR* Name, int64 Value)
		{
			if (FEnumProperty* P = FindFProperty<FEnumProperty>(ULGUIFreeTypeRenderFontData::StaticClass(), Name))
				P->GetUnderlyingProperty()->SetIntPropertyValue(P->ContainerPtrToValuePtr<void>(Font), Value);
		};
		auto SetBool = [Font](const TCHAR* Name, bool Value)
		{
			if (FBoolProperty* P = FindFProperty<FBoolProperty>(ULGUIFreeTypeRenderFontData::StaticClass(), Name)) P->SetPropertyValue_InContainer(Font, Value);
		};
		SetEnum(TEXT("fontType"), static_cast<int64>(ELGUIDynamicFontDataType::CustomFontFile));
		if (FStrProperty* P = FindFProperty<FStrProperty>(ULGUIFreeTypeRenderFontData::StaticClass(), TEXT("fontFilePath")))
		{
			P->SetPropertyValue_InContainer(Font, FontFile);
		}
		SetBool(TEXT("useRelativeFilePath"), FPaths::IsRelative(FontFile));
		SetBool(TEXT("useExternalFileOrEmbedInToUAsset"), false);
		Font->ReloadFont();
		Font->MarkPackageDirty();
		if (!SaveAsset(Font)) return nullptr;
		UE_LOG(LogWuwaKuroHud, Display, TEXT("Font %s <- %s"), *FontPackage, *FontFile);
		return Font;
	}

	/** DT_UiViews 里加 / 覆盖一行：视图类用默认的 UWuwaUiView，其他字段默认值。 */
	bool AddViewRow(FContext& Ctx, const FString& ViewName, const FString& LayerName, const FString& PrefabPackage)
	{
		UDataTable* Table = LoadAsset<UDataTable>(ViewTablePackage);
		const UEnum* LayerEnum = StaticEnum<EWuwaUiLayer>();
		const int64 Layer = LayerEnum->GetValueByNameString(LayerName);
		if (!Table || Table->GetRowStruct() != FWuwaUiViewConfig::StaticStruct() || Layer == INDEX_NONE)
		{
			UE_LOG(LogWuwaKuroHud, Error, TEXT("Cannot add view %s: table %s missing or layer '%s' unknown."), *ViewName, ViewTablePackage, *LayerName);
			++Ctx.Errors;
			return false;
		}
		UE_LOG(LogWuwaKuroHud, Display, TEXT("View table: will %s row %s (layer %s)."), Table->FindRowUnchecked(*ViewName) ? TEXT("overwrite") : TEXT("add"), *ViewName, *LayerName);
		if (!Ctx.bApply) return true;
		if (!Backup(Ctx, ViewTablePackage)) return false;
		FWuwaUiViewConfig Row;
		Row.Prefab = TSoftObjectPtr<ULGUIPrefab>(FSoftObjectPath(PrefabPackage + TEXT(".") + FPackageName::GetShortName(PrefabPackage)));
		Row.Layer = static_cast<EWuwaUiLayer>(Layer);
		Table->AddRow(*ViewName, Row);
		return SaveAsset(Table);
	}
}
#endif

int32 UWuwaKuroHudBuildCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaKuroHud;
	FString LayoutPath, PrefabPackage;
	if (!FParse::Value(*Params, TEXT("Layout="), LayoutPath, false) || !FParse::Value(*Params, TEXT("Prefab="), PrefabPackage))
	{
		UE_LOG(LogWuwaKuroHud, Error, TEXT("Usage: %s"), *HelpUsage);
		return 1;
	}
	FContext Ctx;
	Ctx.bApply = FParse::Param(*Params, TEXT("Apply"));
	Ctx.TextureFolder = FPackageName::GetLongPackagePath(PrefabPackage) / TEXT("Kuro/Textures");
	Ctx.SpriteFolder = FPackageName::GetLongPackagePath(PrefabPackage) / TEXT("Kuro/Sprites");
	FString Text;
	TSharedPtr<FJsonObject> Layout;
	const FString LayoutFile = FPaths::IsRelative(LayoutPath) ? FPaths::ProjectDir() / LayoutPath : LayoutPath;
	if (!FFileHelper::LoadFileToString(Text, *LayoutFile) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Layout) || !Layout)
	{
		UE_LOG(LogWuwaKuroHud, Error, TEXT("Cannot read layout %s"), *LayoutFile);
		return 1;
	}
	const TArray<TSharedPtr<FJsonValue>>& Nodes = Layout->GetArrayField(TEXT("nodes"));

	// 1. 贴图
	for (const TSharedPtr<FJsonValue>& Value : Nodes)
	{
		FString File;
		if (Value->AsObject()->TryGetStringField(TEXT("texture"), File) && !Ctx.Textures.Contains(File))
		{
			Ctx.Textures.Add(File, ImportTexture(Ctx, File));
		}
	}
	UE_LOG(LogWuwaKuroHud, Display, TEXT("%s: %d nodes, %d textures -> %s"), *LayoutPath, Nodes.Num(), Ctx.Textures.Num(), *Ctx.TextureFolder);
	FString AddView, LayerName;
	const bool bAddView = FParse::Value(*Params, TEXT("AddView="), AddView) && FParse::Value(*Params, TEXT("Layer="), LayerName);
	if (!Ctx.bApply)
	{
		if (bAddView) AddViewRow(Ctx, AddView, LayerName, PrefabPackage);
		UE_LOG(LogWuwaKuroHud, Display, TEXT("Dry run; pass -Apply to build %s."), *PrefabPackage);
		return Ctx.Errors ? 1 : 0;
	}
	{
		TArray<UTexture*> ToCompile;
		for (const TPair<FString, UTexture2D*>& Pair : Ctx.Textures) if (Pair.Value) ToCompile.Add(Pair.Value);
		FTextureCompilingManager::Get().FinishCompilation(ToCompile);
		if (!ToCompile.IsEmpty() && ToCompile[0]->GetSurfaceWidth() <= 0.f)
		{
			UE_LOG(LogWuwaKuroHud, Error, TEXT("Texture sizes are unavailable (NullRHI?). Run with -AllowCommandletRendering -RenderOffscreen."));
			return 1;
		}
	}

	// 字体（可选）：-Font=<TTF，相对项目目录或绝对路径> -FontAsset=/Game/...
	ULGUIFontData_BaseObject* Font = nullptr;
	FString FontFile, FontPackage;
	if (FParse::Value(*Params, TEXT("Font="), FontFile, false) && FParse::Value(*Params, TEXT("FontAsset="), FontPackage))
	{
		Font = BuildFont(Ctx, FontFile, FontPackage);
		if (!Font) return 1;
	}

	// 已有的字体资产（不重新生成）：-UseFontAsset=/Game/...
	FString UseFontPackage;
	if (!Font && FParse::Value(*Params, TEXT("UseFontAsset="), UseFontPackage))
	{
		Font = LoadAsset<ULGUIFontData_BaseObject>(UseFontPackage);
		if (!Font)
		{
			UE_LOG(LogWuwaKuroHud, Error, TEXT("Font asset %s not found."), *UseFontPackage);
			return 1;
		}
	}

	// 曲线（补间 / 逐字动画的缓动曲线）
	TMap<FString, UCurveFloat*> Curves;
	const TArray<TSharedPtr<FJsonValue>>* CurveDefs = nullptr;
	if (Layout->TryGetArrayField(TEXT("curves"), CurveDefs))
	{
		for (const TSharedPtr<FJsonValue>& Value : *CurveDefs)
		{
			if (UCurveFloat* Curve = BuildCurve(Ctx, Value->AsObject())) Curves.Add(Value->AsObject()->GetStringField(TEXT("path")), Curve);
		}
		UE_LOG(LogWuwaKuroHud, Display, TEXT("%d curves"), Curves.Num());
	}

	// 2. 节点（父节点总在子节点前）
	UWorld* World = ULGUIPrefabManagerObject::GetPreviewWorldForPrefabPackage();
	TArray<UUIItem*> Items;
	TArray<int32> ButtonNodes;
	TArray<TPair<int32, UUITexture*>> SlicedFills;   // 九宫格填充：容器下标 → 满宽贴图（容器摆好后再挂）
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> Node = Nodes[Index]->AsObject();
		const FString Type = Node->GetStringField(TEXT("type"));
		const int32 Parent = Node->GetIntegerField(TEXT("parent"));
		const FString Name = Index == 0 ? FPackageName::GetShortName(PrefabPackage) : Node->GetStringField(TEXT("name"));
		UUIItem* Item = nullptr;
		const TSharedPtr<FJsonObject>* Fill = nullptr;
		UUITexture* SlicedTexture = nullptr;
		if (Type == TEXT("Texture") && Node->TryGetObjectField(TEXT("fill"), Fill) && (*Fill)->HasField(TEXT("sliced")))
		{
			// 九宫格填充（原作 FilledSliced）：左端固定的裁剪容器 + 满宽九宫格贴图，UWuwaUiSlicedFill 按比例改容器宽度
			AUIContainerActor* Container = World->SpawnActor<AUIContainerActor>();
			AddClipCanvas(Container);
			UWuwaUiSlicedFill* SlicedFill = NewObject<UWuwaUiSlicedFill>(Container, TEXT("WuwaUiSlicedFill"), RF_Transactional);
			Container->AddInstanceComponent(SlicedFill);
			SlicedFill->RegisterComponent();
			SlicedFill->FullWidth = (*Fill)->GetNumberField(TEXT("width"));
			AUITextureActor* TextureActor = World->SpawnActor<AUITextureActor>();
			SlicedTexture = TextureActor->GetUITexture();
			UTexture2D* Source = Ctx.Textures.FindRef(Node->GetStringField(TEXT("texture")));
			const TArray<TSharedPtr<FJsonValue>>* Slice = nullptr;
			const bool bSliced = Node->TryGetArrayField(TEXT("slice"), Slice);
			SlicedTexture->SetTexture(Source);
			if (bSliced) SlicedTexture->SetTextureType(EUITextureType::Sliced);
			if (Source) ApplySpriteData(SlicedTexture, Source, bSliced ? Slice : nullptr);
			TextureActor->SetActorLabel(Node->GetStringField(TEXT("name")) + TEXT("_Texture"));
			SlicedFills.Emplace(Index, SlicedTexture);
			Item = Container->GetUIItem();
		}
		else if (Type == TEXT("Texture") && Node->TryGetObjectField(TEXT("fill"), Fill))
		{
			// 填充型：UISprite + Filled（协奏圈这类，运行时改 fillAmount）
			AUISpriteActor* Actor = World->SpawnActor<AUISpriteActor>();
			UUISprite* Sprite = Actor->GetUISprite();
			if (UTexture2D* Source = Ctx.Textures.FindRef(Node->GetStringField(TEXT("texture"))))
			{
				Sprite->SetSprite(GetOrCreateSprite(Ctx, Source), false);
			}
			Sprite->SetSpriteType(EUISpriteType::Filled);
			Sprite->SetFillMethod(FillMethod((*Fill)->GetStringField(TEXT("method"))));
			Sprite->SetFillOrigin(static_cast<uint8>((*Fill)->GetIntegerField(TEXT("origin"))));
			Sprite->SetFillDirectionFlip((*Fill)->GetBoolField(TEXT("flip")));
			Sprite->SetFillAmount((*Fill)->GetNumberField(TEXT("amount")));
			Item = Sprite;
		}
		else if (Type == TEXT("Texture"))
		{
			AUITextureActor* Actor = World->SpawnActor<AUITextureActor>();
			UUITexture* Texture = Actor->GetUITexture();
			UTexture2D* Source = Ctx.Textures.FindRef(Node->GetStringField(TEXT("texture")));
			const TArray<TSharedPtr<FJsonValue>>* Slice = nullptr;
			const bool bSliced = Node->TryGetArrayField(TEXT("slice"), Slice);
			Texture->SetTexture(Source);
			if (bSliced) Texture->SetTextureType(EUITextureType::Sliced);
			if (Source) ApplySpriteData(Texture, Source, bSliced ? Slice : nullptr);
			Item = Texture;
		}
		else if (Type == TEXT("Niagara"))
		{
			// 界面特效：UWuwaUINiagara（Niagara 模拟画进 LGUI 画布），平时不播，由界面代码 Play
			AWuwaUINiagaraActor* Actor = World->SpawnActor<AWuwaUINiagaraActor>();
			const TSharedPtr<FJsonObject> Def = Node->GetObjectField(TEXT("niagara"));
			UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *Def->GetStringField(TEXT("system")));
			if (!System)
			{
				UE_LOG(LogWuwaKuroHud, Error, TEXT("Niagara system %s not found (run -run=WuwaUiEffectSetup)."), *Def->GetStringField(TEXT("system")));
				++Ctx.Errors;
			}
			Actor->GetUINiagara()->SetNiagaraSystem(System);
			Actor->GetUINiagara()->SetNiagaraScale(Def->GetNumberField(TEXT("scale")));
			Item = Actor->GetUINiagara();
		}
		else if (Type == TEXT("Text"))
		{
			AUITextActor* Actor = World->SpawnActor<AUITextActor>();
			UUIText* Label = Actor->GetUIText();
			const TSharedPtr<FJsonObject> Def = Node->GetObjectField(TEXT("text"));
			const FString String = Def->GetStringField(TEXT("text"));
			if (Font) Label->SetFont(Font);
			Label->SetRichText(String.Contains(TEXT("<")));
			Label->SetText(FText::FromString(String));
			Label->SetFontSize(Def->GetNumberField(TEXT("size")));
			Label->SetParagraphHorizontalAlignment(HAlign(Def->GetStringField(TEXT("hAlign"))));
			Label->SetParagraphVerticalAlignment(VAlign(Def->GetStringField(TEXT("vAlign"))));
			Label->SetOverflowType(EUITextOverflowType::HorizontalOverflow);
			const TSharedPtr<FJsonObject>* OutlineEffect = nullptr;
			if (Node->TryGetObjectField(TEXT("outlineEffect"), OutlineEffect))
			{
				// 原作单独的描边组件（UIEffectOutline），颜色运行时可改（伤害数字按元素配色）
				UUIEffectOutline* Outline = NewObject<UUIEffectOutline>(Actor, TEXT("UIEffectOutline"), RF_Transactional);
				Actor->AddInstanceComponent(Outline);
				Outline->RegisterComponent();
				Outline->SetOutlineColor(Color((*OutlineEffect)->GetArrayField(TEXT("color"))));
				Outline->SetOutlineSize(Vec2(*OutlineEffect, TEXT("size")));
			}
			else if (Def->GetNumberField(TEXT("outline")) > 0)
			{
				UUIEffectOutline* Outline = NewObject<UUIEffectOutline>(Actor, TEXT("UIEffectOutline"), RF_Transactional);
				Actor->AddInstanceComponent(Outline);
				Outline->RegisterComponent();
				Outline->SetOutlineColor(Color(Def->GetArrayField(TEXT("outlineColor"))));
				const float Size = Def->GetNumberField(TEXT("outline"));
				Outline->SetOutlineSize(FVector2D(Size, Size));
			}
			Item = Label;
		}
		else
		{
			Item = World->SpawnActor<AUIContainerActor>()->GetUIItem();
		}
		Item->GetOwner()->SetActorLabel(Name);
		if (Parent >= 0) Item->AttachToComponent(Items[Parent], FAttachmentTransformRules::KeepRelativeTransform);
		const TSharedPtr<FJsonObject> Anchor = Node->GetObjectField(TEXT("anchor"));
		FUIAnchorData Data;
		Data.Pivot = Vec2(Anchor, TEXT("pivot"));
		Data.AnchorMin = Vec2(Anchor, TEXT("min"));
		Data.AnchorMax = Vec2(Anchor, TEXT("max"));
		Data.AnchoredPosition = Vec2(Anchor, TEXT("pos"));
		Data.SizeDelta = Vec2(Anchor, TEXT("size"));
		Item->SetAnchorData(Data);
		const FVector2D Scale = Vec2(Node, TEXT("scale"));
		if (!Scale.Equals(FVector2D::UnitVector)) Item->SetRelativeScale3D(FVector(1.f, Scale.X, Scale.Y)); // LGUI 的界面平面：Y 水平、Z 竖直
		double Rotation = 0;
		if (Node->TryGetNumberField(TEXT("rotation"), Rotation) && Rotation != 0) Item->SetRelativeRotation(FRotator(0.f, 0.f, Rotation)); // 平面内旋转 = 绕 X 轴
		const FColor NodeColor = Color(Node->GetArrayField(TEXT("color")));
		if (SlicedTexture)
		{
			SlicedTexture->SetColor(NodeColor);   // 九宫格填充：颜色在里面的贴图上
		}
		else if (UUIBaseRenderable* Renderable = Cast<UUIBaseRenderable>(Item))
		{
			Renderable->SetColor(NodeColor);
		}
		else if (NodeColor.A < 255)
		{
			// 原作旧版 LGUI 的容器透明度会乘到子节点上；LGUI3 用 CanvasGroup 做
			AActor* Owner = Item->GetOwner();
			UUICanvasGroup* Group = NewObject<UUICanvasGroup>(Owner, TEXT("UICanvasGroup"), RF_Transactional);
			Owner->AddInstanceComponent(Group);
			Group->RegisterComponent();
			Group->SetAlpha(NodeColor.A / 255.f);
		}
		Item->SetRaycastTarget(false);
		bool bClip = false;
		if (Node->TryGetBoolField(TEXT("clip"), bClip) && bClip && !SlicedTexture) AddClipCanvas(Item->GetOwner());
		const TArray<TSharedPtr<FJsonValue>>* TextAnimations = nullptr;
		if (Node->TryGetArrayField(TEXT("textAnimations"), TextAnimations)) AddTextAnimations(Ctx, Item->GetOwner(), *TextAnimations, Curves);
		Items.Add(Item);
		bool bButton = false;
		Node->TryGetBoolField(TEXT("button"), bButton);
		if (bButton) ButtonNodes.Add(Index);
	}

	// 2b. 九宫格填充的贴图：挂到容器下，左端对齐、满宽、上下撑满
	for (const TPair<int32, UUITexture*>& Pair : SlicedFills)
	{
		UUIItem* Container = Items[Pair.Key];
		Pair.Value->AttachToComponent(Container, FAttachmentTransformRules::KeepRelativeTransform);
		FUIAnchorData Data;
		Data.Pivot = FVector2D(0.0, 0.5);
		Data.AnchorMin = FVector2D(0.0, 0.0);
		Data.AnchorMax = FVector2D(0.0, 1.0);
		Data.AnchoredPosition = FVector2D::ZeroVector;
		Data.SizeDelta = FVector2D(Container->GetOwner()->FindComponentByClass<UWuwaUiSlicedFill>()->FullWidth, 0.0);
		Pair.Value->SetAnchorData(Data);
		Pair.Value->SetRaycastTarget(false);
	}
	// 2c. 补间（目标可能是任意节点，全部节点建好后再挂）；默认隐藏的节点最后再隐藏
	int32 TweenGroups = 0;
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> Node = Nodes[Index]->AsObject();
		const TArray<TSharedPtr<FJsonValue>>* Tweens = nullptr;
		if (Node->TryGetArrayField(TEXT("tweens"), Tweens))
		{
			AddTweenGroup(Items[Index]->GetOwner(), *Tweens, Items, Curves);
			++TweenGroups;
		}
	}
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		bool bHidden = false;
		if (Nodes[Index]->AsObject()->TryGetBoolField(TEXT("hidden"), bHidden) && bHidden) Items[Index]->SetIsUIActive(false);
	}
	if (TweenGroups) UE_LOG(LogWuwaKuroHud, Display, TEXT("%d tween groups"), TweenGroups);

	// 3. 按钮：过渡目标是子节点里第一个图标
	for (int32 Index : ButtonNodes)
	{
		UUIItem* Target = nullptr;
		TArray<AActor*> Stack = {Items[Index]->GetOwner()};
		while (!Target && Stack.Num())
		{
			AActor* Actor = Stack[0];
			Stack.RemoveAt(0);
			if (UUITexture* Texture = Actor->FindComponentByClass<UUITexture>()) { Target = Texture; break; }
			TArray<AActor*> Children;
			Actor->GetAttachedActors(Children);
			Stack.Append(Children);
		}
		MakeButton(Items[Index], Target);
	}

	// 4. 注册表：节点的 registry = 注册表里依次放的节点下标（导出规格里的 registries 决定，界面代码按下标取）
	int32 Registries = 0;
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
		if (!Nodes[Index]->AsObject()->TryGetArrayField(TEXT("registry"), Entries)) continue;
		TArray<AActor*> Actors;
		for (const TSharedPtr<FJsonValue>& Entry : *Entries)
		{
			const int32 EntryIndex = static_cast<int32>(Entry->AsNumber());
			if (!Items.IsValidIndex(EntryIndex))
			{
				UE_LOG(LogWuwaKuroHud, Error, TEXT("Node %d: registry entry %d is out of range."), Index, EntryIndex);
				return 1;
			}
			Actors.Add(Items[EntryIndex]->GetOwner());
		}
		AddRegistry(Items[Index]->GetOwner(), Actors);
		++Registries;
	}

	// 5. 保存预制体
	if (!Backup(Ctx, PrefabPackage)) return 1;
	ULGUIPrefab* Prefab = LoadAsset<ULGUIPrefab>(PrefabPackage);
	if (!Prefab)
	{
		UPackage* Package = CreatePackage(*PrefabPackage);
		Prefab = NewObject<ULGUIPrefab>(Package, *FPackageName::GetShortName(PrefabPackage), RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Prefab);
	}
	AActor* Root = Items[0]->GetOwner();
	TMap<UObject*, FGuid> ObjectToGuid;
	TMap<TObjectPtr<AActor>, FLGUISubPrefabData> SubPrefabMap;
	Prefab->SavePrefab(Root, ObjectToGuid, SubPrefabMap);
	ULGUIBPLibrary::DestroyActorWithHierarchy(Root, true);
	if (!SaveAsset(Prefab)) return 1;
	UE_LOG(LogWuwaKuroHud, Display, TEXT("Saved %s (%d nodes, %d buttons, %d registries)."), *PrefabPackage, Items.Num(), ButtonNodes.Num(), Registries);

	if (bAddView && !AddViewRow(Ctx, AddView, LayerName, PrefabPackage)) return 1;
	if (!Ctx.BackupDir.IsEmpty()) UE_LOG(LogWuwaKuroHud, Display, TEXT("Backups in %s"), *Ctx.BackupDir);
	return Ctx.Errors ? 1 : 0;
#else
	return 1;
#endif
}
