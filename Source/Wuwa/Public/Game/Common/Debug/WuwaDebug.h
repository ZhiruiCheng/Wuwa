#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WuwaDebug.generated.h"

/**
 * 调试打印的频道。每个频道一个控制台开关 wuwa.Debug.<频道名>（0 关 / 1 开，默认关），
 * 输入 wuwa.Debug.List 列出所有开关和当前状态。
 */
UENUM(BlueprintType)
enum class EWuwaDebugChannel : uint8
{
	/** 角色运动状态：Position / Move / Direction / Gait / Action Override / Revision（BP_RoleBase 每帧）。 */
	MoveState,
	/** 动画参数：左右脚进度、播放速率、步态（ABP_RoleBase 每帧）。 */
	AnimParams,
	/** 战斗输入：当前主技能状态和预输入缓存（C# CombatInputDebug 每帧）。 */
	CombatInput,
	/** 攀爬：检测射线的调试线（每帧）和"进入攀爬"提示（UWuwaMovementComponent）。 */
	Climb,
	/** 锁定：挑目标时画探测距离和候选（2 秒），当前目标一直画红线（UWuwaLockOnComponent）。 */
	LockOn,
	/** 位移吸附：技能窗口内每帧画到目标的线和停止距离圈（UWuwaMovementComponent）。 */
	SkillMotion,
};

UCLASS()
class WUWA_API UWuwaDebugLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 这个频道的开关开着没有。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Debug")
	static bool IsDebugChannelEnabled(EWuwaDebugChannel Channel);

	/**
	 * 带频道的 Print String：参数和引擎的 Print String 一样，多一个 Channel；频道开关关着时什么都不做（不上屏、不写日志）。
	 * 和 Print String 一样只在开发版本里有（打包 Shipping 时整个调用被去掉）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Debug", meta = (WorldContext = "WorldContextObject", CallableWithoutWorldContext,
		Keywords = "log print debug", AdvancedDisplay = "3", DevelopmentOnly, DisplayName = "Wuwa Debug Print"))
	static void DebugPrint(const UObject* WorldContextObject, EWuwaDebugChannel Channel, const FString& InString = FString(TEXT("Hello")),
		bool bPrintToScreen = true, bool bPrintToLog = true, FLinearColor TextColor = FLinearColor(0.0f, 0.66f, 1.0f), float Duration = 2.f,
		const FName Key = NAME_None);
};
