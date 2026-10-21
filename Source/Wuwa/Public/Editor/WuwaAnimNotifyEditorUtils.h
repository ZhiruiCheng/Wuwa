#pragma once

#include "CoreMinimal.h"

class UAnimSequenceBase;

#if WITH_EDITOR
namespace WuwaAnimNotifyEditor
{
	/**
	 * 按 UE 编辑器放通知时的规则刷新通知的触发偏移（FAnimNotifyEvent::RefreshTriggerOffset / RefreshEndTriggerOffset）：
	 * 放在第 0 帧的通知要"提前一点"触发，否则瞬时通知永远不触发（取区间时要求 结束 > 上一帧位置）。
	 * 命令行用 Link 加通知后、保存前调一次（不调的话原作第 0 帧的子弹 / 镜头等通知全部丢失）。
	 */
	WUWA_API void RefreshTriggerOffsets(UAnimSequenceBase* Animation);
}
#endif
