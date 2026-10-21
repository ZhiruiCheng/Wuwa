#include "BulletEditor/SWuwaBulletEditor.h"

#include "Animation/AnimMontage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BulletEditor/SWuwaBulletViewport.h"
#include "DataTableEditorUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Engine/SkeletalMesh.h"
#include "FileHelpers.h"
#include "Game/NewWorld/Bullet/WuwaBulletTypes.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"
#include "GameFramework/Character.h"
#include "IStructureDetailsView.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "UObject/StructOnScope.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WuwaBulletEditor"

namespace WuwaBulletEditor
{
	const TCHAR* ConfigSection = TEXT("WuwaBulletEditor");

	bool IsBulletTable(const FAssetData& Asset)
	{
		const FString RowStructure = Asset.GetTagValueRef<FString>(TEXT("RowStructure"));
		return RowStructure.EndsWith(TEXT("WuwaBulletInfo"));
	}

	bool IsSkillTable(const FAssetData& Asset)
	{
		return Asset.GetTagValueRef<FString>(TEXT("RowStructure")).EndsWith(TEXT("WuwaSkillInfo"));
	}
}

void SWuwaBulletEditor::Construct(const FArguments& InArgs)
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = true;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = PropertyEditor.CreateStructureDetailView(DetailsArgs, FStructureDetailsViewArgs(), nullptr);
	DetailsView->GetOnFinishedChangingPropertiesDelegate().AddSP(this, &SWuwaBulletEditor::OnFinishedChangingProperties);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(6.f, 4.f)
		[
			MakeToolbar()
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SSplitter)
			+ SSplitter::Slot().Value(0.2f)
			[
				MakeRowPanel()
			]
			+ SSplitter::Slot().Value(0.52f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SAssignNew(Viewport, SWuwaBulletViewport).Editor(SharedThis(this))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(4.f)
				[
					MakeTimeBar()
				]
			]
			+ SSplitter::Slot().Value(0.28f)
			[
				DetailsView->GetWidget().ToSharedRef()
			]
		]
	];

	// 上次打开的表
	FString LastTable;
	if (GConfig->GetString(WuwaBulletEditor::ConfigSection, TEXT("LastTable"), LastTable, GEditorPerProjectIni) && !LastTable.IsEmpty())
	{
		SetTable(LoadObject<UDataTable>(nullptr, *LastTable));
	}
}

SWuwaBulletEditor::~SWuwaBulletEditor() = default;

TSharedRef<SWidget> SWuwaBulletEditor::MakeToolbar()
{
	auto Label = [](const FText& Text)
	{
		return SNew(STextBlock).Text(Text);
	};
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 4.f, 0.f)
		[
			Label(LOCTEXT("Table", "子弹表"))
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).MaxWidth(320.f)
		[
			SNew(SObjectPropertyEntryBox)
			.AllowedClass(UDataTable::StaticClass())
			.ObjectPath(this, &SWuwaBulletEditor::GetTablePath)
			.OnObjectChanged(this, &SWuwaBulletEditor::OnTableChanged)
			.OnShouldFilterAsset(this, &SWuwaBulletEditor::ShouldFilterTable)
			.AllowClear(false)
			.DisplayThumbnail(false)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 4.f, 0.f)
		[
			Label(LOCTEXT("Character", "预览角色"))
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).MaxWidth(260.f)
		[
			SNew(SClassPropertyEntryBox)
			.MetaClass(ACharacter::StaticClass())
			.AllowNone(true)
			.SelectedClass(this, &SWuwaBulletEditor::GetSelectedCharacterClass)
			.OnSetClass(this, &SWuwaBulletEditor::OnCharacterClassChanged)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 4.f, 0.f)
		[
			Label(LOCTEXT("PreviewMesh", "预览网格"))
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).MaxWidth(260.f)
		[
			SNew(SObjectPropertyEntryBox)
			.AllowedClass(USkeletalMesh::StaticClass())
			.ObjectPath(this, &SWuwaBulletEditor::GetPreviewMeshPath)
			.OnObjectChanged(this, &SWuwaBulletEditor::OnPreviewMeshChanged)
			.ToolTipText(LOCTEXT("PreviewMeshTip", "留空 = 用角色蓝图里的网格；没有角色蓝图（怪物 / Boss）时用这个网格，胶囊按网格包围盒估计"))
			.DisplayThumbnail(false)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 4.f, 0.f)
		[
			Label(LOCTEXT("Montage", "蒙太奇"))
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).MaxWidth(320.f)
		[
			SNew(SObjectPropertyEntryBox)
			.AllowedClass(UAnimMontage::StaticClass())
			.ObjectPath(this, &SWuwaBulletEditor::GetMontagePath)
			.OnObjectChanged(this, &SWuwaBulletEditor::OnMontageChanged)
			.DisplayThumbnail(false)
		]
		+ SHorizontalBox::Slot().FillWidth(0.2f)
		[
			SNullWidget::NullWidget
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f, 0.f, 0.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("Save", "保存"))
			.ToolTipText(LOCTEXT("SaveTip", "保存子弹表"))
			.IsEnabled_Lambda([this]() { return Table && Table->GetPackage()->IsDirty(); })
			.OnClicked(this, &SWuwaBulletEditor::OnSave)
		];
}

