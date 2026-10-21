#include "BulletEditor/SWuwaBulletViewport.h"

#include "AdvancedPreviewScene.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BulletEditor/SWuwaBulletEditor.h"
#include "BulletEditor/WuwaBulletShapeDraw.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "Game/NewWorld/Bullet/WuwaBulletTypes.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"
#include "PrimitiveDrawingUtils.h"
#include "SceneManagement.h"
#include "UnrealWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WuwaBulletEditor"

namespace WuwaBulletViewport
{
	/** 技能目标（BornStandard = SkillTarget）在预览里放在发射者正前方多远。 */
	constexpr float SkillTargetDistance = 300.f;

	const TCHAR* StandardName(EWuwaBulletBornStandard Standard)
	{
		switch (Standard)
		{
		case EWuwaBulletBornStandard::Attacker: return TEXT("发射者");
		case EWuwaBulletBornStandard::SkillTarget: return TEXT("技能目标");
		case EWuwaBulletBornStandard::World: return TEXT("世界原点");
		case EWuwaBulletBornStandard::Parent: return TEXT("父子弹");
		default: return TEXT("?");
		}
	}

	const TCHAR* FollowName(EWuwaBulletFollowType Follow)
	{
		switch (Follow)
		{
		case EWuwaBulletFollowType::FollowBone: return TEXT("挂到骨骼（保持世界位置）");
		case EWuwaBulletFollowType::Fixed: return TEXT("固定");
		case EWuwaBulletFollowType::SnapToBone: return TEXT("吸附到骨骼（骨骼空间偏移）");
		default: return TEXT("?");
		}
	}

	/** 缩放控件的增量 → 尺寸：按形状把用得到的分量乘上 (1 + 增量)。 */
	void ApplyScale(FWuwaBulletInfo& Info, const FVector& Delta)
	{
		auto Dominant = [](double A, double B) { return FMath::Abs(A) >= FMath::Abs(B) ? A : B; };
		FVector& S = Info.Size;
		switch (Info.Shape)
		{
		case EWuwaBulletShape::Box:
			S *= FVector::OneVector + Delta;
			break;
		case EWuwaBulletShape::Sphere:
			S.X *= 1.0 + Dominant(Dominant(Delta.X, Delta.Y), Delta.Z);
			break;
		case EWuwaBulletShape::Sector:
			S.X *= 1.0 + Dominant(Delta.X, Delta.Y);
			S.Z *= 1.0 + Delta.Z;
			break;
		case EWuwaBulletShape::Cylinder:
		{
			const double Radial = 1.0 + Dominant(Delta.X, Delta.Y);
			S.X *= Radial;
			S.Y *= Radial;
			S.Z *= 1.0 + Delta.Z;
			break;
		}
		}
		S = S.ComponentMax(FVector::ZeroVector);
	}
}

// ---- FWuwaBulletViewportClient ----

FWuwaBulletViewportClient::FWuwaBulletViewportClient(const TSharedRef<FAdvancedPreviewScene>& InScene, const TSharedRef<SEditorViewport>& InViewport, TWeakPtr<SWuwaBulletEditor> InEditor)
	: FEditorViewportClient(nullptr, &InScene.Get(), InViewport)
	, Scene(InScene)
	, Editor(MoveTemp(InEditor))
{
	SetRealtime(true);
	DrawHelper.bDrawGrid = true;
	DrawHelper.bDrawPivot = false;
	EngineShowFlags.SetSelectionOutline(false);
	SetViewLocation(FVector(420.f, -420.f, 260.f));
	SetViewRotation(FRotator(-18.f, 135.f, 0.f));
	if (Widget) Widget->SetSnapEnabled(true);

	// 半透明实体用关卡编辑器画体积（Shaded Volume）的 GeomMaterial，颜色的 alpha = 不透明度
	if (GEngine && GEngine->GeomMaterial)
	{
		SelectedFill = MakeUnique<FColoredMaterialRenderProxy>(GEngine->GeomMaterial->GetRenderProxy(), FLinearColor(1.f, 0.4f, 0.05f, 0.25f));
		NoCollisionFill = MakeUnique<FColoredMaterialRenderProxy>(GEngine->GeomMaterial->GetRenderProxy(), FLinearColor(0.5f, 0.5f, 0.5f, 0.15f));
	}
}

FWuwaBulletViewportClient::~FWuwaBulletViewportClient()
{
	// 材质代理可能还在渲染线程上用
	FlushRenderingCommands();
}

void FWuwaBulletViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);
	if (TSharedPtr<SWuwaBulletEditor> Pinned = Editor.Pin())
	{
		Pinned->TickPreview(DeltaSeconds);
	}
	UpdatePreview();
	if (UWorld* World = Scene->GetWorld())
	{
		World->Tick(LEVELTICK_All, DeltaSeconds);
	}
	if (bFocusPending)
	{
		bFocusPending = false;
		FocusNow();
	}
}

void FWuwaBulletViewportClient::UpdatePreview()
{
	TSharedPtr<SWuwaBulletEditor> Pinned = Editor.Pin();
	if (!Pinned) return;

	UClass* Class = Pinned->GetCharacterClass();
	USkeletalMesh* Override = Pinned->GetPreviewMesh();
	if (!Mesh || Class != MeshClass.Get() || Override != MeshOverride.Get())
	{
		if (Mesh)
		{
			Scene->RemoveComponent(Mesh);
			Mesh = nullptr;
		}
		MeshClass = Class;
		MeshOverride = Override;
		MeshMontage = nullptr;
		MeshRelative = FTransform(FRotator(0.f, -90.f, 0.f), FVector(0.f, 0.f, -90.f));
		CapsuleHalfHeight = 90.f;
		CapsuleRadius = 30.f;
		USkeletalMesh* Asset = nullptr;
		// 角色蓝图的 CDO：网格资产、网格相对胶囊的变换、胶囊尺寸（和游戏里一致）
		if (const ACharacter* Character = Class ? Cast<ACharacter>(Class->GetDefaultObject()) : nullptr)
		{
			if (const USkeletalMeshComponent* CharacterMesh = Character->GetMesh())
			{
				Asset = CharacterMesh->GetSkeletalMeshAsset();
				MeshRelative = CharacterMesh->GetRelativeTransform();
			}
			if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
			{
				CapsuleHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
				CapsuleRadius = Capsule->GetUnscaledCapsuleRadius();
			}
		}
		if (Override)
		{
			if (!Class && Override->GetBounds().BoxExtent.Z > 0.0)
			{
				// 没有角色蓝图：胶囊按网格包围盒估计（脚底在网格原点，半高 = 包围盒高度一半）
				const FBox Bounds = Override->GetBounds().GetBox();
				CapsuleHalfHeight = FMath::Max(Bounds.Max.Z * 0.5f, 20.f);
				CapsuleRadius = FMath::Clamp(CapsuleHalfHeight * 0.4f, 20.f, 200.f);
				MeshRelative.SetLocation(FVector(0.f, 0.f, -CapsuleHalfHeight));
			}
			Asset = Override;
		}
		Mesh = NewObject<USkeletalMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
		Mesh->SetSkeletalMeshAsset(Asset);
		Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Mesh->SetUpdateAnimationInEditor(true);
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Scene->AddComponent(Mesh, MeshRelative);
	}

	UAnimMontage* Montage = Pinned->GetMontage();
	if (Montage && Mesh->GetSkeletalMeshAsset() && Montage->GetSkeleton() != Mesh->GetSkeletalMeshAsset()->GetSkeleton())
	{
		Montage = nullptr;   // 骨架不匹配（换了角色还没换蒙太奇）：参考姿势
	}
	if (Montage != MeshMontage.Get())
	{
		MeshMontage = Montage;
		Mesh->SetAnimation(Montage);
		Mesh->Stop();
		// 根运动从姿势里抽出来（根骨骼锁在原点），由下面按蒙太奇根运动移动发射者，和游戏里胶囊被根运动带着走一样
		if (UAnimInstance* Instance = Mesh->GetAnimInstance())
		{
			Instance->SetRootMotionMode(ERootMotionMode::RootMotionFromEverything);
		}
	}
	const float Time = Pinned->GetPreviewTime();
	if (Montage)
	{
		Mesh->SetPosition(Time, false);
	}

	// 发射者 = 胶囊中心，脚底在地面（Z = 0）
	const FVector Start(0.f, 0.f, CapsuleHalfHeight);
	Attacker = FTransform(Start);
	if (Montage && Pinned->ShouldApplyRootMotion() && Montage->HasRootMotion())
	{
		// 蒙太奇根运动在网格空间，按网格相对胶囊的朝向转到角色空间（≈ CharacterMovement 的 ConvertLocalRootMotionToWorld）
		const FTransform RootMotion = Montage->ExtractRootMotionFromTrackRange(0.f, Time, FAnimExtractContext());
		const FQuat MeshRotation = MeshRelative.GetRotation();
		Attacker = FTransform(MeshRotation * RootMotion.GetRotation() * MeshRotation.Inverse(), Start + MeshRotation.RotateVector(RootMotion.GetTranslation()));
	}
	Mesh->SetWorldTransform(MeshRelative * Attacker);
}

