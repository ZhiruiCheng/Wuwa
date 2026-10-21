#include "Game/NewWorld/Bullet/WuwaBulletTypes.h"

namespace WuwaBullet
{
	int64 ParseBulletId(FName RowName)
	{
		const FString Text = RowName.ToString();
		if (Text.IsEmpty() || !Text.IsNumeric() || Text.Contains(TEXT(".")) || Text.StartsWith(TEXT("-"))) return 0;
		return FCString::Atoi64(*Text);
	}

	int32 GetSkillId(int64 BulletId)
	{
		return BulletId > 0 ? static_cast<int32>(BulletId / 1000) : 0;
	}

	FTransform ComputeBornBase(const FWuwaBulletInfo& Info, const FWuwaBulletBornContext& Context)
	{
		const FTransform& Attacker = Context.Attacker;
		switch (Info.BornStandard)
		{
		case EWuwaBulletBornStandard::World:
			return FTransform::Identity;
		case EWuwaBulletBornStandard::SkillTarget:
		{
			// 原作 IVo：距离按发射者 Actor 位置（胶囊中心）算
			const FVector From = Attacker.GetLocation();
			FVector Origin = From;
			if (Context.SkillTarget.IsSet())
			{
				Origin = Context.SkillTarget->GetLocation();
				const FVector Offset = Origin - From;
				const double Distance = Offset.Size();
				const double MaxDistance = Info.BornDistLimit.Y > 0.f ? Info.BornDistLimit.Y : TNumericLimits<double>::Max();
				if (Distance > MaxDistance) Origin = From + Offset.GetSafeNormal() * MaxDistance;
				else if (Distance < Info.BornDistLimit.X && Distance > UE_KINDA_SMALL_NUMBER) Origin = From + Offset.GetSafeNormal() * Info.BornDistLimit.X;
			}
			else if (Info.BornDistLimit.Z > 0.f)
			{
				Origin = From + Attacker.GetRotation().GetForwardVector() * Info.BornDistLimit.Z;
			}
			return FTransform(Attacker.GetRotation(), Origin);
		}
		case EWuwaBulletBornStandard::Parent:
			if (Context.Parent.IsSet())
			{
				FTransform Parent = *Context.Parent;
				Parent.SetScale3D(FVector::OneVector);
				return Parent;
			}
			break;   // 没有父子弹时按发射者
		default:
			break;
		}
		if (Context.Bone.IsSet() && !Info.BoneName.IsNone())
		{
			const FTransform& Bone = *Context.Bone;
			if (Info.FollowType == EWuwaBulletFollowType::SnapToBone) return FTransform(Bone.GetRotation(), Bone.GetLocation());
			return FTransform(Attacker.GetRotation(), Bone.GetLocation());
		}
		return FTransform(Attacker.GetRotation(), Attacker.TransformPosition(FVector(0.f, 0.f, -Context.CapsuleHalfHeight)));
	}

	FTransform ComputeBornTransform(const FWuwaBulletInfo& Info, const FWuwaBulletBornContext& Context)
	{
		const FTransform Base = ComputeBornBase(Info, Context);
		return FTransform(Base.GetRotation() * Info.BornRotation.Quaternion(), Base.TransformPosition(Info.BornOffset));
	}

	FTransform ComputeShapeTransform(const FWuwaBulletInfo& Info, const FTransform& Born)
	{
		return FTransform(Born.GetRotation(), Born.TransformPosition(Info.CenterOffset));
	}

	bool HasCollision(const FWuwaBulletInfo& Info)
	{
		const FVector& S = Info.Size;
		switch (Info.Shape)
		{
		case EWuwaBulletShape::Box: return S.X > 0.f && S.Y > 0.f && S.Z > 0.f;
		case EWuwaBulletShape::Sphere: return S.X > 0.f;
		case EWuwaBulletShape::Sector: return S.X > 0.f && S.Z > 0.f;
		case EWuwaBulletShape::Cylinder: return S.X > 0.f && S.Z > 0.f;
		default: return false;
		}
	}

	namespace
	{
		/** 原作 SpaceUtils.boundPoints：包围盒 8 个角的方向。 */
		const FVector BoundPoints[8] = {
			FVector(1, 1, 1), FVector(1, 1, -1), FVector(1, -1, 1), FVector(1, -1, -1),
			FVector(-1, 1, 1), FVector(-1, 1, -1), FVector(-1, -1, 1), FVector(-1, -1, -1)};

		/** 原作 SpaceUtils.G9s：平面上（忽略 Z）盒子和圆是否相交。 */
		bool BoxCircle2D(const FVector& BoxOrigin, const FVector& BoxExtent, const FVector& Point, double Radius)
		{
			FVector D = (Point - BoxOrigin).GetAbs() - BoxExtent;
			D.Z = 0.0;
			bool bInsideX = false;
			if (D.X < 0.0) { D.X = 0.0; bInsideX = true; }
			if (D.Y < 0.0)
			{
				if (bInsideX) return true;
				D.Y = 0.0;
			}
			return D.SizeSquared() <= Radius * Radius;
		}
	}

