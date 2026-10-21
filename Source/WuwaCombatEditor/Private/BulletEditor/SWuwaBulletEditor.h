#pragma once

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "UObject/GCObject.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class IStructureDetailsView;
class SWuwaBulletViewport;
class UAnimMontage;
class USkeletalMesh;
class UDataTable;
struct FAssetData;
struct FPropertyChangedEvent;
struct FWuwaBulletInfo;
class FStructOnScope;

/**
 * 子弹编辑器（Window ▸ 子弹编辑器，或在子弹表上右键 ▸ 在子弹编辑器中打开）。
 * 左边是表里的子弹列表；中间视口按技能表找到的蒙太奇摆好预览角色的姿势，把子弹判定体画在出生位置上，
 * 可以用 W / E / R 拖动出生偏移、旋转、尺寸；右边是这一行的全部字段。改动直接写回子弹表（可撤销），点"保存"存盘。
 * 出生位置的算法和运行时共用 WuwaBullet::ComputeBornTransform。
 */
class SWuwaBulletEditor : public SCompoundWidget, public FSelfRegisteringEditorUndoClient, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SWuwaBulletEditor) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SWuwaBulletEditor() override;

	/** 打开一张子弹表（行结构必须是 FWuwaBulletInfo），并自动找同目录的技能表和角色蓝图。 */
	void SetTable(UDataTable* InTable);
	/** 选中一行（列表、详情、视口一起切）。 */
	void SelectRow(FName RowName);
	UDataTable* GetTable() const { return Table; }
	UDataTable* GetSkillTable() const { return SkillTable; }
	TSharedPtr<SWuwaBulletViewport> GetViewport() const { return Viewport; }

	// ---- 给视口用 ----
	const FWuwaBulletInfo* GetSelectedBullet() const;
	FWuwaBulletInfo* GetSelectedBulletMutable();
	FName GetSelectedRowName() const { return SelectedRow; }
	/** 同一技能的其它子弹（行名、数据），预览时用淡色一起画。 */
	TArray<TPair<FName, const FWuwaBulletInfo*>> GetSiblingBullets() const;
	UClass* GetCharacterClass() const { return CharacterClass; }
	/** 预览网格：不为空时代替角色蓝图里的网格（没有角色蓝图的怪物 / Boss 只有网格）。 */
	USkeletalMesh* GetPreviewMesh() const { return PreviewMesh; }
	UAnimMontage* GetMontage() const { return Montage; }
	float GetPreviewTime() const { return PreviewTime; }
	void SetPreviewTime(float InTime) { PreviewTime = InTime; bPlaying = false; }
	bool ShouldApplyRootMotion() const { return bApplyRootMotion; }
	bool ShouldShowSiblings() const { return bShowSiblings; }
	/** 视口每帧调用：播放时推进预览时间。 */
	void TickPreview(float DeltaSeconds);
	/** 视口拖动开始时调用（记录撤销点）；拖动中直接改选中行的编辑副本；结束时调用 CommitEditedRow 写回表。 */
	void CommitEditedRow(const FText& Description);

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SWuwaBulletEditor"); }

	// FEditorUndoClient
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }

private:
	struct FRowItem
	{
		FName Name;
		FString Label;
	};
	using FRowItemPtr = TSharedPtr<FRowItem>;

	TSharedRef<SWidget> MakeToolbar();
	TSharedRef<SWidget> MakeRowPanel();
	TSharedRef<SWidget> MakeTimeBar();

	void RefreshRows();
	void ReloadEditedRow();
	void AutoPickMontage();
	void FindCompanions();
	FName MakeNewRowName() const;
	/**
	 * 子弹 ID → 技能 ID：角色是 技能 ID × 1000 + 序号（1205001000 → 1205001），芙露德莉丝是 × 100（8002500101 → 80025001）。
	 * 有技能表时取表里存在的那个前缀，没有时按 × 1000。
	 */
	int64 ResolveSkillId(FName BulletRow) const;

	// 列表
	TSharedRef<ITableRow> OnGenerateRow(FRowItemPtr Item, const TSharedRef<STableViewBase>& Owner);
	void OnRowSelected(FRowItemPtr Item, ESelectInfo::Type SelectInfo);
	void OnFilterChanged(const FText& Text);
	FReply OnAddRow();
	FReply OnDuplicateRow();
	FReply OnDeleteRow();
	FReply OnSave();

	// 详情
	void OnFinishedChangingProperties(const FPropertyChangedEvent& Event);

	// 资产选择
	FString GetTablePath() const;
	void OnTableChanged(const FAssetData& Asset);
	bool ShouldFilterTable(const FAssetData& Asset) const;
	FString GetMontagePath() const;
	void OnMontageChanged(const FAssetData& Asset);
	const UClass* GetSelectedCharacterClass() const { return CharacterClass; }
	void OnCharacterClassChanged(const UClass* InClass);
	FString GetPreviewMeshPath() const;
	void OnPreviewMeshChanged(const FAssetData& Asset);

	// 时间轴
	float GetMontageLength() const;
	FText GetTimeText() const;
	FReply OnTogglePlay();

	TObjectPtr<UDataTable> Table;
	TObjectPtr<UDataTable> SkillTable;
	TObjectPtr<UClass> CharacterClass;
	TObjectPtr<UAnimMontage> Montage;
	TObjectPtr<USkeletalMesh> PreviewMesh;

	TArray<FRowItemPtr> AllRows;
	TArray<FRowItemPtr> VisibleRows;
	FString Filter;
	FName SelectedRow;
	/** 上次按技能表自动选蒙太奇时的技能 ID：同一技能内切换子弹不再自动换（可能是手动选的）。 */
	int64 AutoPickedSkillId = 0;
	/** 选中行的编辑副本：详情面板和视口拖动都改它，改完写回表。 */
	TSharedPtr<FStructOnScope> EditedRow;

	float PreviewTime = 0.f;
	bool bPlaying = false;
	bool bApplyRootMotion = true;
	bool bShowSiblings = true;

	TSharedPtr<SListView<FRowItemPtr>> RowList;
	TSharedPtr<IStructureDetailsView> DetailsView;
	TSharedPtr<SWuwaBulletViewport> Viewport;
};
