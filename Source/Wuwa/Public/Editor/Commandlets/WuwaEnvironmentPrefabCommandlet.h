#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaEnvironmentPrefabCommandlet.generated.h"

/**
 * 把区域关卡打包成可复用的"预制体"关卡（Level Instance 源关卡）：
 * 读配方里的关卡（ArtSource/Terrain/<区域>.terrain.json 的 level），只保留环境内容 —— Landscape 和物件 Actor（标签 WuwaProps）——
 * 去掉光照 / 天空 / 雾 / PlayerStart 等，另存为同目录下的 LI_<关卡名去掉 L_>。坐标保持原作世界坐标（地形和物件材质的
 * 世界色彩图按世界坐标取色，挪动会取错颜色）。用法：把 LI 关卡拖进任意关卡，得到 Level Instance，Actor 放在原点即可。
 * 默认只读；-Apply 才写入（已有的 LI 关卡先备份到 Saved/Backups/EnvironmentPrefab/<时间>/）。
 */
UCLASS()
class WUWA_API UWuwaEnvironmentPrefabCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaEnvironmentPrefabCommandlet();
	virtual int32 Main(const FString& Params) override;
};
