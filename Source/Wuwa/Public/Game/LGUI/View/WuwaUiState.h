#pragma once
#include "CoreMinimal.h"
#include "WuwaUiState.generated.h"

/** 界面生命周期状态：每个操作都有"进行中"和"完成"两个状态（对应鸣潮 ComponentAction）。 */
UENUM(BlueprintType)
enum class EWuwaUiState : uint8
{
	None,
	Creating,
	Created,
	Starting,
	Started,
	Showing,
	Shown,
	Hiding,
	Hidden,
	Destroying,
	Destroyed
};

enum class EWuwaUiCommand : uint8 { Create, Start, Show, Hide, Destroy };

/** 指令完成回调：操作结束（成功、失败或被跳过）时调用一次。 */
using FWuwaUiDone = TFunction<void(bool bSuccess)>;
