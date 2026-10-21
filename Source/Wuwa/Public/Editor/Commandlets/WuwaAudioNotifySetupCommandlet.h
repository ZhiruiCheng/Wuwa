#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaAudioNotifySetupCommandlet.generated.h"

/**
 * 按 ArtSource/Audio/<角色>/audio.json（prepare_audio.py 生成）的 notifies，把原作的两种音频通知装到项目动画上：
 * 瞬时 → Wwise Audio Event（喊招语音，蒙太奇），持续 → Wwise Audio Event Window（动作音效，动画序列）。
 * 事件资产按名字在 /Game/WwiseAudio 下找（先在 Wwise 里生成 SoundBank，再跑 WwiseReconcileCommandlet）。
 * -Character=changli；默认只检查，-Apply 才修改并保存（先备份到 Saved/Backups/AudioNotifySetup）。
 * 重复运行会替换本工具上次在 "WwiseAudio" 轨道上装的通知。
 */
UCLASS()
class WUWA_API UWuwaAudioNotifySetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaAudioNotifySetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
