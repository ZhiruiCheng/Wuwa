#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaChangliAttackEffectSetupCommandlet.generated.h"

/**
 * -Attack=AttackNN 选择攻击段（默认 Attack01）。默认只检查：列出 AM_AttackNN 当前的通知与将要生成的配置。
 * -Apply：按 ArtSource/ChangliSlash/AttackNN.json 生成该段刀光的 EffectModel（每个 Niagara 一个 Model + 一个 Group），
 * 并在 AM_AttackNN 上放置 Play Effect 通知（原作每个摆放位置一个）。重复运行会替换本工具上次生成的通知。
 * 若存在 AttackNNTuning.json，同时应用人工放大（Niagara Model 的 Scale）和调色（重建材质的 Tint / Intensity）。
 * Tuning 里的 spriteLayers 会按给定发射参数整个重建对应的 Sprite 参考 Niagara 系统。
 */
UCLASS()
class WUWA_API UWuwaChangliAttackEffectSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaChangliAttackEffectSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
