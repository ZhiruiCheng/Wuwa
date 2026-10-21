#pragma once

#include "CoreMinimal.h"
#include "WuwaSkillMotion.generated.h"

class UCurveFloat;

/**
 * 位移吸附到目标（原作 TsAnimNotifyStatePositionBranchTarget）的配置。字段名后面是原作字段名，默认值同原作。
 * 每帧：离目标远就往目标推一段（缓动曲线 + 最大速度封顶），够近了就把这一帧的根运动水平位移抵消掉（人钉在原地）。
 * 停止距离 = Distance + 自己胶囊半径 + 目标胶囊半径（勾了忽略双方半径则不加；目标点是骨骼时不加目标半径）。
 * 原作的目标偏移、离目标地面最小高度、黑板目标、召唤物共享目标还没做（导入脚本遇到非默认值会报错）。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaMoveToTargetConfig
{
	GENERATED_BODY()

	/** 停止距离（不含双方半径）。原作 Distance。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MoveToTarget", meta = (Units = "cm"))
	float Distance = 30.f;

	/** 往目标推的最大速度；≤ 0 时只抵消、不往前推。原作 MaxSpeed。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MoveToTarget", meta = (Units = "cm/s"))
	float MaxSpeed = 2000.f;

	/** 推的时候每帧朝向目标（只转水平朝向）。原作 是否永远面向目标。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MoveToTarget")
	bool bLookAtTarget = true;

	/** 只算水平距离、只在水平面上推。原作 忽略Z轴方向。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MoveToTarget")
	bool bIgnoreZ = true;

	/** 只在竖直方向上推。原作 忽略水平方向。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MoveToTarget")
	bool bIgnoreHorizontal = false;

	/** 够近时仍按曲线修正高度差（不忽略 Z 时）。原作 永远修正Z轴。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MoveToTarget")
	bool bAlwaysMoveZ = false;

	/** 停止距离不加双方胶囊半径。原作 忽略双方半径。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MoveToTarget")
	bool bIgnoreRadius = false;

	/** 比停止距离还近时往后退到停止距离。原作 允许反向移动。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MoveToTarget")
	bool bAllowReverse = false;

	/** 比停止距离远时往前推。原作 允许正向移动。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MoveToTarget")
	bool bAllowForward = true;

	/** 离目标比这还近就整段不管（> 0 才生效）。原作 最小吸附距离。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MoveToTarget", meta = (Units = "cm"))
	float MinDistance = 0.f;

	/** 离目标比这还远就整段不管（> 0 才生效）。原作 最大吸附距离。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MoveToTarget", meta = (Units = "cm"))
	float MaxDistance = 0.f;

	/**
	 * 目标点偏移（原作 TargetOffset），加到目标点上再吸附。原作另有 OffsetByOrientation / OffsetByAbsolute 两个开关，
	 * 都没开时按世界坐标加（推断；例：嘉贝空中下落攻击起手 (0, 0, 500) = 冲向目标头顶 5 米）。开关本身项目还不支持。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MoveToTarget", meta = (Units = "cm"))
	FVector TargetOffset = FVector::ZeroVector;

	/** 进度曲线（输入 已过时间 / 窗口时长，输出 0~1）；不填用三次缓动 (3 - 2t)t²。原作 MoveCurve。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MoveToTarget")
	TObjectPtr<UCurveFloat> MoveCurve;
};

/** 一帧位移吸附需要的数据（世界空间，Z 轴朝上）。 */
struct FWuwaMoveToTargetFrame
{
	/** 本帧根运动之前的位置（= 上一帧结束时的位置，原作 LastLocation）。 */
	FVector Location = FVector::ZeroVector;
	/** 本帧动画根运动的位移。 */
	FVector RootMotion = FVector::ZeroVector;
	/** 目标点（目标骨骼或目标位置）。 */
	FVector TargetLocation = FVector::ZeroVector;
	/** 自己的胶囊半径。 */
	float SelfRadius = 0.f;
	/** 目标的胶囊半径（目标点是骨骼时为 0）。 */
	float TargetRadius = 0.f;
	/** 窗口里已过的时间、窗口总时长、本帧时长。 */
	float NowTime = 0.f;
	float TotalTime = 0.f;
	float DeltaTime = 0.f;
};

/** 一帧位移吸附的结果。 */
struct FWuwaMoveToTargetResult
{
	/** 本帧实际的位移（替换根运动位移）。 */
	FVector Displacement = FVector::ZeroVector;
	/** 是否要转向目标；转向的朝向（水平）。 */
	bool bFaceTarget = false;
	FRotator Facing = FRotator::ZeroRotator;
	/** 这一帧走的哪个分支（调试 / 测试用）。 */
	enum class EBranch : uint8 { None, Pull, Hold } Branch = EBranch::None;
};

/** 技能转向（原作 BaseSkillComponent 的 SkillCanRotate / SkillRotateToTarget / SkillRotateSpeed 与 SZr 转向参数）。 */
struct FWuwaSkillRotateState
{
	/** 窗口开着（原作 SetSkillCanRotate）。 */
	bool bCanRotate = false;
	/** 转向技能目标；否则转向输入方向（原作 SetSkillRotateToTarget）。 */
	bool bRotateToTarget = false;
	/** 转速（度 / 秒，原作 SetSkillRotateSpeed）。 */
	float Speed = 0.f;
	/** 目标方向再偏转的角度（原作 AnsRotateOffset，已取反并夹在 ±180）。 */
	bool bUseOffset = false;
	float Offset = 0.f;
	/** 离目标方向小于 Pause 度时停转，停转后大于 Resume 度才继续（> 0 才生效）。 */
	float PauseThreshold = 0.f;
	float ResumeThreshold = 0.f;
	bool bPaused = false;
	/** 开启窗口的通知（结束时只清自己开的）。 */
	const void* Owner = nullptr;

	void Reset() { *this = FWuwaSkillRotateState(); }
};

namespace WuwaSkillMotion
{
	/** 原作 MathUtils.GetCubicValue：(3 - 2t)t²。 */
	WUWA_API float GetCubicValue(float T);

	/** 原作 GetRate：这一帧要走完剩余距离的比例（窗口结束那帧为 1）。 */
	WUWA_API float GetRate(const UCurveFloat* Curve, float NowTime, float TotalTime, float DeltaTime);

	/** 原作 MoveToTarget 的一帧（窗口内、目标有效时）。 */
	WUWA_API FWuwaMoveToTargetResult ComputeMoveToTarget(const FWuwaMoveToTargetConfig& Config, const FWuwaMoveToTargetFrame& Frame);

	/**
	 * 原作 SkillUtils.GetSkillRotateDirect（技能目标类型）+ 转向偏移 / 停转阈值：返回要转向的方向（水平），没有时为零向量。
	 * Forward 是角色当前朝向；会更新 State.bPaused。
	 */
	WUWA_API FVector GetSkillRotateDirection(FWuwaSkillRotateState& State, const FVector& Location, const FVector& Forward, const FVector* TargetLocation);

	/** 原作 MathUtils.RotatorInterpConstantTo，只转水平朝向：每帧最多转 Speed × DeltaTime 度。 */
	WUWA_API float InterpYawConstantTo(float CurrentYaw, float TargetYaw, float DeltaTime, float Speed);
}