FWuwaBulletBornContext FWuwaBulletViewportClient::MakeContext(const FWuwaBulletInfo& Info) const
{
	FWuwaBulletBornContext Context;
	Context.Attacker = Attacker;
	Context.CapsuleHalfHeight = CapsuleHalfHeight;
	if (!Info.BoneName.IsNone() && Mesh && Mesh->DoesSocketExist(Info.BoneName))
	{
		Context.Bone = Mesh->GetSocketTransform(Info.BoneName, RTS_World);
	}
	Context.SkillTarget = FTransform(Attacker.GetRotation(), Attacker.TransformPosition(FVector(WuwaBulletViewport::SkillTargetDistance, 0.f, 0.f)));
	return Context;
}

FTransform FWuwaBulletViewportClient::GetOffsetFrame(const FWuwaBulletInfo& Info, const FWuwaBulletBornContext& Context) const
{
	// BornOffset 所在的坐标系，与运行时同一套计算
	return WuwaBullet::ComputeBornBase(Info, Context);
}

void FWuwaBulletViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FEditorViewportClient::Draw(View, PDI);
	TSharedPtr<SWuwaBulletEditor> Pinned = Editor.Pin();
	if (!Pinned) return;
	const ERHIFeatureLevel::Type FeatureLevel = View->GetFeatureLevel();

	// 发射者：胶囊 + 朝向箭头（脚底）
	const FQuat Rotation = Attacker.GetRotation();
	DrawWireCapsule(PDI, Attacker.GetLocation(), Rotation.GetAxisX(), Rotation.GetAxisY(), Rotation.GetAxisZ(), FLinearColor(0.3f, 0.6f, 1.f, 0.6f),
		CapsuleRadius, CapsuleHalfHeight, 16, SDPG_World);
	DrawDirectionalArrow(PDI, FRotationTranslationMatrix(Attacker.Rotator(), Attacker.TransformPosition(FVector(0.f, 0.f, -CapsuleHalfHeight + 2.f))),
		FLinearColor(0.3f, 0.6f, 1.f), CapsuleRadius + 60.f, 10.f, SDPG_World, 1.5f);

	const FWuwaBulletInfo* Selected = Pinned->GetSelectedBullet();
	TOptional<FTransform> SelectedShape;
	if (Selected)
	{
		SelectedShape = WuwaBullet::ComputeShapeTransform(*Selected, WuwaBullet::ComputeBornTransform(*Selected, MakeContext(*Selected)));
	}

	if (Pinned->ShouldShowSiblings())
	{
		for (const TPair<FName, const FWuwaBulletInfo*>& Sibling : Pinned->GetSiblingBullets())
		{
			const FWuwaBulletInfo& Info = *Sibling.Value;
			const FTransform Shape = WuwaBullet::ComputeShapeTransform(Info, WuwaBullet::ComputeBornTransform(Info, MakeContext(Info)));
			// 和选中的完全重合（同一判定体的多段子弹很常见）就不画，免得盖住高亮
			if (Selected && Info.Shape == Selected->Shape && Info.Size.Equals(Selected->Size, 0.5) && Shape.Equals(*SelectedShape, 0.5)) continue;
			WuwaBulletDraw::DrawShape(PDI, FeatureLevel, Info, Shape, FLinearColor(0.35f, 0.75f, 1.f, 0.18f), 0.f, nullptr);
		}
	}

	if (const FWuwaBulletInfo* Info = Selected)
	{
		const FWuwaBulletBornContext Context = MakeContext(*Info);
		const FTransform Born = WuwaBullet::ComputeBornTransform(*Info, Context);
		const bool bCollision = WuwaBullet::HasCollision(*Info);
		WuwaBulletDraw::DrawShape(PDI, FeatureLevel, *Info, *SelectedShape,
			bCollision ? FLinearColor(1.f, 0.5f, 0.05f) : FLinearColor(0.6f, 0.6f, 0.6f), 2.5f, bCollision ? SelectedFill.Get() : NoCollisionFill.Get(), /*DepthBias*/ 2.f);
		// 出生偏移的起点 → 子弹原点，子弹自身坐标轴
		const FTransform Frame = GetOffsetFrame(*Info, Context);
		DrawDashedLine(PDI, Frame.GetLocation(), Born.GetLocation(), FLinearColor::Yellow, 6.f, SDPG_Foreground);
		DrawCoordinateSystem(PDI, Born.GetLocation(), Born.Rotator(), 30.f, SDPG_Foreground, 1.f);
		if (Info->BornStandard == EWuwaBulletBornStandard::SkillTarget && Context.SkillTarget.IsSet())
		{
			DrawWireSphere(PDI, Context.SkillTarget->GetLocation(), FLinearColor::Red, 25.f, 16, SDPG_World, 1.f);
		}
	}
}

