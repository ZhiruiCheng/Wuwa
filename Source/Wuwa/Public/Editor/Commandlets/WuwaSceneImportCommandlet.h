#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaSceneImportCommandlet.generated.h"

/**
 * 整个场景的 glTF（Tools/Blender/export_scene_gltf.py 从 .blend 导出）→ 新关卡：用 Interchange 场景导入
 * （默认的场景管线：网格、材质、贴图、灯光一起建，每个物体一个 Actor，相同网格共用一个资产）。
 *  - 资产放在 /Game/Environment/<Name>/（Interchange 按类型分子目录）；
 *  - 关卡 /Game/Environment/<Name>/Maps/L_<Name>：导入的 Actor 打上 WuwaProps 标签（之后 WuwaEnvironmentPrefab 可以
 *    只保留它们做成 LI_ 预制体），再放一套户外光照和场景中心上方地面上的 PlayerStart。
 * 默认只读；-Apply 才写入（已有关卡先备份到 Saved/Backups/SceneImport/<时间>/ 再整关重建）。
 */
UCLASS()
class WUWA_API UWuwaSceneImportCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaSceneImportCommandlet();
	virtual int32 Main(const FString& Params) override;
};
