#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaUiLayoutSetupCommandlet.generated.h"

/**
 * UI 铺满屏幕的资产配置（直接改预制体，编辑器里看得到）：
 * - LP_UIRoot 根画布的 LGUICanvasScaler：ScaleWithScreenSize，参考分辨率 1920x1080，按高度缩放；
 * - HUD_Main 根节点：横竖拉伸铺满所在层。
 * 默认只读审计；-Apply 才修改，修改前备份到 Saved/Backups/UiLayoutSetup/<时间>/。用 LGUI 自己的 PrefabHelperObject 加载/保存，嵌套子预制体保持不变。
 */
UCLASS()
class WUWA_API UWuwaUiLayoutSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaUiLayoutSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