FText FWuwaBulletViewportClient::GetInfoText() const
{
	TSharedPtr<SWuwaBulletEditor> Pinned = Editor.Pin();
	const FWuwaBulletInfo* Info = Pinned ? Pinned->GetSelectedBullet() : nullptr;
	if (!Info) return LOCTEXT("NoBullet", "选一张子弹表，再在左边选一颗子弹");

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("%s  %s"), *Pinned->GetSelectedRowName().ToString(), *Info->BulletName));
	Lines.Add(WuwaBulletDraw::DescribeSize(*Info));
	FString Born = FString::Printf(TEXT("出生：%s"), WuwaBulletViewport::StandardName(Info->BornStandard));
	if (!Info->BoneName.IsNone()) Born += FString::Printf(TEXT("  骨骼 %s"), *Info->BoneName.ToString());
	Born += FString::Printf(TEXT("  跟随：%s"), WuwaBulletViewport::FollowName(Info->FollowType));
	Lines.Add(Born);
	Lines.Add(FString::Printf(TEXT("持续 %.2fs  判定延迟 %.2fs  判定时长 %.2fs  伤害 ID %lld"), Info->Duration, Info->CollisionDelay, Info->CollisionDuration, Info->DamageId));

	if (!WuwaBullet::HasCollision(*Info)) Lines.Add(TEXT("· 尺寸 ≤ 0：不开碰撞（一般是只用来生成子子弹的父子弹）"));
	if (!Info->BoneName.IsNone() && !(Mesh && Mesh->DoesSocketExist(Info->BoneName)))
	{
		Lines.Add(FString::Printf(TEXT("· 预览网格上没有骨骼 %s（武器可能是单独的网格），按没配骨骼算"), *Info->BoneName.ToString()));
	}
	if (Info->BornStandard == EWuwaBulletBornStandard::Parent) Lines.Add(TEXT("· 父子弹的位置要运行时才知道，预览按发射者算"));
	if (Info->BornStandard == EWuwaBulletBornStandard::SkillTarget) Lines.Add(FString::Printf(TEXT("· 技能目标放在正前方 %.0f（红球）"), WuwaBulletViewport::SkillTargetDistance));
	if (!Pinned->GetMontage()) Lines.Add(TEXT("· 没有蒙太奇（技能表里没配），角色是参考姿势"));
	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

void FWuwaBulletViewportClient::FocusOnCharacter()
{
	// 预览网格 / 蒙太奇要等下一帧 Tick 才换好，那时再取景
	bFocusPending = true;
}

void FWuwaBulletViewportClient::FocusNow()
{
	// 角色胶囊 + 选中子弹的包围球
	const FVector Center = Attacker.GetLocation();
	FBox Box(Center - FVector(CapsuleRadius + 40.f, CapsuleRadius + 40.f, CapsuleHalfHeight), Center + FVector(CapsuleRadius + 40.f, CapsuleRadius + 40.f, CapsuleHalfHeight));
	const TSharedPtr<SWuwaBulletEditor> Pinned = Editor.Pin();
	if (const FWuwaBulletInfo* Info = Pinned ? Pinned->GetSelectedBullet() : nullptr)
	{
		const FVector Shape = WuwaBullet::ComputeShapeTransform(*Info, WuwaBullet::ComputeBornTransform(*Info, MakeContext(*Info))).GetLocation();
		const double Radius = FMath::Min(WuwaBulletDraw::GetBoundingRadius(*Info), 3000.0);
		Box += FBox(Shape - FVector(Radius), Shape + FVector(Radius));
	}
	FocusViewportOnBox(Box);
}

// ---- 变换控件 ----

UE::Widget::EWidgetMode FWuwaBulletViewportClient::GetWidgetMode() const
{
	const TSharedPtr<SWuwaBulletEditor> Pinned = Editor.Pin();
	return Pinned && Pinned->GetSelectedBullet() ? WidgetMode : UE::Widget::WM_None;
}

bool FWuwaBulletViewportClient::CanSetWidgetMode(UE::Widget::EWidgetMode NewMode) const
{
	return NewMode == UE::Widget::WM_Translate || NewMode == UE::Widget::WM_Rotate || NewMode == UE::Widget::WM_Scale;
}

