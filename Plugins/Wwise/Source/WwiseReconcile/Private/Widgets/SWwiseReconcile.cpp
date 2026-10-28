/*******************************************************************************
The content of this file includes portions of the proprietary AUDIOKINETIC Wwise
Technology released in source code form as part of the game integration package.
The content of this file may not be used without valid licenses to the
AUDIOKINETIC Wwise Technology.
Note that the use of the game engine is subject to the Unreal(R) Engine End User
License Agreement at https://www.unrealengine.com/en-US/eula/unreal

License Usage

Licensees holding valid licenses to the AUDIOKINETIC Wwise Technology may use
this file in accordance with the end user license agreement provided with the
software or, alternatively, in accordance with the terms contained
in a written agreement between you and Audiokinetic Inc.
Copyright (c) 2026 Audiokinetic Inc.
*******************************************************************************/

#include "Widgets/SWwiseReconcile.h"

#include "AkAudioStyle.h"
#include "Widgets/ProjectedResultColumn.h"
#include "Widgets/ReconcileOperationColumn.h"
#include "Widgets/ReconcileUEAssetStatusColumn.h"
#include "Widgets/SWwiseReconcileItemView.h"
#include "Widgets/WwiseReconcileObjectColumn.h"
#include "Wwise/WwiseReconcile.h"
#include "WwiseUEFeatures.h"

#include "Editor.h"
#include "Interfaces/IMainFrameModule.h"
#include "ObjectTools.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "AkAudio"

void SWwiseReconcile::Construct(const FArguments& InArgs, const TArray< FWwiseReconcileItem >& InReconcileItems, TSharedRef<SWindow>& ReconcileWindow)
{
	Window = ReconcileWindow;
	
	bOnlyReferencedAssets = true;
	for(auto& Item : InReconcileItems)
	{
		TSharedPtr<FWwiseReconcileItem> ReconcileItem = MakeShared<FWwiseReconcileItem>(Item);
		ReconcileItems.Add(ReconcileItem);

		if (bOnlyReferencedAssets && !ReconcileItem->IsAssetReferenced())
		{
			bOnlyReferencedAssets = false;
		}
	}

	HeaderRowWidget =
		SNew(SHeaderRow)
		.CanSelectGeneratedColumn(true);

	SetupColumns(*HeaderRowWidget);

	ChildSlot
	[
		SNew(SBorder)
		.Padding(4)
		.BorderImage(FAkAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			[
				// Description
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 10.f, 0.f, 10.f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Top)
				[
					SNew(SHorizontalBox)
					.Visibility(EVisibility::Visible)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Top)
					.Padding(0.f, 0.f, 2.f, 0.f)
					[
						SNew(SImage)
						.Image(FAkAppStyle::Get().GetBrush("Icons.Warning"))
						.ColorAndOpacity(FSlateColor(FColor::Yellow))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Top)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ReconcileWarning", "Make sure the SoundBanks are properly generated before doing the Reconciliation process."))
						.ColorAndOpacity(FSlateColor(FColor::Yellow))
					]
				]
				// List
				+ SVerticalBox::Slot()
				.FillHeight(1.f)
				[
					SAssignNew(ReconcileList, SWwiseReconcileListView, StaticCastSharedRef<SWwiseReconcile>(AsShared()))
					.ListItemsSource(&ReconcileItems)
					.OnGenerateRow(this, &SWwiseReconcile::GenerateRow)
					.ClearSelectionOnClick(false)
					.SelectionMode(ESelectionMode::None)
					.HeaderRow(HeaderRowWidget)
					.OnMouseButtonDoubleClick(this, &SWwiseReconcile::OnReconcileItemDoubleClicked)
				]
				+ SVerticalBox::Slot()
				.Padding(0.f, 4.f, 0.f, 4.f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoHeight()
				[
					SNew(STextBlock)
					.Visibility(this, &SWwiseReconcile::GetManualActionWarningVisibility)
					.Text(LOCTEXT("ManualActionWarning", "Manual actions are required. Please follow the instruction for each element."))
					.ColorAndOpacity(FSlateColor(FColor::Yellow))
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 4.f, 0.f, 4.f)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.AutoWidth()
					.FillWidth(1.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SAssignNew(ForceDeleteAssetsCheckbox, SCheckBox)
							.OnCheckStateChanged(this, &SWwiseReconcile::OnForceDeleteCheckboxChanged)
							.IsEnabled(this, &SWwiseReconcile::CanInteractWithForceDelete)
							.Content()
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("Force delete referenced assets")))
							]
						]
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 20.0f, 0.0f)
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("( DESTRUCTIVE ACTION )")))
							.ColorAndOpacity(FSlateColor(FColor::Red))
							.Visibility(this, &SWwiseReconcile::GetDestructiveActionWarningVisibility)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Right)
						[
							SNew(SButton)
							.Text(LOCTEXT("ReconcileButton", "Reconcile Unreal Assets"))
							.IsEnabled(this, &SWwiseReconcile::CanReconcileItems)
							.OnClicked(this, &SWwiseReconcile::ReconcileAssets)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Right)
						[
							SNew(SButton)
							.Text(LOCTEXT("CancelButton", "Cancel"))
							.OnClicked(this, &SWwiseReconcile::CloseWindow)
						]
					]
				]
			]
		]
	];
}