	FVector GetBroadphaseBox(const FWuwaBulletInfo& Info, FVector& OutExtent)
	{
		const FVector& S = Info.Size;
		switch (Info.Shape)
		{
		case EWuwaBulletShape::Sphere:
			OutExtent = FVector(S.X);
			return FVector::ZeroVector;
		case EWuwaBulletShape::Sector:
		{
			// 原作 GetSectorExtent：张角 < 180° 时盒子覆盖前方半径的一半；否则按 cos(半角) 往后扩
			const double Half = FMath::DegreesToRadians(S.Y * 0.5);
			if (S.Y < 180.f)
			{
				OutExtent = FVector(0.5 * S.X, FMath::Sin(Half) * S.X, S.Z);
				return FVector(0.5 * S.X, 0.0, 0.0);
			}
			const double Cos = FMath::Cos(Half);
			OutExtent = FVector(S.X * (1.0 - Cos) * 0.5, S.X, S.Z);
			return FVector(S.X * (1.0 + Cos) * 0.5, 0.0, 0.0);
		}
		case EWuwaBulletShape::Cylinder:
			OutExtent = FVector(S.X, S.X, S.Z);
			return FVector::ZeroVector;
		default:
			OutExtent = S;
			return FVector::ZeroVector;
		}
	}

	bool IsBoxInRing(const FVector& Center, const FVector& Size, const FVector& Origin, const FVector& Extent, const FQuat& Rotation)
	{
		const double Outer2 = FMath::Square(Size.X);
		if (Outer2 < FMath::Square(Extent.X) || Outer2 < FMath::Square(Extent.Y))
		{
			return BoxCircle2D(FVector(Origin.X, Origin.Y, 0.0), FVector(Extent.X, Extent.Y, 0.0), FVector(Center.X, Center.Y, 0.0), Size.X);
		}
		const FVector ToBox = Origin - Center;
		const double HalfHeight = Size.Z, Inner2 = FMath::Square(Size.Y);
		int32 InHeight = 0, InDistance = 0;
		for (const FVector& Point : BoundPoints)
		{
			const FVector Corner = Rotation.RotateVector(Extent * Point) + ToBox;
			if (InHeight != 3) { if (Corner.Z > HalfHeight) InHeight |= 1; else if (-Corner.Z > HalfHeight) InHeight |= 2; else InHeight = 3; }
			const double Distance2 = Corner.SizeSquared2D();
			if (InDistance != 3) { if (Outer2 < Distance2) InDistance |= 1; else if (Distance2 < Inner2) InDistance |= 2; else InDistance = 3; }
			if (InHeight == 3 && InDistance == 3) return true;
		}
		return false;
	}

	bool IsBoxInSector(const FVector& Center, const FVector& Size, const FQuat& ShapeRotation, const FVector& Origin, const FVector& Extent, const FQuat& Rotation)
	{
		if (Extent.X > Size.X || Extent.Y > Size.X) return true;
		const FVector ToBox = Origin - Center;
		const FQuat ToLocal = ShapeRotation.Inverse();
		const double Radius2 = FMath::Square(Size.X), Half = FMath::DegreesToRadians(Size.Y * 0.5), HalfHeight = Size.Z;
		const bool bNarrow = Half < 0.5 * PI;
		int32 InRadius = 0, InHeight = 0, InAngle = 0;
		TArray<FVector, TInlineAllocator<8>> Left, Right;
		for (const FVector& Point : BoundPoints)
		{
			const FVector Corner = ToLocal.RotateVector(Rotation.RotateVector(Extent * Point) + ToBox);
			if (InHeight != 3) { if (Corner.Z > HalfHeight) InHeight |= 1; else if (-Corner.Z > HalfHeight) InHeight |= 2; else InHeight = 3; }
			if (InRadius != 3 && Corner.SizeSquared2D() <= Radius2) InRadius = 3;
			const double Angle = FMath::Atan2(Corner.Y, Corner.X);
			if (InAngle != 3)
			{
				if (bNarrow)
				{
					if (Half < Angle) Left.Add(Corner);
					else if (Half < -Angle) Right.Add(Corner);
					else InAngle = 3;
				}
				else if (FMath::Abs(Angle) <= Half) InAngle = 3;
			}
			if (InHeight == 3 && InRadius == 3 && InAngle == 3) return true;
		}
		// 原作 Az：一个在扇形左边、一个在右边的角，连线和 +X 轴的交点在前方，说明盒子横跨扇形
		if (bNarrow && InHeight == 3 && InRadius == 3)
		{
			for (const FVector& R : Right)
			{
				for (const FVector& L : Left)
				{
					if (FMath::Abs(L.Y - R.Y) < UE_SMALL_NUMBER) continue;
					if (L.X - (L.X - R.X) / (L.Y - R.Y) * L.Y >= 0.0) return true;
				}
			}
		}
		return false;
	}

	bool IsCollisionActive(const FWuwaBulletInfo& Info, float LiveTime)
	{
		if (!HasCollision(Info)) return false;
		const float SinceOpen = LiveTime - FMath::Max(Info.CollisionDelay, 0.f);
		// 结束端留一点容差（原作也是 ActiveLengthMs + SmallNumber），免得 0.3 - 0.1 > 0.2 这种浮点误差提前关窗
		return SinceOpen >= 0.f && (Info.CollisionDuration <= 0.f || SinceOpen <= Info.CollisionDuration + KINDA_SMALL_NUMBER);
	}
}
