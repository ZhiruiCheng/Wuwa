#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaBulletNotifySetupCommandlet.generated.h"

/**
 * 子弹通知：ArtSource/Bullet/<角色>.bullet_notifies.json（prepare_bullet_notifies.py 从原作蒙太奇的 TsAnimNotifyReSkillEvent 生成）
 * → 项目蒙太奇上的 Spawn Bullet 通知（UWuwaAnimNotify_SpawnBullet，放在 "Bullet" 轨道）。
 * 每个蒙太奇上原有的 Spawn Bullet 通知先全部移除再按配方装，可以重复运行。
 * 核对：项目蒙太奇时长与原作同长（或整段拉长 1.25 倍，时间跟着缩放），否则不装这个蒙太奇；
 * 子弹 ID 必须在角色蓝图的 BulletTable 里；子弹所属技能（ID / 1000）应是技能表里用这个蒙太奇的技能，否则警告。
 * 默认只读；-Apply 才写入，先备份到 Saved/Backups/BulletNotifySetup/<时间>/。
 */
UCLASS()
class WUWA_API UWuwaBulletNotifySetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaBulletNotifySetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