TSharedRef<SWidget> SWuwaBulletEditor::MakeRowPanel()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(2.f)
		[
			SNew(SSearchBox)
			.HintText(LOCTEXT("Search", "按 ID / 名字过滤"))
			.OnTextChanged(this, &SWuwaBulletEditor::OnFilterChanged)
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SAssignNew(RowList, SListView<FRowItemPtr>)
			.ListItemsSource(&VisibleRows)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SWuwaBulletEditor::OnGenerateRow)
			.OnSelectionChanged(this, &SWuwaBulletEditor::OnRowSelected)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(2.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SButton).HAlign(HAlign_Center).Text(LOCTEXT("Add", "新建"))
				.ToolTipText(LOCTEXT("AddTip", "在选中子弹所属技能下新建一颗（ID 取该技能最大序号 + 1）"))
				.IsEnabled_Lambda([this]() { return Table != nullptr; })
				.OnClicked(this, &SWuwaBulletEditor::OnAddRow)
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SButton).HAlign(HAlign_Center).Text(LOCTEXT("Duplicate", "复制"))
				.IsEnabled_Lambda([this]() { return GetSelectedBullet() != nullptr; })
				.OnClicked(this, &SWuwaBulletEditor::OnDuplicateRow)
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SButton).HAlign(HAlign_Center).Text(LOCTEXT("Delete", "删除"))
				.IsEnabled_Lambda([this]() { return GetSelectedBullet() != nullptr; })
				.OnClicked(this, &SWuwaBulletEditor::OnDeleteRow)
			]
		];
}

TSharedRef<SWidget> SWuwaBulletEditor::MakeTimeBar()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton)
			.Text_Lambda([this]() { return bPlaying ? LOCTEXT("Pause", "暂停") : LOCTEXT("Play", "播放"); })
			.IsEnabled_Lambda([this]() { return Montage != nullptr; })
			.OnClicked(this, &SWuwaBulletEditor::OnTogglePlay)
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(8.f, 0.f)
		[
			SNew(SSlider)
			.IsEnabled_Lambda([this]() { return Montage != nullptr; })
			.Value_Lambda([this]() { const float Length = GetMontageLength(); return Length > 0.f ? PreviewTime / Length : 0.f; })
			.OnValueChanged_Lambda([this](float Value) { PreviewTime = Value * GetMontageLength(); bPlaying = false; })
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(this, &SWuwaBulletEditor::GetTimeText)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([this]() { return bApplyRootMotion ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bApplyRootMotion = State == ECheckBoxState::Checked; })
			.ToolTipText(LOCTEXT("RootMotionTip", "按蒙太奇根运动移动发射者（游戏里胶囊会被根运动带着走，没挂骨骼的子弹从当时的位置出生）"))
			[
				SNew(STextBlock).Text(LOCTEXT("RootMotion", "根运动"))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([this]() { return bShowSiblings ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bShowSiblings = State == ECheckBoxState::Checked; })
			[
				SNew(STextBlock).Text(LOCTEXT("Siblings", "同技能其它子弹"))
			]
		];
}

