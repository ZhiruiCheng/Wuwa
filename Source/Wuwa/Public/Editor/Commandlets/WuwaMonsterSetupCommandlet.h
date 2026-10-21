#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaMonsterSetupCommandlet.generated.h"

/**
 * 怪物蓝图：按配方（ArtSource/Boss/<怪物>.monster.json）建 / 更新一个继承 AWuwaCharactorBase 的蓝图：
 * 阵营、胶囊、网格（偏移 / 朝向 / 单节点待机动画）、子弹表、武器（构造脚本里的骨骼网格组件，挂在网格插槽上）。
 * 没有 AI 前不自动生成 AI 控制器。默认只读；-Apply 才写入，已有蓝图先备份到 Saved/Backups/MonsterSetup/<时间>/。
 */
UCLASS()
class WUWA_API UWuwaMonsterSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaMonsterSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
