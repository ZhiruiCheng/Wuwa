#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Engine/DataTable.h"
#include "GameplayEffectComponent.h"
#include "WuwaBuffTypes.generated.h"

class UGameplayEffect;
class UTexture2D;

/**
 * buff 表（DT_Buff）的一行：行名 = 原作 buff ID（db_buff.db 的 Id），指向这个 buff 的 GameplayEffect。
 * 原作的 buff 字段（持续方式、时长、授予 Tag、持续 / 移除 Tag 条件、叠层、cue）几乎一一对应 GE 的字段，由 WuwaBuffSetup 按原作数据生成 GE 资产。
 * GE 是硬引用（只有几 KB）：表加载时一起加载，加 buff 时不会同步加载卡一下。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaBuffRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff")
	TSubclassOf<UGameplayEffect> Effect;
};

/**
 * buff 表现表（DT_BuffCue）的一行：行名 = 原作 GameplayCue ID（db_buffeffect.db 的 gameplaycue）。
 * 目前只导入 UI 用的 cue（buff 图标）；原作同一张表里还有特效、材质控制器等 cue，项目还没接。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaBuffCueRow : public FTableRowBase
{
	GENERATED_BODY()

	/**
	 * 原作 CueType（UI 相关的几种，见 BuffItemContainer.AddBuffByCue）：
	 *   2 普通 buff 图标（RoleBuffView，每个 buff 一个）、14 同一 cue 的 buff 合并成一个图标并显示个数、
	 *   24 角色专属 buff 栏（RoleUniqueBuffView，血条上方）、37 …
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	int32 CueType = 0;

	/** 图标（原作 Path）。硬引用：表加载时一起加载。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	TObjectPtr<UTexture2D> Icon;

	/** 排序：大的在前，同优先级新的在前（原作 BuffItemInfo.Compare）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	int32 Priority = 0;

	/**
	 * 原作 Parameters（BuffItem.Activate 里的用法）：
	 *   [0] "1" 增益角标 / "2" 减益角标；[1] 倒计时填充色（十六进制 RRGGBBAA，空 = FFFFFF7F）；[2] 图标颜色（空 = 白）；
	 *   [3] 图标样式（1 大图标、2 / 4 不显示倒计时、3 / 4 彩色边框）；[4] "1" = 简化显示时隐藏。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	TArray<FString> Parameters;
};

/**
 * 挂在 buff 的 GE 上：这个 GE 是哪个原作 buff、有哪些表现（cue）。
 * 从一个活动中的 GE 反查 buff ID / 图标时读它，不用查表。
 */
UCLASS(DisplayName = "Wuwa Buff Info", CollapseCategories)
class WUWA_API UWuwaBuffInfoComponent : public UGameplayEffectComponent
{
	GENERATED_BODY()

public:
	/** 原作 buff ID（DT_Buff 的行名）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buff")
	int64 BuffId = 0;

	/** 原作 GameplayCueIds（DT_BuffCue 的行名）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Buff")
	TArray<int64> CueIds;
};

/** UI 用的一个 buff 图标（一个活动 GE 的一个 UI cue）。由 UWuwaFormationUiController 从队员 ASC 里整理出来。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaBuffUiData
{
	GENERATED_BODY()

	FActiveGameplayEffectHandle Handle;
	UPROPERTY(BlueprintReadOnly, Category = "Buff") int64 BuffId = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Buff") int64 CueId = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Buff") int32 CueType = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Buff") TObjectPtr<UTexture2D> Icon;
	UPROPERTY(BlueprintReadOnly, Category = "Buff") int32 Priority = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Buff") TArray<FString> Parameters;
	/** 总时长（秒）；≤ 0 = 永久（不显示倒计时）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Buff") float Duration = 0.f;
	/** 结束时的世界时间（本机 World->GetTimeSeconds()）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Buff") double EndWorldTime = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Buff") int32 StackCount = 1;
	/** 加上的先后（越大越新），排序用。 */
	UPROPERTY(BlueprintReadOnly, Category = "Buff") int64 SortId = 0;

	/** 同一个图标：同一个 GE、同一个 cue。 */
	bool IsSameIcon(const FWuwaBuffUiData& Other) const { return Handle == Other.Handle && CueId == Other.CueId; }
	/** 剩余比例 0~1（永久 buff 为 1）。 */
	float GetRemainPercent(double WorldTime) const
	{
		return Duration > 0.f ? FMath::Clamp(static_cast<float>((EndWorldTime - WorldTime) / Duration), 0.f, 1.f) : 1.f;
	}
};