// ---- 表 / 伴随资产 ----

void SWuwaBulletEditor::SetTable(UDataTable* InTable)
{
	if (InTable && InTable->GetRowStruct() != FWuwaBulletInfo::StaticStruct())
	{
		InTable = nullptr;
	}
	Table = InTable;
	SkillTable = nullptr;
	AutoPickedSkillId = 0;
	SelectedRow = NAME_None;
	EditedRow.Reset();
	if (DetailsView) DetailsView->SetStructureData(nullptr);
	if (Table)
	{
		GConfig->SetString(WuwaBulletEditor::ConfigSection, TEXT("LastTable"), *Table->GetPathName(), GEditorPerProjectIni);
		FindCompanions();
	}
	RefreshRows();
	if (VisibleRows.Num() > 0)
	{
		SelectRow(VisibleRows[0]->Name);
	}
	if (Viewport) Viewport->FocusOnCharacter();
}

void SWuwaBulletEditor::FindCompanions()
{
	CharacterClass = nullptr;
	PreviewMesh = nullptr;
	Montage = nullptr;
	PreviewTime = 0.f;
	bPlaying = false;
	// 约定的目录：<角色目录>/Data/DT_BulletInfo_X 旁边是 DT_SkillInfo_X；角色蓝图在 <角色目录> 下
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	const FString DataFolder = FPackageName::GetLongPackagePath(Table->GetOutermost()->GetName());
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(*DataFolder), Assets, /*bRecursive*/ false);
	for (const FAssetData& Asset : Assets)
	{
		if (WuwaBulletEditor::IsSkillTable(Asset))
		{
			SkillTable = Cast<UDataTable>(Asset.GetAsset());
			break;
		}
	}

	const FString RoleFolder = DataFolder.EndsWith(TEXT("/Data")) ? FPackageName::GetLongPackagePath(DataFolder) : DataFolder;
	Assets.Reset();
	Registry.GetAssetsByPath(FName(*RoleFolder), Assets, /*bRecursive*/ true);
	Assets.Sort([](const FAssetData& A, const FAssetData& B)
	{
		// 先看根目录、名字以 BP_ 开头的
		const int32 DepthA = A.PackagePath.ToString().Len(), DepthB = B.PackagePath.ToString().Len();
		return DepthA != DepthB ? DepthA < DepthB : A.AssetName.LexicalLess(B.AssetName);
	});
	for (const FAssetData& Asset : Assets)
	{
		if (Asset.AssetClassPath != UBlueprint::StaticClass()->GetClassPathName() || !Asset.AssetName.ToString().StartsWith(TEXT("BP_"))) continue;
		const UBlueprint* Blueprint = Cast<UBlueprint>(Asset.GetAsset());
		if (Blueprint && Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(ACharacter::StaticClass()))
		{
			CharacterClass = Blueprint->GeneratedClass;
			return;
		}
	}

	// 没有角色蓝图：找骨架和技能蒙太奇一致的骨骼网格（武器网格骨架不同，会被排除）
	const USkeleton* Skeleton = nullptr;
	if (SkillTable)
	{
		SkillTable->ForeachRow<FWuwaSkillInfo>(TEXT("SWuwaBulletEditor"), [&Skeleton](const FName&, const FWuwaSkillInfo& Skill)
		{
			if (!Skeleton && Skill.Montages.Num() > 0)
			{
				if (const UAnimMontage* First = Skill.Montages[0].LoadSynchronous()) Skeleton = First->GetSkeleton();
			}
		});
	}
	USkeletalMesh* Fallback = nullptr;
	for (const FAssetData& Asset : Assets)
	{
		if (Asset.AssetClassPath != USkeletalMesh::StaticClass()->GetClassPathName()) continue;
		USkeletalMesh* Candidate = Cast<USkeletalMesh>(Asset.GetAsset());
		if (!Candidate) continue;
		if (Skeleton && Candidate->GetSkeleton() == Skeleton)
		{
			PreviewMesh = Candidate;
			return;
		}
		if (!Fallback) Fallback = Candidate;
	}
	PreviewMesh = Fallback;
}

