#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaRoleAttributeSetupCommandlet.generated.h"

/**
 * 角色属性数据：读 ArtSource/Role/prepare_role_attributes.py 生成的配方（原作 BaseProperty + RolePropertyGrowth），写进项目资产：
 *   -run=WuwaRoleAttributeSetup -Recipe="C:/UEProjects/Wuwa/ArtSource/Role/role_attributes.recipe.json" [-Apply]
 * - DT_RoleBaseProperty（行名 = 原作 PropertyId，FWuwaRoleBaseProperty）、DT_RolePropertyGrowth（行名 "<等级>_<突破>"，FWuwaRolePropertyGrowth）整表重写；
 * - 角色资产（DA_Role_*）的 BasePropertyId；其他字段不动。
 * UWuwaRoleSettings 的两张表在 Config/DefaultGame.ini 里配。角色生成时由 UWuwaRoleAttributeInit 按这些数据初始化 ASC 属性。
 * 默认只检查；-Apply 才写（已有的资产先备份到 Saved/Backups/RoleAttributeSetup/<时间>/）。
 */
UCLASS()
class WUWA_API UWuwaRoleAttributeSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaRoleAttributeSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
