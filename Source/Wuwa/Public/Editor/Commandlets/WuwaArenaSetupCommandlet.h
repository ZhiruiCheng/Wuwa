#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaArenaSetupCommandlet.generated.h"

/**
 * 战斗场地的摆放和渲染（场景本身由 WuwaSceneImport 导入）：读 ArtSource/Environment/<名字>.arena.json，
 *   -run=WuwaArenaSetup -Recipe=<abs>.arena.json [-Apply]
 * - 场地中心（配方 center 的 XY）往下打射线找最上面一层地面；
 * - Boss 预览：中心地面上放一个骨骼网格 Actor（网格 + 循环待机动画 + 武器挂骨骼插槽），面朝玩家出生点；
 *   重跑时先删掉上次放的（标签 WuwaArenaBoss）。只是摆着看的，之后做敌人时换成真正的敌人角色；
 * - PlayerStart 挪到 Boss 正前方 playerDistance 处、面朝 Boss；
 * - 光照：太阳角度 / 强度 / 色温、天光、高度雾（贴着场地以下的高度，让塔下远景有空气感），一个无边界的后期体积（曝光、泛光、调色）。
 * 默认只读；-Apply 才写（关卡先备份到 Saved/Backups/ArenaSetup/<时间>/）。-ProbeFloor：只打印场地地面高度剖面（排查碰撞）。
 * 不要加 -AllowCommandletRendering：命令行里给骨骼网格组件建渲染 / 动画状态会崩（这里也不需要渲染）。
 */
UCLASS()
class WUWA_API UWuwaArenaSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaArenaSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