void SWuwaBulletEditor::RefreshRows()
{
	AllRows.Reset();
	if (Table)
	{
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			const FWuwaBulletInfo* Info = reinterpret_cast<const FWuwaBulletInfo*>(Pair.Value);
			FRowItemPtr Item = MakeShared<FRowItem>();
			Item->Name = Pair.Key;
			Item->Label = FString::Printf(TEXT("%s  %s"), *Pair.Key.ToString(), *Info->BulletName);
			AllRows.Add(Item);
		}
		AllRows.Sort([](const FRowItemPtr& A, const FRowItemPtr& B)
		{
			const int64 IdA = WuwaBullet::ParseBulletId(A->Name), IdB = WuwaBullet::ParseBulletId(B->Name);
			return IdA != IdB ? IdA < IdB : A->Name.LexicalLess(B->Name);
		});
	}
	VisibleRows.Reset();
	for (const FRowItemPtr& Item : AllRows)
	{
		if (Filter.IsEmpty() || Item->Label.Contains(Filter))
		{
			VisibleRows.Add(Item);
		}
	}
	if (RowList)
	{
		RowList->RequestListRefresh();
		for (const FRowItemPtr& Item : VisibleRows)
		{
			if (Item->Name == SelectedRow)
			{
				RowList->SetItemSelection(Item, true, ESelectInfo::Direct);
				RowList->RequestScrollIntoView(Item);
			}
		}
	}
}

void SWuwaBulletEditor::SelectRow(FName RowName)
{
	SelectedRow = RowName;
	ReloadEditedRow();
	AutoPickMontage();
	if (RowList)
	{
		for (const FRowItemPtr& Item : VisibleRows)
		{
			if (Item->Name == RowName)
			{
				RowList->SetSelection(Item, ESelectInfo::Direct);
				RowList->RequestScrollIntoView(Item);
			}
		}
	}
}

void SWuwaBulletEditor::ReloadEditedRow()
{
	const uint8* Row = Table && !SelectedRow.IsNone() ? Table->FindRowUnchecked(SelectedRow) : nullptr;
	if (!Row)
	{
		SelectedRow = NAME_None;
		EditedRow.Reset();
	}
	else
	{
		EditedRow = MakeShared<FStructOnScope>(FWuwaBulletInfo::StaticStruct());
		FWuwaBulletInfo::StaticStruct()->CopyScriptStruct(EditedRow->GetStructMemory(), Row);
	}
	if (DetailsView) DetailsView->SetStructureData(EditedRow);
}

void SWuwaBulletEditor::AutoPickMontage()
{
	// 子弹 ID / 1000 = 技能 ID → 技能表里这个技能的第一个蒙太奇；同一技能内切换子弹时不动（可能是手动选的）
	const int64 SkillId = ResolveSkillId(SelectedRow);
	if (!SkillTable || SkillId == 0 || SkillId == AutoPickedSkillId) return;
	AutoPickedSkillId = SkillId;
	const FWuwaSkillInfo* Skill = SkillTable->FindRow<FWuwaSkillInfo>(FName(*LexToString(SkillId)), TEXT("SWuwaBulletEditor"), /*bWarnIfRowMissing*/ false);
	UAnimMontage* Found = Skill && Skill->Montages.Num() > 0 ? Skill->Montages[0].LoadSynchronous() : nullptr;
	if (Found)
	{
		Montage = Found;
		PreviewTime = 0.f;
		bPlaying = false;
	}
}

int64 SWuwaBulletEditor::ResolveSkillId(FName BulletRow) const
{
	const int64 BulletId = WuwaBullet::ParseBulletId(BulletRow);
	if (BulletId <= 0) return 0;
	if (SkillTable)
	{
		for (const int64 Divisor : {1000, 100})
		{
			if (SkillTable->GetRowMap().Contains(FName(*LexToString(BulletId / Divisor)))) return BulletId / Divisor;
		}
	}
	return BulletId / 1000;
}

