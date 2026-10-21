#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WuwaAnimDataLibrary.generated.h"

class UWuwaAnimLogicParams;
class UWuwaMovementComponent;
class UAnimSequence;

/**
 * Movement 与动画之间的只读适配层。
 * 分项更新命名参考原作接口；下面的数据来源与赋值公式是本 Demo 的实现。
 * 不暴露 BlueprintCallable 更新入口，避免蓝图/工作线程重复写同一份快照。
 */
UCLASS()
class WUWA_API UWuwaAnimDataLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 仅匹配首对不同脚的标记，不跳到收尾恢复步，也不跨尾帧循环。 */
	UFUNCTION(BlueprintPure, Category = "Animation|Sync", meta = (BlueprintThreadSafe))
	static float GetStopEntryTime(const UAnimSequence* Sequence, const FMarkerSyncAnimPosition& SyncPosition, bool& bMatched);

	// 唯一采集入口：每次先清空，然后完整采集；来源失效不会保留旧角色的数据。
	static bool UpdateAnimationData(const UWuwaMovementComponent* Movement, UWuwaAnimLogicParams* Params);

private:
	static void UpdateAnimInfoMove(const UWuwaMovementComponent& Movement, UWuwaAnimLogicParams& Params);
	static void UpdateAnimInfoUnifiedState(const UWuwaMovementComponent& Movement, UWuwaAnimLogicParams& Params);
	
	//获得
	UFUNCTION(BlueprintPure, Category = "Animation|Sync", meta = (BlueprintThreadSafe))
	static float GetStartTimeFromSyncPosition(const UAnimSequence* Sequence,
											  const FMarkerSyncAnimPosition& SyncPosition);
};
