#include "Editor/Commandlets/WuwaUiTestAssetSetupCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Core/Actor/UIContainerActor.h"
#include "Core/Actor/UITextActor.h"
#include "Core/ActorComponent/UIItem.h"
#include "Core/ActorComponent/UIText.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Game/LGUI/Component/WuwaUiComponentsRegistry.h"
#include "Game/LGUI/Component/WuwaUiViewAnimation.h"
#include "Game/LGUI/Core/WuwaUISettings.h"
#include "Game/LGUI/Test/WuwaTestPopView.h"
#include "Game/Module/Mail/WuwaMailBoxView.h"
#include "HAL/FileManager.h"
#include "Interaction/UIButtonComponent.h"
#include "LGUIBPLibrary.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PrefabSystem/LGUIPrefab.h"
#include "PrefabSystem/LGUIPrefabManager.h"
#include "UObject/Package.h"
#include "Sound/SoundBase.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaUiTestAssetSetup, Log, All);

UWuwaUiTestAssetSetupCommandlet::UWuwaUiTestAssetSetupCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

#if WITH_EDITOR
namespace WuwaUiTestAssetSetup
{
	const TCHAR* TargetPrefabPath = TEXT("/Game/LGUI/TestPop_Prefab.TestPop_Prefab");
	const TCHAR* ButtonPrefabPath = TEXT("/LGUI/Prefabs/Button.Button");
	const TCHAR* NormalPrefabPath = TEXT("/Game/LGUI/TestNormal_Prefab.TestNormal_Prefab");
	const TCHAR* ViewTablePackage = TEXT("/Game/LGUI/DT_UiViews");
	const TCHAR* MailBoxPrefabPackage = TEXT("/Game/LGUI/MailBox_Prefab");
	const TCHAR* MailBoxPrefabPath = TEXT("/Game/LGUI/MailBox_Prefab.MailBox_Prefab");
	const TCHAR* EffectsPrefabPackage = TEXT("/Game/LGUI/TestEffects_Prefab");
	const TCHAR* EffectsPrefabPath = TEXT("/Game/LGUI/TestEffects_Prefab.TestEffects_Prefab");
	const TCHAR* TestSoundPath = TEXT("/Engine/EditorSounds/Notifications/CompileSuccess_Cue.CompileSuccess_Cue");

	/** 按钮预设的 UIButtonComponent 可能在根上也可能在子节点上：找到挂着它的那个 Actor。 */
	AActor* FindActorWithComponent(AActor* Root, TSubclassOf<UActorComponent> Type)
	{
		if (!Root) return nullptr;
		if (Root->FindComponentByClass(Type)) return Root;
		TArray<AActor*> Children;
		Root->GetAttachedActors(Children);
		for (AActor* Child : Children)
		{
			if (AActor* Found = FindActorWithComponent(Child, Type)) return Found;
		}
		return nullptr;
	}

	/** 加载到 LGUI 的预制体预览世界里（和 ULGUIPrefab::RecreatePrefab 一样的做法）。 */
	AActor* LoadForEdit(ULGUIPrefab* Prefab, TMap<FGuid, TObjectPtr<UObject>>& GuidToObject, TMap<TObjectPtr<AActor>, FLGUISubPrefabData>& SubPrefabMap)
	{
		UWorld* World = ULGUIPrefabManagerObject::GetPreviewWorldForPrefabPackage();
		return Prefab->LoadPrefabWithExistingObjects(World, nullptr, GuidToObject, SubPrefabMap);
	}

	bool Backup(const FString& Filename)
	{
		const FString BackupDir = FPaths::ProjectSavedDir() / TEXT("Backups/UiTestAssetSetup") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
		const FString BackupFile = BackupDir / FPaths::GetCleanFilename(Filename);
		if (IFileManager::Get().Copy(*BackupFile, *Filename) != COPY_OK)
		{
			UE_LOG(LogWuwaUiTestAssetSetup, Error, TEXT("Failed to back up %s"), *Filename);
			return false;
		}
		UE_LOG(LogWuwaUiTestAssetSetup, Display, TEXT("Backed up to %s"), *BackupFile);
		return true;
	}

