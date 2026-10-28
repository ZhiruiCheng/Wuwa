// UWuwaAnimInstance 的脚步 IK 部分：每帧在游戏线程决定脚步 IK 该不该生效、该多强，
// 结果写进 FootIKFrame，由 Control Rig 的 Wuwa Terrain Foot IK 节点在动画线程读取。
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimInstance.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"

// 带名字的命名空间：模块开了 Unity Build，匿名命名空间里的名字可能和同一编译单元的其他文件冲突
namespace WuwaAnimFootIKUpdate
{
	// 移动平台（原作导出蓝图的数值）：速度超过 25 cm/s 判定为在动，降到 5 cm/s 以下才解除，
	// 两个阈值防止在临界速度附近来回切换；平台系数的插值速度 3
	constexpr float PlatformStartSpeed = 25.f;
	constexpr float PlatformStopSpeed = 5.f;
	constexpr float PlatformInterpSpeed = 3.f;

	// 陡坡：沿移动方向的坡度 40° 开始削弱，50° 完全关闭；削弱量的插值速度 2
	constexpr float SlopeFadeStartDegrees = 40.f;
	constexpr float SlopeFadeEndDegrees = 50.f;
	constexpr float SlopeInterpSpeed = 2.f;

	// 最终强度 Alpha 的插值速度
	constexpr float AlphaInterpSpeed = 12.f;

	/** 脚下移动平台的速度：平台自身的平移速度 + 平台旋转在角色位置产生的切向速度。 */
	FVector GetPlatformVelocity(const ACharacter& Character)
	{
		const UPrimitiveComponent* Base = Character.GetMovementBase();
		const FName Bone = Character.GetBasedMovement().BoneName;
		return MovementBaseUtility::GetMovementBaseVelocity(Base, Bone)
			+ MovementBaseUtility::GetMovementBaseTangentialVelocity(Base, Bone, Character.GetActorLocation());
	}

	/** 沿移动方向的坡度（度）：顺着走是上坡为正。原作导出里的正负约定未知，这里只用绝对值。 */
	float GetTravelSlopeDegrees(const UWuwaMovementComponent& Movement)
	{
		const FVector Normal = Movement.CurrentFloor.HitResult.ImpactNormal;
		const FVector Direction = Movement.Velocity.GetSafeNormal2D();
		return FMath::RadiansToDegrees(FMath::Atan2(-FVector::DotProduct(Normal, Direction), Normal.Z));
	}
}

void UWuwaAnimInstance::UpdateFootIK(float DeltaSeconds, ACharacter* Character, UWuwaMovementComponent* Movement)
{
	check(IsInGameThread());
	using namespace WuwaAnimFootIKUpdate;

	// 1. 允不允许用 IK：在地面上、开关开着、没有动作覆盖、没在播蒙太奇。
	//    原作的 IKMode 在原生代码里看不到，空中和放技能时按项目的保守规则关闭。
	const bool bOnGround = Character && Movement && Movement->IsMovingOnGround();
	const bool bAllowed = bOnGround && FootIKSettings.bEnabled && !bForceDisableFootIK
		&& !LocomotionData.bHasActionOverride && !IsAnyMontagePlaying();
	FootIKFrame.bGrounded = bAllowed;

	// 2. 跑动状态：Rig 用它决定抬起的脚还贴不贴地（冲刺后的急停也按冲刺算）
	FootIKFrame.bSprint = LocomotionData.bStateGroundSprint
		|| (bRoleStopSprint && GetDebugStateName(TEXT("SM_Ground")) == TEXT("Stop"));
	FootIKFrame.bRun = LocomotionData.bStateGroundRun;

	// 3. 在空中：强度归零，平台和坡度的状态复位
	if (!bOnGround)
	{
		FootIKFrame.Alpha = 0.f;
		FootIKPlatformAlpha = 1.f;
		FootIKSlopeDiminish = 0.f;
		bFootIKPlatformMoving = false;
		return;
	}

	// 4. 地面高度：胶囊底部，再减去离地板的距离
	FootIKFrame.GroundZ = Character->GetActorLocation().Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (Movement->CurrentFloor.IsWalkableFloor())
	{
		FootIKFrame.GroundZ -= Movement->CurrentFloor.GetDistanceToFloor();
	}

	// 5. 移动平台：平台在动时逐渐关掉 IK
	const float PlatformSpeed = GetPlatformVelocity(*Character).Size();
	bFootIKPlatformMoving = PlatformSpeed > (bFootIKPlatformMoving ? PlatformStopSpeed : PlatformStartSpeed);
	FootIKPlatformAlpha = FMath::FInterpTo(FootIKPlatformAlpha, bFootIKPlatformMoving ? 0.f : 1.f, DeltaSeconds, PlatformInterpSpeed);

	// 6. 陡坡：移动中沿走向的坡度太大时削弱 IK
	float TargetSlopeDiminish = 0.f;
	if (Movement->CurrentFloor.IsWalkableFloor() && LocomotionData.bHasMovingSpeed)
	{
		TargetSlopeDiminish = FMath::GetMappedRangeValueClamped(FVector2D(SlopeFadeStartDegrees, SlopeFadeEndDegrees),
			FVector2D(0.f, 1.f), FMath::Abs(GetTravelSlopeDegrees(*Movement)));
	}
	FootIKSlopeDiminish = FMath::FInterpTo(FootIKSlopeDiminish, TargetSlopeDiminish, DeltaSeconds, SlopeInterpSpeed);

	// 7. 最终强度：允许时 = (1 − 坡度削弱) × 平台系数，不允许时为 0；平滑过渡过去
	const float TargetAlpha = bAllowed ? (1.f - FootIKSlopeDiminish) * FootIKPlatformAlpha : 0.f;
	FootIKFrame.Alpha = FMath::FInterpTo(FootIKFrame.Alpha, TargetAlpha, DeltaSeconds, AlphaInterpSpeed);
}
