#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaKuroHudBuildCommandlet.generated.h"

/**
 * 按原作 UI 预制体还原界面：读 ArtSource/UI/KuroPrefab/kuro_export.py 导出的布局（节点、LGUI3 锚点、贴图、文字），生成 LGUI 预制体。
 *   -run=WuwaKuroHudBuild -Layout="C:/UEProjects/Wuwa/ArtSource/UI/HUD/Kuro/hud_main.layout.json" -Prefab=/Game/LGUI/HUD/HUD_Main [-Apply]
 *   （-Layout 要带引号的绝对路径：不带引号时 FParse 会在 ".layout" 处截断）
 *       [-AddView=SubtitleTips -Layer=Float]  -AllowCommandletRendering -RenderOffscreen
 * - 贴图导入到 <预制体目录>/Kuro/Textures（按 PNG 文件名），九宫格的边距写进 UITexture 的 spriteData（Sliced）；
 * - 布局里有 fill 的节点（原作 Filled 精灵，比如协奏圈）做成 UISprite + Filled，精灵资产从同一张贴图生成到 <预制体目录>/Kuro/Sprites；
 * - type 为 Niagara 的节点做成 AWuwaUINiagaraActor（UWuwaUINiagara：界面里的 Niagara 特效，系统由 -run=WuwaUiEffectSetup 重建）；
 * - 布局里 button 的节点挂 UIButtonComponent（根节点接收点击，颜色过渡作用在第一个图标上）；
 * - 布局里节点的 registry（下标数组）→ 这个节点挂 WuwaUiComponentsRegistry，依次放这些节点（导出规格的 registries 决定；
 *   HUD_Main：根 [三个队伍槽, 技能栏, 角色状态]，队伍槽 [根, 头像, 血条, 数字, 边框] —— UWuwaMainHudView / 各 Panel 的约定）；
 * - -Font=<TTF> -FontAsset=/Game/...：把 TTF 做成 LGUI SDF 字体（嵌进资产）并给所有文字用；
 * - -AddView：在 DT_UiViews 里加 / 覆盖一行（视图类用默认的 UWuwaUiView），比如把字幕做成 Float 层的提示。
 * 默认只检查；-Apply 才写（已有的预制体、表先备份到 Saved/Backups/KuroHudBuild/<时间>/）。需要真实渲染（贴图尺寸）。
 */
UCLASS()
class WUWA_API UWuwaKuroHudBuildCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaKuroHudBuildCommandlet();
	virtual int32 Main(const FString& Params) override;
};