FVector FWuwaBulletViewportClient::GetWidgetLocation() const
{
	const TSharedPtr<SWuwaBulletEditor> Pinned = Editor.Pin();
	const FWuwaBulletInfo* Info = Pinned ? Pinned->GetSelectedBullet() : nullptr;
	return Info ? WuwaBullet::ComputeBornTransform(*Info, MakeContext(*Info)).GetLocation() : FVector::ZeroVector;
}

FMatrix FWuwaBulletViewportClient::GetWidgetCoordSystem() const
{
	const TSharedPtr<SWuwaBulletEditor> Pinned = Editor.Pin();
	const FWuwaBulletInfo* Info = Pinned ? Pinned->GetSelectedBullet() : nullptr;
	if (!Info || CoordSystem == COORD_World) return FMatrix::Identity;
	const FWuwaBulletBornContext Context = MakeContext(*Info);
	// 平移：沿出生偏移所在坐标系的轴（拖 X 就是改 BornOffset.X）；旋转 / 缩放：沿子弹自身的轴
	const FQuat Rotation = WidgetMode == UE::Widget::WM_Translate ? GetOffsetFrame(*Info, Context).GetRotation()
		: WuwaBullet::ComputeBornTransform(*Info, Context).GetRotation();
	return FQuatRotationMatrix(Rotation);
}

bool FWuwaBulletViewportClient::InputWidgetDelta(FViewport* InViewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale)
{
	TSharedPtr<SWuwaBulletEditor> Pinned = Editor.Pin();
	FWuwaBulletInfo* Info = Pinned ? Pinned->GetSelectedBulletMutable() : nullptr;
	if (!Info || CurrentAxis == EAxisList::None) return false;

	const FWuwaBulletBornContext Context = MakeContext(*Info);
	if (!Drag.IsZero())
	{
		Info->BornOffset += GetOffsetFrame(*Info, Context).InverseTransformVector(Drag);
	}
	if (!Rot.IsZero())
	{
		// 世界里转 Rot 之后的朝向，换回相对基准朝向的 BornRotation
		const FQuat World = WuwaBullet::ComputeBornTransform(*Info, Context).GetRotation();
		const FQuat Base = World * Info->BornRotation.Quaternion().Inverse();
		Info->BornRotation = (Base.Inverse() * Rot.Quaternion() * World).Rotator();
	}
	if (!Scale.IsZero())
	{
		WuwaBulletViewport::ApplyScale(*Info, Scale);
	}
	return true;
}

void FWuwaBulletViewportClient::TrackingStarted(const FInputEventState& InInputState, bool bIsDraggingWidget, bool bNudge)
{
	FEditorViewportClient::TrackingStarted(InInputState, bIsDraggingWidget, bNudge);
	bDraggingWidget = bIsDraggingWidget;
}

void FWuwaBulletViewportClient::TrackingStopped()
{
	FEditorViewportClient::TrackingStopped();
	if (bDraggingWidget)
	{
		bDraggingWidget = false;
		if (TSharedPtr<SWuwaBulletEditor> Pinned = Editor.Pin())
		{
			Pinned->CommitEditedRow(LOCTEXT("DragBullet", "拖动子弹"));
		}
	}
}

// ---- SWuwaBulletViewport ----

void SWuwaBulletViewport::Construct(const FArguments& InArgs)
{
	Editor = InArgs._Editor;
	Scene = MakeShared<FAdvancedPreviewScene>(FPreviewScene::ConstructionValues());
	SEditorViewport::Construct(SEditorViewport::FArguments());
}

SWuwaBulletViewport::~SWuwaBulletViewport()
{
	if (Client)
	{
		Client->Viewport = nullptr;
	}
}

TSharedRef<FEditorViewportClient> SWuwaBulletViewport::MakeEditorViewportClient()
{
	Client = MakeShared<FWuwaBulletViewportClient>(Scene.ToSharedRef(), SharedThis(this), Editor);
	return Client.ToSharedRef();
}

void SWuwaBulletViewport::PopulateViewportOverlays(TSharedRef<SOverlay> Overlay)
{
	SEditorViewport::PopulateViewportOverlays(Overlay);
	Overlay->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(10.f)
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return Client ? Client->GetInfoText() : FText::GetEmpty(); })
			.ShadowOffset(FVector2D(1.f, 1.f))
		];
	Overlay->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Bottom)
		.Padding(10.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Hint", "W 平移出生偏移 · E 旋转初始旋转 · R 缩放尺寸 · F 对准角色 · 右键拖动看四周"))
			.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.6f))
			.ShadowOffset(FVector2D(1.f, 1.f))
		];
}

#undef LOCTEXT_NAMESPACE
