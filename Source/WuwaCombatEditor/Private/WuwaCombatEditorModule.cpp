#include "BulletEditor/SWuwaBulletEditor.h"
#include "ContentBrowserMenuContexts.h"
#include "Engine/DataTable.h"
#include "Framework/Docking/TabManager.h"
#include "Game/NewWorld/Bullet/WuwaBulletTypes.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "WuwaCombatEditor"

/** 战斗编辑器工具：注册"子弹编辑器"页签（Window ▸ Tools），并在子弹表的右键菜单里加"在子弹编辑器中打开"。 */
class FWuwaCombatEditorModule : public IModuleInterface
{
public:
	static inline const FName BulletEditorTab = TEXT("WuwaBulletEditor");

	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(BulletEditorTab, FOnSpawnTab::CreateRaw(this, &FWuwaCombatEditorModule::SpawnBulletEditor))
			.SetDisplayName(LOCTEXT("BulletEditor", "子弹编辑器"))
			.SetTooltipText(LOCTEXT("BulletEditorTip", "可视化查看 / 编辑子弹表（FWuwaBulletInfo）的判定体形状、出生位置"))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.DataTable"));
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FWuwaCombatEditorModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(BulletEditorTab);
		}
	}

	/** 打开（或切到）子弹编辑器并显示这张表；RowName 非空时选中那一行。 */
	bool OpenBulletEditor(UDataTable* Table, FName RowName = NAME_None)
	{
		FGlobalTabmanager::Get()->TryInvokeTab(BulletEditorTab);
		TSharedPtr<SWuwaBulletEditor> Editor = BulletEditor.Pin();
		if (!Editor) return false;
		Editor->SetTable(Table);
		if (!RowName.IsNone()) Editor->SelectRow(RowName);
		return true;
	}

private:
	TSharedRef<SDockTab> SpawnBulletEditor(const FSpawnTabArgs& Args)
	{
		TSharedRef<SWuwaBulletEditor> Editor = SNew(SWuwaBulletEditor);
		BulletEditor = Editor;
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)[Editor];
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("ContentBrowser.AssetContextMenu.DataTable"));
		FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("GetAssetActions"));
		Section.AddDynamicEntry(TEXT("WuwaOpenBulletEditor"), FNewToolMenuSectionDelegate::CreateLambda([this](FToolMenuSection& InSection)
		{
			const UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
			if (!Context || Context->SelectedAssets.Num() != 1) return;
			const FAssetData& Asset = Context->SelectedAssets[0];
			if (!Asset.GetTagValueRef<FString>(TEXT("RowStructure")).EndsWith(TEXT("WuwaBulletInfo"))) return;
			InSection.AddMenuEntry(TEXT("WuwaOpenBulletEditor"),
				LOCTEXT("OpenInBulletEditor", "在子弹编辑器中打开"),
				LOCTEXT("OpenInBulletEditorTip", "可视化查看 / 编辑这张子弹表"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.DataTable"),
				FUIAction(FExecuteAction::CreateLambda([this, Asset]()
				{
					OpenBulletEditor(Cast<UDataTable>(Asset.GetAsset()));
				})));
		}));
	}

	TWeakPtr<SWuwaBulletEditor> BulletEditor;
};

IMPLEMENT_MODULE(FWuwaCombatEditorModule, WuwaCombatEditor)

/** UWuwaCombatEditorLibrary::OpenBulletEditor 的实现（模块对象只在这个文件里可见）。 */
bool WuwaOpenBulletEditor(UDataTable* Table, FName RowName)
{
	FWuwaCombatEditorModule* Module = FModuleManager::GetModulePtr<FWuwaCombatEditorModule>(TEXT("WuwaCombatEditor"));
	return Module && Table && Module->OpenBulletEditor(Table, RowName);
}

#undef LOCTEXT_NAMESPACE
