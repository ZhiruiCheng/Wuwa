#include "Game/NewWorld/Character/Common/Component/Move/WuwaSkillMotion.h"

#include "Curves/CurveFloat.h"

namespace WuwaSkillMotion
{
	/** 一帧抵消量超过 20 米视为异常（原作 INVALID_LAST_LOCATION_THRESHOLD_SQUARED = 4e6），不抵消。 */
	constexpr double InvalidHoldDistanceSquared = 4e6;

	float GetCubicValue(float T)
	{
		return (-2.f * T + 3.f) * T * T;
	}

	float GetRate(const UCurveFloat* Curve, float NowTime, float TotalTime, float DeltaTime)
	{
		const float Next = NowTime + DeltaTime;
		if (TotalTime <= Next) return 1.f;
		const float From = Curve ? Curve->GetFloatValue(NowTime / TotalTime) : GetCubicValue(NowTime / TotalTime);
		const float To = Curve ? Curve->GetFloatValue(Next / TotalTime) : GetCubicValue(Next / TotalTime);
		if (From >= 1.f) return 0.f;
		return (To - From) / (1.f - From);
	}

	FWuwaMoveToTargetResult ComputeMoveToTarget(const FWuwaMoveToTargetConfig& Config, const FWuwaMoveToTargetFrame& Frame)
	{
		FWuwaMoveToTargetResult Result;
		Result.Displacement = Frame.RootMotion;
		// 原作在根运动之后算：P1 = 根运动之后的位置；目标在 P0→P1 之间被越过时（点积 < 0）距离记 -1，走"抵消"分支
		const FVector Before = Frame.Location;
		const FVector After = Frame.Location + Frame.RootMotion;
		FVector Toward = Frame.TargetLocation - After;
		const bool bPassed = FVector::DotProduct(Toward, Frame.TargetLocation - Before) < 0.0;
		double Distance;
		if (Config.bIgnoreZ)
		{
			Toward.Z = 0.0;
			Distance = bPassed ? -1.0 : Toward.Size();
		}
		else if (Config.bIgnoreHorizontal)
		{
			Toward.X = Toward.Y = 0.0;
			Distance = bPassed ? -1.0 : Toward.Size();
		}
		else
		{
			Distance = bPassed ? -1.0 : Toward.Size2D();
		}
		if ((Config.MinDistance > 0.f && Distance < Config.MinDistance) || (Config.MaxDistance > 0.f && Distance > Config.MaxDistance)) return Result;

		double Stop = Config.Distance;
		if (!Config.bIgnoreRadius) Stop += Frame.SelfRadius + Frame.TargetRadius;
		const double MaxStep = Config.MaxSpeed * Frame.DeltaTime;
		FVector Move;
		if (Distance > KINDA_SMALL_NUMBER && ((Stop < Distance && Config.bAllowForward) || (Stop > Distance && Config.bAllowReverse)))
		{
			// 往目标推（或退）：剩余距离 × 这一帧的进度，按最大速度封顶
			if (Config.MaxSpeed <= 0.f) return Result;
			const float Rate = GetRate(Config.MoveCurve, Frame.NowTime, Frame.TotalTime, Frame.DeltaTime);
			if (Rate <= 0.f) return Result;
			const double Length = Toward.Size();
			const double Amount = FMath::Clamp((Length - Stop) * Rate, -MaxStep, MaxStep);
			Move = Toward * (Amount / Length);
			Result.Branch = FWuwaMoveToTargetResult::EBranch::Pull;
		}
		else
		{
			// 够近（或越过）：抵消这一帧根运动的水平位移；勾了永远修正 Z 轴时高度按曲线贴向目标
			Move = Before - After;
			if (Config.bAlwaysMoveZ && !Config.bIgnoreZ)
			{
				const float Rate = GetRate(Config.MoveCurve, Frame.NowTime, Frame.TotalTime, Frame.DeltaTime);
				Move.Z = FMath::Clamp((Frame.TargetLocation.Z - After.Z) * Rate, -MaxStep, MaxStep);
			}
			else
			{
				Move.Z = 0.0;
			}
			if (Move.SizeSquared() > InvalidHoldDistanceSquared) Move = FVector::ZeroVector;
			Result.Branch = FWuwaMoveToTargetResult::EBranch::Hold;
		}
		Result.Displacement = Frame.RootMotion + Move;

		if (Config.bLookAtTarget)
		{
			FVector Face = Frame.TargetLocation - (Before + Result.Displacement);
			Face.Z = 0.0;
			if (!Face.IsNearlyZero())
			{
				Result.bFaceTarget = true;
				Result.Facing = FRotator(0.0, Face.Rotation().Yaw, 0.0);
			}
		}
		return Result;
	}

	FVector GetSkillRotateDirection(FWuwaSkillRotateState& State, const FVector& Location, const FVector& Forward, const FVector* TargetLocation)
	{
		FVector Direction = TargetLocation ? (*TargetLocation - Location).GetSafeNormal() : FVector::ZeroVector;
		if (Direction.IsNearlyZero()) return FVector::ZeroVector;
		if (State.bUseOffset && State.Offset != 0.f)
		{
			Direction.Z = 0.0;
			Direction = FRotator(0.0, State.Offset, 0.0).RotateVector(Direction);
		}
		const FVector Planar = FVector(Direction.X, Direction.Y, 0.0).GetSafeNormal();
		const FVector PlanarForward = FVector(Forward.X, Forward.Y, 0.0).GetSafeNormal();
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Planar, PlanarForward), -1.0, 1.0)));
		if (State.bPaused)
		{
			if (State.ResumeThreshold > 0.f)
			{
				if (Angle < State.ResumeThreshold) Direction = Forward;
				else State.bPaused = false;
			}
		}
		else if (State.PauseThreshold > 0.f && Angle < State.PauseThreshold)
		{
			State.bPaused = true;
			Direction = Forward;
		}
		return Direction;
	}

	float InterpYawConstantTo(float CurrentYaw, float TargetYaw, float DeltaTime, float Speed)
	{
		if (DeltaTime <= 0.f || Speed <= 0.f) return CurrentYaw;
		const float Step = Speed * DeltaTime;
		const float Delta = FMath::Clamp(FRotator::NormalizeAxis(TargetYaw - CurrentYaw), -Step, Step);
		return FRotator::NormalizeAxis(CurrentYaw + Delta);
	}
}
