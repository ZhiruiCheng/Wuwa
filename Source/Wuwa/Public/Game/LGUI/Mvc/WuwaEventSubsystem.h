#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WuwaEventSubsystem.generated.h"

using FWuwaEventHandler = TFunction<void(const FInstancedStruct& Payload)>;

/**
 * 全局事件（≈ 鸣潮 EventSystem）：Controller 改完 Model 发事件，View 和其他模块监听。
 * 事件用 GameplayTag 标识（≈ EEventName），只匹配完全相同的 Tag；参数是一个 USTRUCT，用 FInstancedStruct 装着。
 * 放在 GameInstance 上：玩法代码（不经过 UI）也能发，切地图不受影响。
 */
UCLASS()
class WUWA_API UWuwaEventSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	static UWuwaEventSubsystem* Get(const UObject* WorldContext);

	/** Owner 不为空时：Owner 被销毁后自动失效，也能用 RemoveAll(Owner) 一次性移除（≈ AddWithTarget / RemoveTargetEvents）。 */
	FDelegateHandle Add(FGameplayTag Event, const UObject* Owner, FWuwaEventHandler Handler);
	void Remove(FGameplayTag Event, FDelegateHandle Handle);
	void RemoveAll(const UObject* Owner);

	void Emit(FGameplayTag Event, const FInstancedStruct& Payload = FInstancedStruct());
	template <typename T>
	void Emit(FGameplayTag Event, const T& Payload) { Emit(Event, FInstancedStruct::Make(Payload)); }

	/** 现在还有效的监听数（调试、测试用）。 */
	int32 NumListeners(FGameplayTag Event) const;

private:
	struct FListener
	{
		FDelegateHandle Handle;
		TWeakObjectPtr<const UObject> Owner;
		bool bHasOwner = false;
		TSharedRef<FWuwaEventHandler> Handler;
	};

	bool IsAlive(const FListener& Listener) const { return !Listener.bHasOwner || Listener.Owner.IsValid(); }
	bool IsRegistered(FGameplayTag Event, FDelegateHandle Handle) const;

	TMap<FGameplayTag, TArray<FListener>> Listeners;
};