FName SWuwaBulletEditor::MakeNewRowName() const
{
	const int64 Skill = ResolveSkillId(SelectedRow);
	int64 Max = 0;
	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		if (Skill == 0 || ResolveSkillId(Pair.Key) == Skill) Max = FMath::Max(Max, WuwaBullet::ParseBulletId(Pair.Key));
	}
	int64 Next = Max > 0 ? Max + 1 : Skill * 1000;
	while (Table->GetRowMap().Contains(FName(*LexToString(Next)))) ++Next;
	return FName(*LexToString(Next));
}

// ---- 列表 ----

TSharedRef<ITableRow> SWuwaBulletEditor::OnGenerateRow(FRowItemPtr Item, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(STableRow<FRowItemPtr>, Owner)
		[
			SNew(STextBlock).Text(FText::FromString(Item->Label))
		];
}

void SWuwaBulletEditor::OnRowSelected(FRowItemPtr Item, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo == ESelectInfo::Direct || !Item || Item->Name == SelectedRow) return;
	SelectedRow = Item->Name;
	ReloadEditedRow();
	AutoPickMontage();
}

void SWuwaBulletEditor::OnFilterChanged(const FText& Text)
{
	Filter = Text.ToString();
	RefreshRows();
}

FReply SWuwaBulletEditor::OnAddRow()
{
	const FName Name = MakeNewRowName();
	if (uint8* Row = FDataTableEditorUtils::AddRow(Table, Name))
	{
		FWuwaBulletInfo* Info = reinterpret_cast<FWuwaBulletInfo*>(Row);
		Info->BulletName = TEXT("新子弹");
		Info->Size = FVector(100.f, 100.f, 100.f);
		Info->BornOffset = FVector(100.f, 0.f, 100.f);
		Info->Duration = 0.2f;
		RefreshRows();
		SelectRow(Name);
	}
	return FReply::Handled();
}

FReply SWuwaBulletEditor::OnDuplicateRow()
{
	const FName Name = MakeNewRowName();
	if (FDataTableEditorUtils::DuplicateRow(Table, SelectedRow, Name))
	{
		RefreshRows();
		SelectRow(Name);
	}
	return FReply::Handled();
}

FReply SWuwaBulletEditor::OnDeleteRow()
{
	int32 Index = VisibleRows.IndexOfByPredicate([this](const FRowItemPtr& Item) { return Item->Name == SelectedRow; });
	if (FDataTableEditorUtils::RemoveRow(Table, SelectedRow))
	{
		SelectedRow = NAME_None;
		RefreshRows();
		if (VisibleRows.Num() > 0)
		{
			SelectRow(VisibleRows[FMath::Clamp(Index, 0, VisibleRows.Num() - 1)]->Name);
		}
		else
		{
			ReloadEditedRow();
		}
	}
	return FReply::Handled();
}

FReply SWuwaBulletEditor::OnSave()
{
	if (Table)
	{
		FEditorFileUtils::PromptForCheckoutAndSave({Table->GetPackage()}, /*bCheckDirty*/ false, /*bPromptToSave*/ false);
	}
	return FReply::Handled();
}

// ---- 编辑写回 ----

const FWuwaBulletInfo* SWuwaBulletEditor::GetSelectedBullet() const
{
	return EditedRow ? reinterpret_cast<const FWuwaBulletInfo*>(EditedRow->GetStructMemory()) : nullptr;
}

FWuwaBulletInfo* SWuwaBulletEditor::GetSelectedBulletMutable()
{
	return EditedRow ? reinterpret_cast<FWuwaBulletInfo*>(EditedRow->GetStructMemory()) : nullptr;
}

TArray<TPair<FName, const FWuwaBulletInfo*>> SWuwaBulletEditor::GetSiblingBullets() const
{
	TArray<TPair<FName, const FWuwaBulletInfo*>> Out;
	const int64 Skill = ResolveSkillId(SelectedRow);
	if (!Table || Skill == 0) return Out;
	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		if (Pair.Key != SelectedRow && ResolveSkillId(Pair.Key) == Skill)
		{
			Out.Emplace(Pair.Key, reinterpret_cast<const FWuwaBulletInfo*>(Pair.Value));
		}
	}
	return Out;
}

