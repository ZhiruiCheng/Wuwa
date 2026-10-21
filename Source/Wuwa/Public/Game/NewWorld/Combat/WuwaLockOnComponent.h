#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"
#include "WuwaLockOnComponent.generated.h"

class AWuwaCharactorBase;

/**
 * 一套锁定参数，对应原作 db_lockon LockOnConfig（策划表"目标筛选配置"）。行名 = 原作配置 ID。
 * 字段默认值就是原作 flatbuffer 的默认值（表里没填时用）。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaLockOnConfig : public FTableRowBase
{
	GENERATED_BODY()

	/** 探测距离（厘米，三维）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn")
	float Distance = 0.f;

	/** 目标最多比自己高多少（厘米）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn")
	float UpDistance = 300.f;

	/** 目标最多比自己低多少（厘米）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn")
	float DownDistance = 800.f;

	/** 扇形半径（厘米）：探测距离外、扇形内（以镜头朝向为中心）的单位也算在范围内；0 = 没有扇形。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn")
	float SectorRadius = 0.f;

	/** 扇形张角（度，总角度）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn")
	float SectorAngle = 0.f;

	/** 容忍角（度）：与参考方向的夹角小于它的单位优先。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn")
	float ToleranceAngle = 22.5f;
};

/** 锁定到的目标：哪个单位、打在它哪根骨骼上（Socket 为空 = 单位本身的位置）。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaLockOnTarget
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "LockOn")
	TWeakObjectPtr<AWuwaCharactorBase> Actor;

	UPROPERTY(BlueprintReadOnly, Category = "LockOn")
	FName Socket;

	bool IsValid() const { return Actor.IsValid(); }
	/** 目标点：Socket 的位置，没有 Socket 时单位的 Actor 位置（胶囊中心）。 */
	FVector GetLocation() const;
};

/**
 * 锁定组件（≈ 原作 BaseLockOnComponent / CharacterLockOnComponent 的软锁定部分），挂在角色身上。
 * DetectSoftLockTarget 挑一个目标存为当前目标；技能组件在技能开始时调用它、再取当前目标作为技能目标。
 * 挑法（原作 DetectAlternativeTargets + FindTheBest）：
 *   1. 候选 = 实体表里自己周围 max(距离, 扇形半径) 内的单位；
 *   2. 过滤：自己、隐藏的（编队不在场）、不是敌对阵营、高度差超出上下距离、既不在距离内也不在扇形内、被挡住（镜头 → 目标的 Visibility 射线）；
 *   3. 参考方向由优先级决定；在"与参考方向夹角 < 容忍角"的单位里挑最近的，没有就在全部里挑最近的（精准模式没有后一步）。
 * 还没做：硬锁定、锁定部位（按骨骼）、VIP 优先名单、禁止锁定 Tag。
 */
UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaLockOnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWuwaLockOnComponent();

	/**
	 * 按锁定参数 ConfigId（0 = 角色默认的软锁定参数）和优先级挑一个目标，结果存为当前目标（可能为空）。返回当前目标是否有效。
	 * 原作 BaseLockOnComponent.DetectSoftLockTarget。
	 */
	bool DetectSoftLockTarget(int32 ConfigId, EWuwaSkillTargetPriority Priority);

	const FWuwaLockOnTarget& GetCurrentTarget() const { return CurrentTarget; }
	void ClearTarget();

	/** 锁定参数表里的一套；没有时为空。 */
	static const FWuwaLockOnConfig* FindConfig(int32 ConfigId);

	/** "系统设置"按游戏设置换成具体的挑法。 */
	static EWuwaSkillTargetPriority ResolvePriority(EWuwaSkillTargetPriority Priority);

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	AWuwaCharactorBase* GetOwnerCharacter() const;
	/** 原作 IsInAreaRole：高度差在上下距离内，且在探测距离内或扇形内。 */
	bool IsInArea(const FWuwaLockOnConfig& Config, const FVector& From, const FVector& Target) const;
	/** 原作 IsBlock（简化版）：镜头位置到目标点的 Visibility 射线被挡住（忽略自己和目标）。 */
	bool IsBlocked(const AWuwaCharactorBase* Target, const FVector& TargetLocation) const;
	FVector GetCameraLocation() const;
	FVector GetCameraForward() const;
	FVector GetInputDirection() const;
	void DrawDebug(const FWuwaLockOnConfig& Config, const TArray<AWuwaCharactorBase*>& Candidates) const;

	FWuwaLockOnTarget CurrentTarget;
	/** 上次挑目标时的输入方向（原作 HasChangeInput：输入方向没变时"摇杆方向优先"保留当前目标）。 */
	FVector LastInputDirection = FVector::ZeroVector;
	/** 调试显示用：上次用的锁定参数。 */
	int32 LastConfigId = 0;
};
