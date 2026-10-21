#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaUiTestAssetSetupCommandlet.generated.h"

/**
 * 准备 UI 框架测试和示例用的资产：
 * - TestPop_Prefab：加上关闭按钮（0 号）、计数文字（1 号）和 WuwaUiComponentsRegistry（已经配好就跳过，注册表和预期不符会报错）；
 * - MailBox_Prefab：邮件示例的界面（没有就新建一个空的），[0] 读一封、[1] 关闭、[2] 未读数；
 * - TestEffects_Prefab：第 7 步效果测试用，根节点挂 WuwaUiViewAnimation（已存在就跳过）；
 * - DT_UiViews：界面配置表，写入（覆盖）测试和示例界面的行，其他行不动。
 * -run=WuwaUiTestAssetSetup          只检查并打印要做的事
 * -run=WuwaUiTestAssetSetup -Apply   备份后修改并保存
 */
UCLASS()
class WUWA_API UWuwaUiTestAssetSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaUiTestAssetSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
