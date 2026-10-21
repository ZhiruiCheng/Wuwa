#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WuwaBuffLibrary.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UWuwaBuffInfoComponent;
struct FActiveGameplayEffect;
struct FWuwaBuffCueRow;

/**
 * 按原作 buff ID 操作 buff（≈ 原作 BaseBuffComponent.AddBuff / RemoveBuff / GetBuffById）。
 * buff 本身是 GAS 的 GameplayEffect，存在角色的 ASC 里；这里只是"buff ID → GE"（DT_Buff）的一层包装，
 * 动画通知、技能、UI 都只认 buff ID，不直接引用 GE 资产。
 * 加和移除只在有权威的一端做（原作 HasBuffAuthority），GE 会复制给客户端。
 */
UCLASS()
class WUWA_API UWuwaBuffLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** DT_Buff 里这个 buff 的 GE；没有时为空。 */
	static TSubclassOf<UGameplayEffect> FindBuffEffect(int64 BuffId);

	/** 给 Target 加 buff（Instigator 为空时算自己加的）；失败返回无效句柄。瞬时 GE（比如"移除某 buff"）执行后句柄也无效。 */
	static FActiveGameplayEffectHandle AddBuff(UAbilitySystemComponent* Target, int64 BuffId, UAbilitySystemComponent* Instigator = nullptr);

	/** 移除 Target 身上这个 buff 的 StackCount 层（-1 = 全部）；返回移除的 GE 个数。 */
	static int32 RemoveBuff(UAbilitySystemComponent* Target, int64 BuffId, int32 StackCount = -1);

	/** Target 身上有没有这个 buff。 */
	static bool HasBuff(const UAbilitySystemComponent* Target, int64 BuffId);

	/** GE 上的 buff 信息组件（不是 buff 的 GE 返回空）。 */
	static const UWuwaBuffInfoComponent* GetBuffInfo(const UGameplayEffect* Effect);
	/** 活动 GE 是哪个 buff；不是 buff 返回 0。 */
	static int64 GetBuffId(const FActiveGameplayEffect& Effect);

	/** DT_BuffCue 的一行；没有时为空。 */
	static const FWuwaBuffCueRow* FindCue(int64 CueId);

	UFUNCTION(BlueprintCallable, Category = "Wuwa|Buff", meta = (DisplayName = "Add Buff"))
	static bool K2_AddBuff(UAbilitySystemComponent* Target, int64 BuffId);
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Buff", meta = (DisplayName = "Remove Buff"))
	static int32 K2_RemoveBuff(UAbilitySystemComponent* Target, int64 BuffId, int32 StackCount = -1);
	UFUNCTION(BlueprintPure, Category = "Wuwa|Buff", meta = (DisplayName = "Has Buff"))
	static bool K2_HasBuff(const UAbilitySystemComponent* Target, int64 BuffId);
};
