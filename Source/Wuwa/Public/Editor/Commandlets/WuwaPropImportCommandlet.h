#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaPropImportCommandlet.generated.h"

/**
 * 区域内的原作静态物件 → 项目资产 + 关卡摆放：读 ArtSource/Terrain/<区域>.props.json（prepare_props.py 生成）。
 *  - 贴图：PNG → UTexture2D（法线按法线贴图压缩）；
 *  - 材质：两个通用主材质 /Game/Environment/Common/Props/M_WuwaProp（不透明）和 M_WuwaPropMasked（遮罩、双面），
 *    每个原作材质实例一个材质实例（漫反射、法线）；
 *  - 网格：glb 用 Interchange 导入（先导到临时目录再移到目标路径，目标路径沿用原作目录结构），按槽位挂上材质，
 *    配方标了 collision 的用复杂碰撞（攀爬要贴合表面），不透明的开 Nanite；
 *  - 摆放：关卡里每个类别（Rocks / Foliage / Structures）一个 Actor，每种网格一个 HISM，实例 = 原作世界变换。
 *    重跑会先删掉上次生成的摆放（Actor 标签 WuwaProps）。
 * 资产已存在的默认跳过（-Reimport 重新导入）；-Limit=N 只处理配方里前 N 种网格（按摆放数量排序）。
 * 默认只读；-Apply 才写入（会话里新建/修改的资产和关卡，关卡先备份到 Saved/Backups/PropImport/<时间>/）。
 */
UCLASS()
class WUWA_API UWuwaPropImportCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaPropImportCommandlet();
	virtual int32 Main(const FString& Params) override;
};
