#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaCharacterToonSetupCommandlet.generated.h"

/**
 * 角色仿鸣潮材质。读 ArtSource/CharacterToon/prepare_toon.py 生成的配方（原作材质参数）：
 *   -run=WuwaCharacterToonSetup -Recipe=ArtSource/CharacterToon/jiabei.toon.json -Mesh=/Game/Characters/Role/jiabei/Mesh/jiabei2_0 [-Apply]
 * 不带 -Apply 只审计网格（UV 套数、顶点色）和配方里的贴图能否在项目里找到。
 * -Apply：
 *  - 生成通用描边母材质 /Game/Characters/Common/Toon/M_Toon_Outline（项目自建，不含原作数据）；
 *  - 每个有原作 _OL 材质的槽位生成描边材质实例（原作参数值），挂到网格该槽位的 Overlay Material 上。
 * 改网格前备份到 Saved/Backups/CharacterToonSetup。
 */
UCLASS()
class WUWA_API UWuwaCharacterToonSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaCharacterToonSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
