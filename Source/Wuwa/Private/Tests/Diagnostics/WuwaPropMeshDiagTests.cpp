#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "ReferenceSkeleton.h"

// 只读诊断：特效道具（长离的伞）导入后的包围盒、根骨与动画轨道，角色骨架上的道具挂点，查道具朝向问题。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaPropMeshDiagTest, "Diag.Effect.PropMeshAxes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWuwaPropMeshDiagTest::RunTest(const FString& Parameters)
{
	if (const USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr,
		TEXT("/Game/Effects/Changli/Reference/Props/EchanglisanMd10011/EchanglisanMd10011.EchanglisanMd10011")))
	{
		const FBoxSphereBounds Bounds = Mesh->GetImportedBounds();
		AddInfo(FString::Printf(TEXT("Skeletal umbrella: origin %s extent %s"), *Bounds.Origin.ToString(), *Bounds.BoxExtent.ToString()));
		const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
		for (int32 Index = 0; Index < FMath::Min(Ref.GetNum(), 4); ++Index)
		{
			const FTransform& Pose = Ref.GetRefBonePose()[Index];
			AddInfo(FString::Printf(TEXT("  bone %d %s parent %d: loc %s rot %s scale %s"), Index, *Ref.GetBoneName(Index).ToString(),
				Ref.GetParentIndex(Index), *Pose.GetLocation().ToString(), *Pose.Rotator().ToString(), *Pose.GetScale3D().ToString()));
		}
	}
	// 角色骨架上的道具挂点：相对角色根骨 Root 的朝向（和原作 .uemodel 里的同一根骨骼对比），以及同名插槽
	if (const USkeletalMesh* Changli = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Role/changli/NewModel/changli2_0.changli2_0")))
	{
		const FReferenceSkeleton& Ref = Changli->GetRefSkeleton();
		auto Component = [&Ref](int32 Index)
		{
			FTransform T = FTransform::Identity;
			while (Index != INDEX_NONE)
			{
				T = T * Ref.GetRefBonePose()[Index];
				Index = Ref.GetParentIndex(Index);
			}
			return T;
		};
		const int32 Root = Ref.FindBoneIndex(TEXT("Root"));
		for (const TCHAR* Name : { TEXT("WeaponProp02"), TEXT("Bip001RHand") })
		{
			const int32 Index = Ref.FindBoneIndex(Name);
			if (Index == INDEX_NONE || Root == INDEX_NONE) continue;
			FTransform Relative = Component(Index).GetRelativeTransform(Component(Root));
			Relative.RemoveScaling();
			const FMatrix M = Relative.ToMatrixNoScale();
			AddInfo(FString::Printf(TEXT("Changli %s relative to Root: X %s Y %s Z %s pos %s"), Name,
				*M.GetUnitAxis(EAxis::X).ToString(), *M.GetUnitAxis(EAxis::Y).ToString(), *M.GetUnitAxis(EAxis::Z).ToString(),
				*Component(Index).GetRelativeTransform(Component(Root)).GetLocation().ToString()));
		}
		for (const USkeletalMeshSocket* Socket : Changli->GetActiveSocketList())
		{
			if (Socket && Socket->SocketName.ToString().StartsWith(TEXT("WeaponProp")))
				AddInfo(FString::Printf(TEXT("Changli socket %s on bone %s: loc %s rot %s"), *Socket->SocketName.ToString(), *Socket->BoneName.ToString(),
					*Socket->RelativeLocation.ToString(), *Socket->RelativeRotation.ToString()));
		}
	}
	// 导入后的伞动画：包装根骨与 Bone_Prop001 的轨道（和参考姿势对比，看 FBX 往返有没有带进旋转）
	if (const UAnimSequence* Umbrella = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Effects/Changli/Reference/Props/EchanglisanMd10011/EchanglisanMd10011_Anim.EchanglisanMd10011_Anim")))
	{
		const IAnimationDataModel* Model = Umbrella->GetDataModel();
		AddInfo(FString::Printf(TEXT("Umbrella anim: %d frames, %.3fs"), Model->GetNumberOfFrames(), Umbrella->GetPlayLength()));
		for (const TCHAR* Name : { TEXT("EchanglisanMd10011_LOD0_Skeleton"), TEXT("Bone_Prop001") })
		{
			if (!Model->IsValidBoneTrackName(Name)) { AddInfo(FString::Printf(TEXT("Umbrella anim %s: no track"), Name)); continue; }
			for (const int32 Frame : { 0, 60 })
			{
				const FTransform T = Model->EvaluateBoneTrackTransform(Name, FFrameTime(Frame), EAnimInterpolationType::Linear);
				AddInfo(FString::Printf(TEXT("Umbrella anim %s f%d: pos %s rot %s scale %s"), Name, Frame, *T.GetLocation().ToString(), *T.Rotator().ToString(), *T.GetScale3D().ToString()));
			}
		}
	}
	// 项目 Attack04 动画里道具挂点骨骼的轨道（原作 Attack04 会大幅移动 / 旋转 WeaponProp02）
	if (const UAnimSequence* Anim = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Characters/Role/changli/NewModel/changli2_0R2T1ChangLiMd10011_LOD0_Skeleton_Attack04.changli2_0R2T1ChangLiMd10011_LOD0_Skeleton_Attack04")))
	{
		const IAnimationDataModel* Model = Anim->GetDataModel();
		// 动画第 60 帧时 WeaponProp02 相对 Root 的朝向（整条骨链按动画轨道求，没有轨道的骨骼用参考姿势）
		if (const USkeletalMesh* Changli = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Role/changli/NewModel/changli2_0.changli2_0")))
		{
			const FReferenceSkeleton& Ref = Changli->GetRefSkeleton();
			for (const int32 Frame : { 45, 60, 90 })
			{
				auto Local = [&](int32 Index)
				{
					const FName Bone = Ref.GetBoneName(Index);
					return Model->IsValidBoneTrackName(Bone) ? Model->EvaluateBoneTrackTransform(Bone, FFrameTime(Frame), EAnimInterpolationType::Linear) : Ref.GetRefBonePose()[Index];
				};
				auto Component = [&](int32 Index)
				{
					FTransform T = FTransform::Identity;
					for (; Index != INDEX_NONE; Index = Ref.GetParentIndex(Index)) T = T * Local(Index);
					return T;
				};
				const int32 Root = Ref.FindBoneIndex(TEXT("Root")), Prop = Ref.FindBoneIndex(TEXT("WeaponProp02"));
				if (Root == INDEX_NONE || Prop == INDEX_NONE) break;
				FTransform Relative = Component(Prop).GetRelativeTransform(Component(Root));
				Relative.RemoveScaling();
				const FMatrix M = Relative.ToMatrixNoScale();
				AddInfo(FString::Printf(TEXT("Attack04 f%d WeaponProp02 vs Root: X %s Z %s"), Frame, *M.GetUnitAxis(EAxis::X).ToString(), *M.GetUnitAxis(EAxis::Z).ToString()));
			}
		}
		for (const TCHAR* Name : { TEXT("WeaponProp02"), TEXT("Bip001RHand") })
		{
			if (!Model->IsValidBoneTrackName(Name)) { AddInfo(FString::Printf(TEXT("Attack04 %s: no track"), Name)); continue; }
			for (const int32 Frame : { 0, 12, 24, 36, 48, 60, 100 })
			{
				const FTransform T = Model->EvaluateBoneTrackTransform(Name, FFrameTime(Frame), EAnimInterpolationType::Linear);
				AddInfo(FString::Printf(TEXT("Attack04 %s f%d: pos %s quat %s"), Name, Frame, *T.GetLocation().ToString(), *T.GetRotation().ToString()));
			}
		}
	}
	if (const UStaticMesh* Static = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/Effects/Changli/Reference/Mesh/Mod_Changli_Umbrella_140001.Mod_Changli_Umbrella_140001")))
	{
		const FBoxSphereBounds Bounds = Static->GetBounds();
		AddInfo(FString::Printf(TEXT("Static effect umbrella: origin %s extent %s"), *Bounds.Origin.ToString(), *Bounds.BoxExtent.ToString()));
	}
	return true;
}

#endif