void SWuwaBulletEditor::CommitEditedRow(const FText& Description)
{
	uint8* Row = Table && EditedRow ? Table->FindRowUnchecked(SelectedRow) : nullptr;
	if (!Row) return;
	if (FWuwaBulletInfo::StaticStruct()->CompareScriptStruct(Row, EditedRow->GetStructMemory(), PPF_None)) return;   // 没变

	const FScopedTransaction Transaction(Description);
	FDataTableEditorUtils::BroadcastPreChange(Table, FDataTableEditorUtils::EDataTableChangeInfo::RowData);
	Table->Modify();
	FWuwaBulletInfo::StaticStruct()->CopyScriptStruct(Row, EditedRow->GetStructMemory());
	Table->HandleDataTableChanged(SelectedRow);
	FDataTableEditorUtils::BroadcastPostChange(Table, FDataTableEditorUtils::EDataTableChangeInfo::RowData);
	Table->MarkPackageDirty();
	RefreshRows();
	// 拖动后让详情面板读一遍新值
	if (DetailsView) DetailsView->SetStructureData(EditedRow);
}

void SWuwaBulletEditor::OnFinishedChangingProperties(const FPropertyChangedEvent& Event)
{
	CommitEditedRow(LOCTEXT("EditBullet", "编辑子弹"));
}

void SWuwaBulletEditor::PostUndo(bool bSuccess)
{
	RefreshRows();
	ReloadEditedRow();
}

void SWuwaBulletEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Table);
	Collector.AddReferencedObject(SkillTable);
	Collector.AddReferencedObject(CharacterClass);
	Collector.AddReferencedObject(Montage);
	Collector.AddReferencedObject(PreviewMesh);
}

// ---- 资产选择 ----

FString SWuwaBulletEditor::GetTablePath() const
{
	return Table ? Table->GetPathName() : FString();
}

void SWuwaBulletEditor::OnTableChanged(const FAssetData& Asset)
{
	SetTable(Cast<UDataTable>(Asset.GetAsset()));
}

bool SWuwaBulletEditor::ShouldFilterTable(const FAssetData& Asset) const
{
	return !WuwaBulletEditor::IsBulletTable(Asset);
}

FString SWuwaBulletEditor::GetMontagePath() const
{
	return Montage ? Montage->GetPathName() : FString();
}

void SWuwaBulletEditor::OnMontageChanged(const FAssetData& Asset)
{
	Montage = Cast<UAnimMontage>(Asset.GetAsset());
	PreviewTime = 0.f;
	bPlaying = false;
}

FString SWuwaBulletEditor::GetPreviewMeshPath() const
{
	return PreviewMesh ? PreviewMesh->GetPathName() : FString();
}

void SWuwaBulletEditor::OnPreviewMeshChanged(const FAssetData& Asset)
{
	PreviewMesh = Cast<USkeletalMesh>(Asset.GetAsset());
	if (Viewport) Viewport->FocusOnCharacter();
}

void SWuwaBulletEditor::OnCharacterClassChanged(const UClass* InClass)
{
	CharacterClass = const_cast<UClass*>(InClass);
	if (Viewport) Viewport->FocusOnCharacter();
}

// ---- 时间轴 ----

void SWuwaBulletEditor::TickPreview(float DeltaSeconds)
{
	const float Length = GetMontageLength();
	if (!bPlaying || Length <= 0.f) return;
	PreviewTime += DeltaSeconds;
	if (PreviewTime > Length) PreviewTime = 0.f;
}

float SWuwaBulletEditor::GetMontageLength() const
{
	return Montage ? Montage->GetPlayLength() : 0.f;
}

FText SWuwaBulletEditor::GetTimeText() const
{
	return FText::FromString(FString::Printf(TEXT("%.2f / %.2f s"), PreviewTime, GetMontageLength()));
}

FReply SWuwaBulletEditor::OnTogglePlay()
{
	bPlaying = !bPlaying;
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
