#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaAnimFbxImportCommandlet.generated.h"

/**
 * 把 Blender 导出的角色 FBX（changli2.0.fbx / jiabei2.0.fbx，含全部动作）导入成动画序列：
 * - 用 Interchange 只导入动画，绑定到已有骨骼（-Skeleton）；资产名 = FBX 文件名 + 动作名，和以前手动导入的一致；
 * - 先导入到空的临时文件夹（直接导入到已有资产的文件夹时 Interchange 会静默地什么都不产出），从不保存它；
 * - -Search 下同名的已有动画（不管在哪个文件夹）用导入结果的数据原地覆盖（引用、通知、根运动等设置都保留）；
 * - 没有同名的新动画移进 -Dest；
 * - 原来挂了「Wuwa Promote Imported Root Motion」修改器的资产导入后重新应用；新动画如果 Root 骨骼有位移也挂上并应用。
 *
 * 默认只预演：导入到 /Game/__WuwaAnimImportPreview，不保存，只出报告。
 * -Apply 才写入：先把要覆盖的 .uasset 复制到 Saved/Backups/AnimFbxImport/<时间>/，再导入并保存。
 *
 * UnrealEditor-Cmd.exe Wuwa.uproject -run=WuwaAnimFbxImport -Fbx=C:/.../changli2.0.fbx
 *   -Skeleton=/Game/Characters/Role/changli/NewModel/changli2_0_Skeleton
 *   -Dest=/Game/Characters/Role/changli/NewModel -Search=/Game/Characters/Role/changli [-Apply]
 */
UCLASS()
class WUWAANIMATIONEDITOR_API UWuwaAnimFbxImportCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaAnimFbxImportCommandlet();
	virtual int32 Main(const FString& Params) override;
};
