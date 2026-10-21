#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaUiEffectSetupCommandlet.generated.h"

/**
 * 界面特效（原作 NS_Fx_LGUI_*，鸣潮自研 LGUI 的 UINiagara 播的那些）重建：读 ArtSource/UI/KuroPrefab/prepare_ui_effects.py 的配方，
 *   -run=WuwaUiEffectSetup -Recipe="C:/UEProjects/Wuwa/ArtSource/UI/HUD/Kuro/Effects/ui_effects.recipe.json" [-Apply]
 * - 贴图、网格（Geometry JSON；网格开 Allow CPU Access，UWuwaUINiagara 运行时要读顶点）导入到配方的 folder；
 * - 每个材质定义生成 M_<id>：Surface + Unlit（LGUI 画布能用）、按定义叠加 / 半透明、双面；贴图的某个通道当遮罩，乘粒子顶点色；
 * - 每个系统用 WuwaUiNiagaraBuilder 重建（CPU、局部空间、出生 / 更新表达式），由 UWuwaUINiagara 在界面里播放。
 * 默认只检查；-Apply 才写（已有的资产先备份到 Saved/Backups/UiEffectSetup/<时间>/）。
 */
UCLASS()
class WUWA_API UWuwaUiEffectSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaUiEffectSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
