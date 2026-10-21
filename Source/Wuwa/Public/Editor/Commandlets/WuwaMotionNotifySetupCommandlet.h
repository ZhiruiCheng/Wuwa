#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaMotionNotifySetupCommandlet.generated.h"

/**
 * 位移吸附 / 转向通知：ArtSource/Combat/<角色>.motion_notifies.json（prepare_motion_notifies.py 从原作蒙太奇生成）
 * → 项目蒙太奇上的 Move To Target / Skill Rotate / Role Rotate 窗口（放在 "Motion" 轨道）。
 * 每个蒙太奇上原有的这三类通知先全部移除再按配方装，可以重复运行。
 * 核对：项目蒙太奇时长与原作同长（或整段拉长 1.25 倍，开始时间和时长跟着缩放），否则不装这个蒙太奇；配方里的字段必须是项目通知（或其 Config）的属性。
 * 默认只读；-Apply 才写入，先备份到 Saved/Backups/MotionNotifySetup/<时间>/。
 */
UCLASS()
class WUWA_API UWuwaMotionNotifySetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaMotionNotifySetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
