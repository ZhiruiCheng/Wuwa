#pragma once

#include "CoreMinimal.h"
#include "EditorViewportClient.h"
#include "SEditorViewport.h"

class FAdvancedPreviewScene;
class FColoredMaterialRenderProxy;
class SWuwaBulletEditor;
class UAnimMontage;
class USkeletalMesh;
class USkeletalMeshComponent;
struct FWuwaBulletBornContext;
struct FWuwaBulletInfo;

/**
 * 子弹编辑器的视口：预览角色（角色蓝图的网格、相对变换、胶囊半高）按蒙太奇的某个时间摆姿势，
 * 发射者 = 胶囊中心（可选叠加蒙太奇根运动）；选中的子弹画成高亮实体 + 线框，同技能的其它子弹画淡色线框。
 * 变换控件：平移改出生偏移（BornOffset），旋转改初始旋转（BornRotation），缩放改尺寸（Size）。
 */
class FWuwaBulletViewportClient : public FEditorViewportClient
{
public:
	FWuwaBulletViewportClient(const TSharedRef<FAdvancedPreviewScene>& InScene, const TSharedRef<SEditorViewport>& InViewport, TWeakPtr<SWuwaBulletEditor> InEditor);
	virtual ~FWuwaBulletViewportClient() override;

	virtual void Tick(float DeltaSeconds) override;
	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;

	virtual bool InputWidgetDelta(FViewport* InViewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale) override;
	virtual void TrackingStarted(const FInputEventState& InInputState, bool bIsDraggingWidget, bool bNudge) override;
	virtual void TrackingStopped() override;
	virtual UE::Widget::EWidgetMode GetWidgetMode() const override;
	virtual void SetWidgetMode(UE::Widget::EWidgetMode NewMode) override { WidgetMode = NewMode; }
	virtual bool CanSetWidgetMode(UE::Widget::EWidgetMode NewMode) const override;
	virtual bool CanCycleWidgetMode() const override { return true; }
	virtual FVector GetWidgetLocation() const override;
	virtual FMatrix GetWidgetCoordSystem() const override;
	virtual ECoordSystem GetWidgetCoordSystemSpace() const override { return CoordSystem; }
	virtual void SetWidgetCoordSystemSpace(ECoordSystem NewCoordSystem) override { CoordSystem = NewCoordSystem; }

	/** 把相机对准预览角色。 */
	void FocusOnCharacter();
	/** 视口左上角的说明文字（选中子弹的形状、出生方式、预览时的近似 / 警告）。用 Slate 画，画布字体没有中文。 */
	FText GetInfoText() const;
	USkeletalMeshComponent* GetPreviewMesh() const { return Mesh; }
	const FTransform& GetAttacker() const { return Attacker; }

private:
	/** 预览角色换了就重建网格组件；每帧按蒙太奇时间摆姿势、算发射者变换。 */
	void UpdatePreview();
	void FocusNow();
	FWuwaBulletBornContext MakeContext(const FWuwaBulletInfo& Info) const;
	/** 出生偏移所在的坐标系（平移控件按它的轴走）。 */
	FTransform GetOffsetFrame(const FWuwaBulletInfo& Info, const FWuwaBulletBornContext& Context) const;

	TSharedRef<FAdvancedPreviewScene> Scene;
	TWeakPtr<SWuwaBulletEditor> Editor;

	TObjectPtr<USkeletalMeshComponent> Mesh;
	TWeakObjectPtr<UClass> MeshClass;
	TWeakObjectPtr<USkeletalMesh> MeshOverride;
	TWeakObjectPtr<UAnimMontage> MeshMontage;
	FTransform MeshRelative = FTransform::Identity;
	float CapsuleHalfHeight = 90.f;
	float CapsuleRadius = 30.f;
	/** 当前预览时间的发射者变换（胶囊中心）。 */
	FTransform Attacker = FTransform::Identity;

	UE::Widget::EWidgetMode WidgetMode = UE::Widget::WM_Translate;
	ECoordSystem CoordSystem = COORD_Local;
	bool bDraggingWidget = false;
	bool bFocusPending = true;

	TUniquePtr<FColoredMaterialRenderProxy> SelectedFill;
	TUniquePtr<FColoredMaterialRenderProxy> NoCollisionFill;
};

class SWuwaBulletViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SWuwaBulletViewport) {}
		SLATE_ARGUMENT(TWeakPtr<SWuwaBulletEditor>, Editor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SWuwaBulletViewport() override;

	void FocusOnCharacter() { if (Client) Client->FocusOnCharacter(); }
	TSharedPtr<FWuwaBulletViewportClient> GetBulletClient() const { return Client; }

protected:
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual void PopulateViewportOverlays(TSharedRef<SOverlay> Overlay) override;
	virtual void OnFocusViewportToSelection() override { FocusOnCharacter(); }

private:
	TWeakPtr<SWuwaBulletEditor> Editor;
	TSharedPtr<FAdvancedPreviewScene> Scene;
	TSharedPtr<FWuwaBulletViewportClient> Client;
};
