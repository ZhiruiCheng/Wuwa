#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "WuwaPoolableActor.generated.h"

/**
 * 被对象池管理的 Actor 可选实现：取出 / 还回时由池通知它恢复自己的状态
 * （代替原作引擎扩展的 ResetActorToDefault 和自定义清理函数）。
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UWuwaPoolableActor : public UInterface
{
	GENERATED_BODY()
};
//如果是蓝图或 C# 实现的接口，C++ 这边看不到真正的函数，只能通过生成的 Execute_ 函数去调用。
class WUWA_API IWuwaPoolableActor
{
	GENERATED_BODY()

public:
	/** 从池里取出之后调用：把自己恢复成刚生成时的样子 **/
	UFUNCTION(BlueprintNativeEvent, Category = "Wuwa|ActorPool") void OnTakenFromPool();
	/** 还回池之前调用：清掉自己的状态（停特效、解绑回调……）。 */
	UFUNCTION(BlueprintNativeEvent, Category = "Wuwa|ActorPool") void OnReturnedToPool();
};
