#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaHudAssetSetupCommandlet.generated.h"

/**
 * 主界面 HUD 的贴图和 LGUI 预制体：
 * - 导入 ArtSource/UI/HUD/Textures/*.png（由 ArtSource/UI/HUD/extract_hud_sprites.py 从鸣潮 UI 资源里提取）
 *   → /Game/LGUI/HUD/Textures/T_HUD_*（UI 贴图设置：不生成 mip、Clamp）；
 * - 每个 HUD 元素搭一个预制体 → /Game/LGUI/HUD/HUD_*，再把它们按截图的位置拼成 HUD_Main。
 *
 * 必须带 -AllowCommandletRendering -RenderOffscreen 运行：LGUI 按贴图实际尺寸算 UITexture 的 UV，NullRHI 下尺寸是 0，UV 会变成 NaN。
 *
 * -run=WuwaHudAssetSetup                    只打印要做的事
 * -run=WuwaHudAssetSetup -Apply             导入缺少的贴图、生成缺少的预制体（已有的不动，方便手工调整后保留）
 * -run=WuwaHudAssetSetup -Apply -Rebuild    重新导入所有贴图、重新生成所有预制体（覆盖前备份）
 */
UCLASS()
class WUWA_API UWuwaHudAssetSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaHudAssetSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
