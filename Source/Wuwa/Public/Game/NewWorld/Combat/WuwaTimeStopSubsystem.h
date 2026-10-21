#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WuwaTimeStopSubsystem.generated.h"

/**
 * 大招时停（原作 TimeController.AddLock / TsAnimNotifyStateAbsoluteTimeStop「动画和子弹冻结」）：
 * 有人开时停期间，200 米内除了开时停的角色以外的单位全部暂停（冻结移动时用 CustomTimeDilation = 0：动画、移动一起停；
 * 不冻结移动时只停动画），它们的子弹也冻结（不计时、不检测命中）。所有锁都撤掉后恢复。
 * 原作 TimeScale 参数 > 0 的"减速"版本没有用到，不做；原作开时停的角色不受其他时间缩放影响（NormalizeTimeScaleLock）也不做。
 */
UCLASS()
class WUWA_API UWuwaTimeStopSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWuwaTimeStopSubsystem* Get(const UObject* WorldContext);

	/** 原作 AddLock：同一个角色重复开时停不处理（返回 false）。 */
	bool BeginAbsoluteTimeStop(AActor* Caster, bool bStopMove = true);
	void EndAbsoluteTimeStop(AActor* Caster);

	/** 这个单位正被时停冻住（子弹系统据此冻结它的子弹）。 */
	bool IsFrozen(const AActor* Actor) const;
	/** 冻住且停移动（CustomTimeDilation 应为 0）。 */
	bool IsMovementFrozen(const AActor* Actor) const;
	bool IsTimeStopActive() const { return Locks.Num() > 0; }

	/** 原作 TIME_STOP_DISTANCE。 */
	static constexpr double Range = 20000.0;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

private:
	void Refresh();
	void Freeze(AActor* Actor, bool bStopMove);
	void Unfreeze(AActor* Actor);

	struct FFrozen
	{
		float SavedDilation = 1.f;
		bool bStopMove = true;
	};
	TMap<TWeakObjectPtr<AActor>, bool> Locks;
	TMap<TWeakObjectPtr<AActor>, FFrozen> Frozen;
};
