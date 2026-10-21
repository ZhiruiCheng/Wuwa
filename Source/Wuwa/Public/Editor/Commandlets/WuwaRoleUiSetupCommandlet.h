#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaRoleUiSetupCommandlet.generated.h"

/**
 * 角色特有的 UI 数据：读 ArtSource/UI/KuroPrefab/prepare_role_ui.py 生成的配方（原作 RoleInfo / SkillButton / DT_SkillInfo /
 * ElementInfo / DT_SpecialEnergyBarInfo 里查出来的），写进项目资产：
 *   -run=WuwaRoleUiSetup -Recipe="C:/UEProjects/Wuwa/ArtSource/UI/HUD/Kuro/role_ui.recipe.json" [-Apply]
 * - 图片导入到配方的 textureDir（UI 贴图：不压缩；界面上都是缩小显示，生成 mip 防锯齿）；
 * - 角色资产（DA_Role_*）：HeadIcon、ElementTag、SkillButtons、SpecialEnergyBarId；其他字段不动；
 * - DT_ElementInfo（行名 = 元素标签，FWuwaElementUiInfo）、DT_SpecialEnergyBarInfo（行名 = 能量条 Id，FWuwaSpecialEnergyBarInfo）整表重写。
 *   能量条预制体本身由 -run=WuwaKuroHudBuild 生成（EB_<Id>），这里只引用。
 * UWuwaUISettings 的 ElementTable / SpecialEnergyBarTable 在 Config/DefaultGame.ini 里配。
 * 默认只检查；-Apply 才写（已有的资产先备份到 Saved/Backups/RoleUiSetup/<时间>/）。
 */
UCLASS()
class WUWA_API UWuwaRoleUiSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaRoleUiSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
