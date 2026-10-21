#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WuwaCreatureSubsystem.generated.h"

class AWuwaCharactorBase;

/**
 * 世界里所有战斗单位的登记表（≈ 原作 CreatureModel）。AWuwaCharactorBase 在 BeginPlay / EndPlay 时自动登记 / 注销。
 * 只回答"有谁、在哪"：敌我判断见 UWuwaCampLibrary，挑目标是锁定组件的事。
 * 编队里不在场的角色（隐藏、关碰撞）也在表里，需要时由调用方按在场状态过滤。
 * 服务器和客户端各有一份，各自登记本地存在的角色，不复制。
 */
UCLASS()
class WUWA_API UWuwaCreatureSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 所在世界的登记表；不是游戏世界（例如动画编辑器的预览）时为空。 */
	static UWuwaCreatureSubsystem* Get(const UObject* WorldContext);

	void Register(AWuwaCharactorBase* Creature);
	void Unregister(AWuwaCharactorBase* Creature);

	/**
	 * Location 周围 Radius（厘米）内的单位，按距离从近到远（≈ 原作 GetEntitiesInRangeWithLocation）。
	 * 距离是到单位 Actor 位置（胶囊中心）的三维距离，只用来粗筛；挑目标时的精确距离 / 角度由锁定组件按目标骨骼算。
	 * 体型大的单位中心可能在半径外而身体在半径内，调用方把半径放宽即可。
	 */
	void GetCreaturesInRange(const FVector& Location, float Radius, TArray<AWuwaCharactorBase*>& Out) const;

	/** 全部登记的单位（可能含已失效的弱引用）。 */
	const TArray<TWeakObjectPtr<AWuwaCharactorBase>>& GetCreatures() const { return Creatures; }

	DECLARE_MULTICAST_DELEGATE_OneParam(FWuwaOnCreatureChanged, AWuwaCharactorBase*);
	/** 登记后广播。 */
	FWuwaOnCreatureChanged OnCreatureRegistered;
	/** 注销前广播（以后锁定组件靠它知道"目标没了"）。 */
	FWuwaOnCreatureChanged OnCreatureUnregistered;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	TArray<TWeakObjectPtr<AWuwaCharactorBase>> Creatures;
};
