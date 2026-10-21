#include "Game/Animation/IK/WuwaFootIK.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Rigs/RigHierarchy.h"
#include "TwoBoneIK.h"

namespace WuwaFootIK
{
static FRigElementKey Bone(const TCHAR* Name) { return FRigElementKey(Name, ERigElementType::Bone); }

FWuwaFootIKResult Solve(
	const FControlRigExecuteContext& Context,
	const FWuwaFootIKFrame& Frame,
	const FWuwaFootIKSettings& Settings,
	FWuwaFootIKWork& Work)
{
	// 1. 取得执行环境，并检查这一帧是否允许做 IK。
	// H 是 CR 的骨骼层级；World 用于地面检测；ToWorld 用于转换坐标。
	FWuwaFootIKResult Result;
	URigHierarchy* H = Context.Hierarchy;
	const UWorld* World = Context.GetWorld();
	const FTransform& ToWorld = Context.GetToWorldSpaceTransform();
	const FVector Origin = ToWorld.GetLocation();

	if (!H || !World || !Settings.bEnabled || !Frame.bGrounded || Frame.Alpha <= .0001f)
	{
		Work = FWuwaFootIKWork{};
		return Result;
	}

	// 2. 初始化跨帧记录；角色位置突变超过 100 cm 时，丢弃旧的平滑偏移。
	if (!Work.bInitialized || FVector::DistSquared(Origin, Work.LastOrigin) > FMath::Square(100.f))
	{
		Work = FWuwaFootIKWork{};
	}

	Work.bInitialized = true;
	Work.LastOrigin = Origin;

	// 把帧时间限制在 0～0.1 秒，再计算本帧向目标靠近的比例。
	const float Dt = FMath::Clamp(float(Context.GetDeltaTime()), 0.f, .1f);
	const float Blend = 1.f - FMath::Exp(-Settings.InterpSpeed * Dt);

	// 3. 找到需要操作的骨骼。
	// Keys[左右脚][骨骼]：Side 0 = 左，1 = 右；骨骼 0 = 大腿，1 = 小腿，2 = 脚，3 = 脚趾。
	const FRigElementKey PelvisKey = Bone(TEXT("Bip001Pelvis"));
	const FRigElementKey Keys[2][4] = {
		{
			Bone(TEXT("Bip001LThigh")), Bone(TEXT("Bip001LCalf")),
			Bone(TEXT("Bip001LFoot")), Bone(TEXT("Bip001LToe0"))
		},
		{
			Bone(TEXT("Bip001RThigh")), Bone(TEXT("Bip001RCalf")),
			Bone(TEXT("Bip001RFoot")), Bone(TEXT("Bip001RToe0"))
		}
	};

	if (H->GetIndex(PelvisKey) == INDEX_NONE)
	{
		return Result;
	}

	for (const auto& Side : Keys)
	{
		for (const auto& Key : Side)
		{
			if (H->GetIndex(Key) == INDEX_NONE)
			{
				return Result;
			}
		}
	}

	// 4. 准备左右脚的临时计算数据，以及忽略角色自身的射线检测参数。
	// Input：本帧输入动画的世界空间变换；Weights：最终 IK 权重。
	// Contact：根据动画抬脚高度估计的接地权重，不是碰撞检测结果。
	// Normals：目标地面法线；Targets：目标垂直修正量，未命中时保持为零。
	FTransform Input[2][4];
	float Weights[2] = {};
	float Contact[2] = {};
	FVector Normals[2] = {FVector::UpVector, FVector::UpVector};
	float Targets[2] = {};

	FCollisionQueryParams Query(SCENE_QUERY_STAT(WuwaFootIK), true);
	if (Context.GetOwningActor())
	{
		Query.AddIgnoredActor(Context.GetOwningActor());
	}
	if (const auto* Component = Cast<UPrimitiveComponent>(Context.GetOwningComponent()))
	{
		Query.AddIgnoredComponent(Component);
	}

	// 5. 分别计算左右脚的地形修正量。这个循环只读取姿势，暂不修改骨骼。
	for (int32 Side = 0; Side < 2; ++Side)
	{
		// 5.1 保存当前动画的原始姿势，转换到世界空间。
		// 这里需要本帧输入姿势，不能把上一帧 IK 解算后的脚位置当作新目标。
		for (int32 B = 0; B < 4; ++B)
		{
			Input[Side][B] = Context.ToWorldSpace(H->GetGlobalTransform(Keys[Side][B]));
		}

		const FVector Foot = Input[Side][2].GetLocation();
		const FVector Toe = Input[Side][3].GetLocation();

		// 5.2 估算动画本身抬脚多少，避免把摆动中的脚强行压到地上。
		// GetGlobalTransform(..., true) 读取初始姿势；TransformVector 不附加世界平移。
		const float FootRest = ToWorld.TransformVector(
			H->GetGlobalTransform(Keys[Side][2], true).GetLocation()).Z;
		const float ToeRest = ToWorld.TransformVector(
			H->GetGlobalTransform(Keys[Side][3], true).GetLocation()).Z;

		const float Lift = FMath::Max(
			0.f,
			FMath::Min(
				float(Foot.Z - Frame.GroundZ) - FootRest,
				float(Toe.Z - Frame.GroundZ) - ToeRest));

		// 抬脚高度从 2 cm 增至 16 cm 时，估计的接地权重从 1 平滑降至 0。
		Contact[Side] = 1.f - FMath::SmoothStep(2.f, 16.f, Lift);

		// 5.3 优先读取动画中的左右脚 IK 曲线（曲线值为零也必须尊重）。
		// 缺少曲线时才使用步态 + Contact 的兜底权重；这是项目调校，不是原作公式。
		const FRigElementKey Curve(
			Side ? TEXT("Foot_IK_r") : TEXT("Foot_IK_l"),
			ERigElementType::Curve);

		const float Minimum = Frame.bSprint ? 0.f : (Frame.bRun ? .55f : 1.f);
		const float CurveWeight = H->IsCurveValueSet(Curve)
			? H->GetCurveValue(Curve)
			: FMath::Lerp(Minimum, 1.f, Contact[Side]);

		Weights[Side] = FMath::Clamp(
			FMath::IsFinite(CurveWeight) ? CurveWeight : 0.f,
			0.f,
			1.f) * Frame.Alpha;

		// 5.4 在脚的 XY 位置向下检测地面。
		// 检测范围围绕角色地面基准 GroundZ，而不是围绕正在抬起的脚骨骼高度。
		FHitResult Hit;
		const FVector Center(Foot.X, Foot.Y, Frame.GroundZ);
		const bool bHit = World->LineTraceSingleByChannel(
			Hit,
			Center + FVector::UpVector * (Settings.MaxStepHeight + 5.f),
			Center - FVector::UpVector * Settings.MaxStepDepth,
			Settings.TraceChannel,
			Query)
			&& !Hit.bStartPenetrating
			&& Hit.ImpactNormal.Z >= .65f;

		if (Side)
		{
			Result.bRightHit = bHit;
		}
		else
		{
			Result.bLeftHit = bHit;
		}

		// 5.5 地形修正量 = 命中地面的高度 - 角色地面基准高度，再限幅并乘权重。
		if (bHit)
		{
			Targets[Side] = FMath::Clamp(
				float(Hit.ImpactPoint.Z - Frame.GroundZ),
				-Settings.MaxStepDepth,
				Settings.MaxStepHeight) * Weights[Side];

			Normals[Side] = Hit.ImpactNormal.GetSafeNormal();
		}

		// 5.6 从上一帧记录平滑靠近目标；未命中时回到零修正和向上的法线。
		// 不把射线末端当成地面。Work.Offsets 的 X/Y 分别存左/右脚的垂直偏移。
		Work.Offsets[Side] = FMath::Lerp(Work.Offsets[Side], Targets[Side], Blend);
		FVector& Normal = Side ? Work.RightNormal : Work.LeftNormal;
		Normal = FMath::Lerp(Normal, Normals[Side], Blend).GetSafeNormal();
	}

	// 6. 根据较低一侧的脚修正量下移骨盆，Contact 降低抬脚侧的影响。
	// 骨盆只下降、不抬高，并受 MaxPelvisDrop 限制；Offsets.Z 保存骨盆的垂直偏移。
	const float PelvisTarget = FMath::Clamp(
		FMath::Min3(
			0.f,
			float(Work.Offsets.X) * Contact[0],
			float(Work.Offsets.Y) * Contact[1]),
		-Settings.MaxPelvisDrop,
		0.f);

	Work.Offsets.Z = FMath::Lerp(float(Work.Offsets.Z), PelvisTarget, Blend);

	// 7. 填充本帧诊断输出。Result 不是姿势，骨骼在后面直接写回 CR。
	Result.LeftWeight = Weights[0];
	Result.RightWeight = Weights[1];
	Result.Offsets = Work.Offsets;

	// 偏移和倾斜都已归零时，直接保留原动画，避免无意义解算引入直腿抖动。
	if (Work.Offsets.IsNearlyZero(.0001)
		&& Work.LeftNormal.Equals(FVector::UpVector, .0001)
		&& Work.RightNormal.Equals(FVector::UpVector, .0001))
	{
		return Result;
	}

	// 8. 先下移骨盆，再基于更新后的腿部姿势做 IK。
	// 计算使用世界空间，写回时转为 Rig VM 空间；最后一个 true 表示传播到子骨骼。
	FTransform Pelvis = Context.ToWorldSpace(H->GetGlobalTransform(PelvisKey));
	Pelvis.AddToTranslation(FVector::UpVector * Work.Offsets.Z);
	H->SetGlobalTransform(PelvisKey, Context.ToVMSpace(Pelvis), false, true);

	// 9. 分别解算左右腿，再调整脚掌朝向并写回骨骼。
	for (int32 Side = 0; Side < 2; ++Side)
	{
		// 9.1 读取下移骨盆后的腿部姿势；脚的目标仍以第 5 步保存的原动画为基准。
		FTransform Thigh = Context.ToWorldSpace(H->GetGlobalTransform(Keys[Side][0]));
		FTransform Calf = Context.ToWorldSpace(H->GetGlobalTransform(Keys[Side][1]));
		FTransform Foot = Context.ToWorldSpace(H->GetGlobalTransform(Keys[Side][2]));
		const FVector Target = Input[Side][2].GetLocation() + FVector::UpVector * Work.Offsets[Side];

		// 9.2 用原动画的膝盖位置确定弯曲方向。
		// Axis 是大腿到脚的方向；Bend 去掉沿 Axis 的分量后，指向膝盖弯曲的一侧。
		const FVector Axis = (Input[Side][2].GetLocation() - Input[Side][0].GetLocation()).GetSafeNormal();
		FVector Bend = Input[Side][1].GetLocation() - Input[Side][0].GetLocation();
		Bend -= Axis * FVector::DotProduct(Bend, Axis);
		if (!Bend.Normalize())
		{
			// 腿完全伸直时，当前姿势可能无法给出弯曲方向，改用初始姿势估计。
			Bend = ToWorld.TransformVector(
				H->GetGlobalTransform(Keys[Side][1], true).GetLocation()
				- H->GetGlobalTransform(Keys[Side][0], true).GetLocation());
			Bend -= Axis * FVector::DotProduct(Bend, Axis);

			if (!Bend.Normalize())
			{
				Bend = FVector::CrossProduct(Axis, FVector::RightVector).GetSafeNormal();
			}
		}

		const FVector Pole = Thigh.GetLocation() + Bend * 100.f;

		// 9.3 解算大腿、小腿和脚的位置，保持腿长及原缩放，不允许拉伸。
		AnimationCore::SolveTwoBoneIK(Thigh, Calf, Foot, Pole, Target, false, 1., 1.);

		// 9.4 根据地面法线计算脚掌倾斜，限制最大角度，再乘 IK 和接地权重。
		const FVector Normal = Side ? Work.RightNormal : Work.LeftNormal;
		FQuat Tilt = FQuat::FindBetweenNormals(FVector::UpVector, Normal);
		const float Angle = Tilt.GetAngle();
		if (Angle > FMath::DegreesToRadians(Settings.MaxFootAngle))
		{
			Tilt = FQuat::Slerp(FQuat::Identity, Tilt, FMath::DegreesToRadians(Settings.MaxFootAngle) / Angle);
		}

		Tilt = FQuat::Slerp(FQuat::Identity, Tilt, Weights[Side] * Contact[Side]);
		Foot.SetRotation((Tilt * Input[Side][2].GetRotation()).GetNormalized());

		// 9.5 把最终骨骼变换写回 CR；Control Rig 节点随后输出修正后的姿势。
		H->SetGlobalTransform(Keys[Side][0], Context.ToVMSpace(Thigh), false, true);
		H->SetGlobalTransform(Keys[Side][1], Context.ToVMSpace(Calf), false, true);
		H->SetGlobalTransform(Keys[Side][2], Context.ToVMSpace(Foot), false, true);
	}

	return Result;
}
}

FRigUnit_WuwaFootIK_Execute()
{
	const auto* Mesh = Cast<USkeletalMeshComponent>(ExecuteContext.GetOwningComponent());
	const auto* Anim = Mesh ? Cast<UWuwaAnimInstance>(Mesh->GetAnimInstance()) : nullptr;
	if (!Anim) { Work = FWuwaFootIKWork{}; Result = FWuwaFootIKResult{}; return; }
	Result = WuwaFootIK::Solve(ExecuteContext, Anim->FootIKFrame, Anim->FootIKSettings, Work);
}