SWwiseReconcile::SWwiseReconcile()
{
}

void SWwiseReconcile::SetupColumns(SHeaderRow& HeaderRow)
{
	Columns.Empty();
	HeaderRow.ClearColumns();

	auto WwiseObjectsColumn = MakeShared<FWwiseReconcileObjectColumn>();
	auto OperationColumn = MakeShared<FReconcileOperationColumn>();
	auto UEAssetStatusColumn = MakeShared<FReconcileUEAssetStatusColumn>();
	auto ProjectedResultColumn = MakeShared<FProjectedResultColumn>();

	auto WwiseObjectArgs = WwiseObjectsColumn->ConstructHeaderRowColumn();
	auto OperationArgs = OperationColumn->ConstructHeaderRowColumn();
	auto UEAssetStatusArgs = UEAssetStatusColumn->ConstructHeaderRowColumn();
	auto ProjectedResultsArgs = ProjectedResultColumn->ConstructHeaderRowColumn();

	Columns.Add(WwiseObjectArgs._ColumnId, WwiseObjectsColumn);
	Columns.Add(OperationArgs._ColumnId, OperationColumn);
	Columns.Add(UEAssetStatusArgs._ColumnId, UEAssetStatusColumn);
	Columns.Add(ProjectedResultsArgs._ColumnId, ProjectedResultColumn);

	HeaderRowWidget->AddColumn(WwiseObjectArgs);
	HeaderRowWidget->AddColumn(OperationArgs);
	HeaderRowWidget->AddColumn(UEAssetStatusArgs);
	HeaderRowWidget->AddColumn(ProjectedResultsArgs);
}

bool SWwiseReconcile::CanReconcileItems() const
{
	return !bOnlyReferencedAssets || (ForceDeleteAssetsCheckbox.IsValid() && ForceDeleteAssetsCheckbox->IsChecked());
}

TSharedRef<ITableRow> SWwiseReconcile::GenerateRow(TSharedPtr<FWwiseReconcileItem> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	check(Item.IsValid());

	TSharedRef<SWwiseReconcileRow> NewRow = SNew(SWwiseReconcileRow, ReconcileList.ToSharedRef(), SharedThis(this)).
		Item(Item);

	return NewRow;
}

EVisibility SWwiseReconcile::GetDestructiveActionWarningVisibility() const
{
	return ForceDeleteAssetsCheckbox->IsChecked() ? EVisibility::Visible : EVisibility::Hidden;
}

EVisibility SWwiseReconcile::GetManualActionWarningVisibility() const
{
	return CanReconcileItems() ? EVisibility::Hidden : EVisibility::Visible;
}

FReply SWwiseReconcile::CloseWindow()
{
	Window.Pin()->RequestDestroyWindow();
	return FReply::Handled();
}

void SWwiseReconcile::OnForceDeleteCheckboxChanged(ECheckBoxState newState)
{
	if (newState == ECheckBoxState::Checked)
	{
		EAppReturnType::Type out = FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("ForceDeleteWarning", "THIS IS A DESTRUCTIVE ACTION.\nEnabling this feature will force delete all assets without taking care of missing references.\n\nAre you sure you want to enable the force delete option?"));
		ForceDeleteAssetsCheckbox->SetIsChecked(out == EAppReturnType::Yes);
	}
	else
	{
		ForceDeleteAssetsCheckbox->SetIsChecked(false);
	}
}

void SWwiseReconcile::OnReconcileItemDoubleClicked(TSharedPtr<FWwiseReconcileItem> item)
{
	CloseWindow();
	GEditor->SyncBrowserToObject(item->Asset);
}

FReply SWwiseReconcile::ReconcileAssets()
{
	EWwiseReconcileOperationFlags flags = EWwiseReconcileOperationFlags::None;
	if (ForceDeleteAssetsCheckbox->IsChecked())
	{
		flags |= EWwiseReconcileOperationFlags::ForceDelete;
	}

	CloseWindow();
	IWwiseReconcile::Get()->ReconcileAssets(flags);
	return FReply::Handled();
}