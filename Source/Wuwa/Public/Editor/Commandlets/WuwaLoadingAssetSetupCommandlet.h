#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaLoadingAssetSetupCommandlet.generated.h"

/**
 * 加载界面的资产：
 * - 贴图：ArtSource/UI/Loading/Textures/*.png → /Game/LGUI/Loading/Textures（先跑 ArtSource/UI/Loading/extract_loading_art.py）；
 * - 预制体 /Game/LGUI/Loading/Loading_Prefab（背景、底部渐变、标题、描述、进度条、百分比 + 注册表）；
 * - 展示表 /Game/LGUI/Loading/DT_LoadingShow（FWuwaLoadingShowRow）；
 * - DT_UiViews 的 LoadingView 行（加载层）。
 * 默认只读审计；-Apply 才写入，已有资产先备份到 Saved/Backups/LoadingAssetSetup/<时间>/。
 * 需要 -AllowCommandletRendering -RenderOffscreen（UITexture 的 UV 要用贴图的实际尺寸算）。
 */
UCLASS()
class WUWA_API UWuwaLoadingAssetSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaLoadingAssetSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