	bool Save(UObject* Asset)
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
			UE_LOG(LogWuwaUiTestAssetSetup, Error, TEXT("Failed to save %s"), *Filename);
			return false;
		}
		return true;
	}

	/** 要放进注册表的一个控件：按钮（带文字）或纯文字。数组下标就是注册表编号。 */
	struct FWidgetSpec
	{
		bool bButton = false;
		FString Label;
		FString Text;
		FVector2D Position = FVector2D::ZeroVector;
	};

	AActor* AddButton(AActor* Root, UUIItem* RootItem, ULGUIPrefab* ButtonPrefab, const FWidgetSpec& Spec)
	{
		// LoadPrefabInEditor 不保留和预设的关联，生成的就是普通 Actor。
		AActor* ButtonRoot = ButtonPrefab->LoadPrefabInEditor(Root->GetWorld(), RootItem, true);
		AActor* ButtonActor = FindActorWithComponent(ButtonRoot, UUIButtonComponent::StaticClass());
		if (!ButtonActor)
		{
			return nullptr;
		}
		ButtonRoot->SetActorLabel(Spec.Label);
		if (UUIItem* ButtonItem = Cast<UUIItem>(ButtonRoot->GetRootComponent()))
		{
			ButtonItem->SetAnchoredPosition(Spec.Position);
		}
		if (AActor* ButtonTextActor = FindActorWithComponent(ButtonRoot, UUIText::StaticClass()))
		{
			ButtonTextActor->FindComponentByClass<UUIText>()->SetText(FText::FromString(Spec.Text));
		}
		return ButtonActor;
	}

	AActor* AddText(AActor* Root, UUIItem* RootItem, const FWidgetSpec& Spec)
	{
		AUITextActor* TextActor = Root->GetWorld()->SpawnActor<AUITextActor>();
		TextActor->SetActorLabel(Spec.Label);
		UUIText* Text = TextActor->GetUIText();
		Text->AttachToComponent(RootItem, FAttachmentTransformRules::KeepRelativeTransform);
		Text->SetWidth(400);
		Text->SetHeight(60);
		Text->SetFontSize(32);
		Text->SetAnchoredPosition(Spec.Position);
		Text->SetText(FText::FromString(Spec.Text));
		return TextActor;
	}

	/** 保存后重新加载一遍，确认注册表里每个编号都是想要的组件。 */
	bool VerifyPrefab(ULGUIPrefab* Prefab, const TArray<FWidgetSpec>& Widgets)
	{
		TMap<FGuid, TObjectPtr<UObject>> GuidToObject;
		TMap<TObjectPtr<AActor>, FLGUISubPrefabData> SubPrefabMap;
		AActor* Root = LoadForEdit(Prefab, GuidToObject, SubPrefabMap);
		const UWuwaUiComponentsRegistry* Registry = Root ? Root->FindComponentByClass<UWuwaUiComponentsRegistry>() : nullptr;
		bool bOk = Registry && Registry->Num() == Widgets.Num();
		for (int32 i = 0; bOk && i < Widgets.Num(); ++i)
		{
			const AActor* Actor = Registry->GetActorAt(i);
			bOk = Actor && (Widgets[i].bButton ? Actor->FindComponentByClass<UUIButtonComponent>() != nullptr : Actor->FindComponentByClass<UUIText>() != nullptr);
		}
		if (Root) ULGUIBPLibrary::DestroyActorWithHierarchy(Root, true);
		return bOk;
	}

	/** 给预制体加上控件和 WuwaUiComponentsRegistry；已经有注册表就跳过。 */
	bool BuildRegistryPrefab(ULGUIPrefab* Prefab, const TArray<FWidgetSpec>& Widgets, bool bApply, bool bBackup)
	{
		const FString PrefabName = Prefab->GetPathName();
		ULGUIPrefab* ButtonPrefab = LoadObject<ULGUIPrefab>(nullptr, ButtonPrefabPath);
		if (!ButtonPrefab)
		{
			UE_LOG(LogWuwaUiTestAssetSetup, Error, TEXT("Missing %s"), ButtonPrefabPath);
			return false;
		}

		TMap<FGuid, TObjectPtr<UObject>> GuidToObject;
		TMap<TObjectPtr<AActor>, FLGUISubPrefabData> SubPrefabMap;
		AActor* Root = LoadForEdit(Prefab, GuidToObject, SubPrefabMap);
		UUIItem* RootItem = Root ? Cast<UUIItem>(Root->GetRootComponent()) : nullptr;
		if (!RootItem)
		{
			UE_LOG(LogWuwaUiTestAssetSetup, Error, TEXT("%s has no UIItem root."), *PrefabName);
			return false;
		}
		if (Root->FindComponentByClass<UWuwaUiComponentsRegistry>())
		{
			ULGUIBPLibrary::DestroyActorWithHierarchy(Root, true);
			if (!VerifyPrefab(Prefab, Widgets))
			{
				UE_LOG(LogWuwaUiTestAssetSetup, Error, TEXT("%s already has a WuwaUiComponentsRegistry that does not match what the code expects; fix or delete it."), *PrefabName);
				return false;
			}
			UE_LOG(LogWuwaUiTestAssetSetup, Display, TEXT("%s already set up, skipped."), *PrefabName);
			return true;
		}

		UE_LOG(LogWuwaUiTestAssetSetup, Display, TEXT("Prefab %s: will add %d widgets and a WuwaUiComponentsRegistry."), *PrefabName, Widgets.Num());
		if (!bApply)
		{
			ULGUIBPLibrary::DestroyActorWithHierarchy(Root, true);
			return true;
		}

		TArray<AActor*> RegistryActors;
		for (const FWidgetSpec& Spec : Widgets)
		{
			AActor* Actor = Spec.bButton ? AddButton(Root, RootItem, ButtonPrefab, Spec) : AddText(Root, RootItem, Spec);
			if (!Actor)
			{
				UE_LOG(LogWuwaUiTestAssetSetup, Error, TEXT("Failed to add '%s' (button preset has no UIButtonComponent?)."), *Spec.Label);
				ULGUIBPLibrary::DestroyActorWithHierarchy(Root, true);
				return false;
			}
			RegistryActors.Add(Actor);
		}

		// 注册表挂在根上：下标顺序就是代码里的编号。
		UWuwaUiComponentsRegistry* Registry = NewObject<UWuwaUiComponentsRegistry>(Root, TEXT("WuwaUiComponentsRegistry"), RF_Transactional);
		Root->AddInstanceComponent(Registry);
		Registry->RegisterComponent();
		Registry->SetComponents(RegistryActors);

		const FString Filename = FPackageName::LongPackageNameToFilename(Prefab->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		if (bBackup && !Backup(Filename))
		{
			ULGUIBPLibrary::DestroyActorWithHierarchy(Root, true);
			return false;
		}

		TMap<UObject*, FGuid> ObjectToGuid;
		for (const TPair<FGuid, TObjectPtr<UObject>>& Pair : GuidToObject)
		{
			if (IsValid(Pair.Value)) ObjectToGuid.Add(Pair.Value, Pair.Key);
		}
		Prefab->SavePrefab(Root, ObjectToGuid, SubPrefabMap);
		ULGUIBPLibrary::DestroyActorWithHierarchy(Root, true);

		if (!Save(Prefab) || !VerifyPrefab(Prefab, Widgets))
		{
			UE_LOG(LogWuwaUiTestAssetSetup, Error, TEXT("%s not saved, or reloading did not find the expected registry."), *PrefabName);
			return false;
		}
		UE_LOG(LogWuwaUiTestAssetSetup, Display, TEXT("Saved and verified %s"), *PrefabName);
		return true;
	}

	/** TestPop_Prefab：[0] 关闭按钮、[1] 计数文字。 */
	bool SetupPopPrefab(bool bApply)
	{
		ULGUIPrefab* Prefab = LoadObject<ULGUIPrefab>(nullptr, TargetPrefabPath);
		if (!Prefab)
		{
			UE_LOG(LogWuwaUiTestAssetSetup, Error, TEXT("Missing %s"), TargetPrefabPath);
			return false;
		}
		return BuildRegistryPrefab(Prefab, {
			{true, TEXT("BtnClose"), TEXT("Close"), FVector2D(0, -80)},
			{false, TEXT("TxtCount"), TEXT("Opened 0 times"), FVector2D(0, 60)},
		}, bApply, true);
	}

	/** 新建一个只有 UIContainerActor 根节点的空 UI 预制体（≈ LGUI 的 ULGUIPrefabFactoryForUI）。 */
	ULGUIPrefab* CreateEmptyUiPrefab(const TCHAR* PackageName, FVector2D Size)
	{
		UPackage* Package = CreatePackage(PackageName);
		ULGUIPrefab* Prefab = NewObject<ULGUIPrefab>(Package, *FPackageName::GetShortName(PackageName), RF_Public | RF_Standalone | RF_Transactional);
		AUIContainerActor* Root = ULGUIPrefabManagerObject::GetPreviewWorldForPrefabPackage()->SpawnActor<AUIContainerActor>();
		Root->SetActorLabel(FPackageName::GetShortName(PackageName));
		if (UUIItem* RootItem = Cast<UUIItem>(Root->GetRootComponent()))
		{
			RootItem->SetWidth(Size.X);
			RootItem->SetHeight(Size.Y);
		}
		TMap<UObject*, FGuid> ObjectToGuid;
		TMap<TObjectPtr<AActor>, FLGUISubPrefabData> SubPrefabMap;
		Prefab->SavePrefab(Root, ObjectToGuid, SubPrefabMap);
		ULGUIBPLibrary::DestroyActorWithHierarchy(Root, true);
		FAssetRegistryModule::AssetCreated(Prefab);
		return Save(Prefab) ? Prefab : nullptr;
	}

	/** MailBox_Prefab（邮件示例）：没有就新建一个空的，[0] 读一封、[1] 关闭、[2] 未读数文字。 */
	bool SetupMailBoxPrefab(bool bApply)
	{
		const FString ObjectPath = FString::Printf(TEXT("%s.%s"), MailBoxPrefabPackage, *FPackageName::GetShortName(MailBoxPrefabPackage));
		ULGUIPrefab* Prefab = LoadObject<ULGUIPrefab>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Prefab)
		{
			UE_LOG(LogWuwaUiTestAssetSetup, Display, TEXT("Prefab %s: will be created."), *ObjectPath);
			if (!bApply)
			{
				return true;
			}
			Prefab = CreateEmptyUiPrefab(MailBoxPrefabPackage, FVector2D(600, 400));
			if (!Prefab)
			{
				return false;
			}
		}
		return BuildRegistryPrefab(Prefab, {
			{true, TEXT("BtnRead"), TEXT("Read one"), FVector2D(-120, -80)},
			{true, TEXT("BtnClose"), TEXT("Close"), FVector2D(120, -80)},
			{false, TEXT("TxtUnread"), TEXT("Unread: 0"), FVector2D(0, 60)},
		}, bApply, false);
	}

	/** TestEffects_Prefab（第 7 步的效果测试）：空 UI 预制体，根节点挂 WuwaUiViewAnimation（打开淡入+缩放，关闭淡出）。 */
	bool SetupEffectsPrefab(bool bApply)
	{
		const FString ObjectPath = FString::Printf(TEXT("%s.%s"), EffectsPrefabPackage, *FPackageName::GetShortName(EffectsPrefabPackage));
		ULGUIPrefab* Prefab = LoadObject<ULGUIPrefab>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Prefab)
		{
			UE_LOG(LogWuwaUiTestAssetSetup, Display, TEXT("%s already exists, skipped."), *ObjectPath);
			return true;
		}
		UE_LOG(LogWuwaUiTestAssetSetup, Display, TEXT("Prefab %s: will be created with a WuwaUiViewAnimation."), *ObjectPath);
		if (!bApply)
		{
			return true;
		}

		Prefab = CreateEmptyUiPrefab(EffectsPrefabPackage, FVector2D(600, 400));
		if (!Prefab)
		{
			return false;
		}
		TMap<FGuid, TObjectPtr<UObject>> GuidToObject;
		TMap<TObjectPtr<AActor>, FLGUISubPrefabData> SubPrefabMap;
		AActor* Root = LoadForEdit(Prefab, GuidToObject, SubPrefabMap);
		if (!Root)
		{
			return false;
		}
		UWuwaUiViewAnimation* Animation = NewObject<UWuwaUiViewAnimation>(Root, TEXT("WuwaUiViewAnimation"), RF_Transactional);
		Animation->ShowEffect = EWuwaUiAnimEffect::FadeScale;
		Animation->ShowDuration = 0.3f;
		Animation->HideEffect = EWuwaUiAnimEffect::Fade;
		Animation->HideDuration = 0.2f;
		Root->AddInstanceComponent(Animation);
		Animation->RegisterComponent();

		TMap<UObject*, FGuid> ObjectToGuid;
		for (const TPair<FGuid, TObjectPtr<UObject>>& Pair : GuidToObject)
		{
			if (IsValid(Pair.Value)) ObjectToGuid.Add(Pair.Value, Pair.Key);
		}
		Prefab->SavePrefab(Root, ObjectToGuid, SubPrefabMap);
		ULGUIBPLibrary::DestroyActorWithHierarchy(Root, true);
		if (!Save(Prefab))
		{
			return false;
		}

		// 重新加载确认组件和参数存进去了。
		AActor* Reloaded = LoadForEdit(Prefab, GuidToObject, SubPrefabMap);
		const UWuwaUiViewAnimation* Check = Reloaded ? Reloaded->FindComponentByClass<UWuwaUiViewAnimation>() : nullptr;
		const bool bOk = Check && Check->ShowEffect == EWuwaUiAnimEffect::FadeScale && FMath::IsNearlyEqual(Check->ShowDuration, 0.3f);
		if (Reloaded) ULGUIBPLibrary::DestroyActorWithHierarchy(Reloaded, true);
		UE_LOG(LogWuwaUiTestAssetSetup, Display, TEXT("%s %s"), bOk ? TEXT("Saved and verified") : TEXT("Saved but verification FAILED for"), *ObjectPath);
		return bOk;
	}

	FWuwaUiViewConfig MakeRow(const TCHAR* PrefabPath, EWuwaUiLayer Layer)
	{
		FWuwaUiViewConfig Row;
		Row.Prefab = TSoftObjectPtr<ULGUIPrefab>(FSoftObjectPath(PrefabPath));
		Row.Layer = Layer;
		return Row;
	}

	/** 框架测试（Wuwa.UI.*）用到的界面。 */
	TArray<TPair<FName, FWuwaUiViewConfig>> TestRows()
	{
		TArray<TPair<FName, FWuwaUiViewConfig>> Rows;
		Rows.Emplace(TEXT("TestNormalView"), MakeRow(NormalPrefabPath, EWuwaUiLayer::Normal));
		Rows.Emplace(TEXT("TestNormalView2"), MakeRow(NormalPrefabPath, EWuwaUiLayer::Normal));

		FWuwaUiViewConfig Pop = MakeRow(TargetPrefabPath, EWuwaUiLayer::Pop);
		Pop.ViewClass = UWuwaTestPopView::StaticClass();
		Pop.bAllowMultiple = true;
		Rows.Emplace(TEXT("TestPopView"), Pop);

		const TCHAR* FloatNames[] = {TEXT("TestFloatA"), TEXT("TestFloatB"), TEXT("TestFloatC")};
		const int32 FloatPriorities[] = {1, 3, 2};
		for (int32 i = 0; i < 3; ++i)
		{
			FWuwaUiViewConfig Float = MakeRow(NormalPrefabPath, EWuwaUiLayer::Float);
			Float.Priority = FloatPriorities[i];
			Rows.Emplace(FloatNames[i], Float);
		}

		FWuwaUiViewConfig Obstructed = MakeRow(NormalPrefabPath, EWuwaUiLayer::Pop);
		Obstructed.BeObstructViews = {TEXT("TestNormalView2")};
		Rows.Emplace(TEXT("TestObstructedPop"), Obstructed);

		FWuwaUiViewConfig Permanent = MakeRow(NormalPrefabPath, EWuwaUiLayer::HUD);
		Permanent.bIsPermanent = true;
		Rows.Emplace(TEXT("TestPermanentHud"), Permanent);

		// 第 7 步：打开效果全开（音效、时间缩放、模糊、UI 相机、全屏）+ 预制体上的打开/关闭动画。
		FWuwaUiViewConfig Effects = MakeRow(EffectsPrefabPath, EWuwaUiLayer::Normal);
		Effects.OpenSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TestSoundPath));
		Effects.CloseSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TestSoundPath));
		Effects.TimeDilation = 0.5f;
		Effects.bBlurBackground = true;
		Effects.UiCameraMode = TEXT("TestCamera");
		Effects.bIsFullScreen = true;
		Rows.Emplace(TEXT("TestEffectsView"), Effects);

		// 和上面叠加时，时间缩放取更慢的 0.2。
		FWuwaUiViewConfig SlowPop = MakeRow(EffectsPrefabPath, EWuwaUiLayer::Pop);
		SlowPop.TimeDilation = 0.2f;
		Rows.Emplace(TEXT("TestSlowPop"), SlowPop);

		// 邮件示例模块。
		FWuwaUiViewConfig MailBox = MakeRow(MailBoxPrefabPath, EWuwaUiLayer::Normal);
		MailBox.ViewClass = UWuwaMailBoxView::StaticClass();
		Rows.Emplace(TEXT("MailBoxView"), MailBox);
		return Rows;
	}

	bool SetupViewTable(bool bApply)
	{
		const FString ObjectPath = FString::Printf(TEXT("%s.%s"), ViewTablePackage, *FPackageName::GetShortName(ViewTablePackage));
		UDataTable* Table = LoadObject<UDataTable>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		const TArray<TPair<FName, FWuwaUiViewConfig>> Rows = TestRows();
		UE_LOG(LogWuwaUiTestAssetSetup, Display, TEXT("View table: %s %s, writing %d test rows."),
			Table ? TEXT("update") : TEXT("create"), ViewTablePackage, Rows.Num());
		if (!bApply)
		{
			return true;
		}

		if (!Table)
		{
			UPackage* Package = CreatePackage(ViewTablePackage);
			Table = NewObject<UDataTable>(Package, *FPackageName::GetShortName(ViewTablePackage), RF_Public | RF_Standalone | RF_Transactional);
			Table->RowStruct = FWuwaUiViewConfig::StaticStruct();
			FAssetRegistryModule::AssetCreated(Table);
		}
		else if (Table->GetRowStruct() != FWuwaUiViewConfig::StaticStruct())
		{
			UE_LOG(LogWuwaUiTestAssetSetup, Error, TEXT("%s exists but its row struct is not FWuwaUiViewConfig."), *ObjectPath);
			return false;
		}
		else
		{
			const FString Filename = FPackageName::LongPackageNameToFilename(ViewTablePackage, FPackageName::GetAssetPackageExtension());
			if (!Backup(Filename)) return false;
		}

		// AddRow 遇到同名行会覆盖；表里其他（业务）行不动。
		for (const TPair<FName, FWuwaUiViewConfig>& Row : Rows)
		{
			Table->AddRow(Row.Key, Row.Value);
		}
		if (!Save(Table))
		{
			return false;
		}
		UE_LOG(LogWuwaUiTestAssetSetup, Display, TEXT("Saved %s with %d rows."), *ObjectPath, Table->GetRowMap().Num());
		return true;
	}
}
#endif

int32 UWuwaUiTestAssetSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaUiTestAssetSetup;
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	const bool bPrefabOk = SetupPopPrefab(bApply);
	const bool bMailOk = SetupMailBoxPrefab(bApply);
	const bool bEffectsOk = SetupEffectsPrefab(bApply);
	const bool bTableOk = SetupViewTable(bApply);
	if (!bApply)
	{
		UE_LOG(LogWuwaUiTestAssetSetup, Display, TEXT("Dry run. Pass -Apply to modify and save."));
	}
	return bPrefabOk && bMailOk && bEffectsOk && bTableOk ? 0 : 1;
#else
	return 1;
#endif
}
