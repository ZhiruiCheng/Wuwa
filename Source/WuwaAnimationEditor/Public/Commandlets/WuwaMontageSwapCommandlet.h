#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaMontageSwapCommandlet.generated.h"

/**
 * 把单片段蒙太奇里的动画换成新导入的版本（比如闪避换成带飘带、30fps 的 Dash_F）：
 * - 片段换成新动画，起止时间、蒙太奇通知、分段按 新时长/旧时长 缩放；
 * - 旧动画的根运动设置（bEnableRootMotion、RootLock 等）抄到新动画；旧动画挂了根运动修改器而新动画没有的，补上并应用；
 * - 旧动画序列上的通知，在新动画没有通知时按比例搬过去。
 * 默认只检查并报告，-Apply 才修改并保存（先备份到 Saved/Backups/MontageSwap/<时间>/）。
 *
 * -Pairs=<蒙太奇路径>=<新动画路径>;<蒙太奇路径>=<新动画路径>...
 */
UCLASS()
class WUWAANIMATIONEDITOR_API UWuwaMontageSwapCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaMontageSwapCommandlet();
	virtual int32 Main(const FString& Params) override;
};
