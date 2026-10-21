#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WuwaCombatEditorLibrary.generated.h"

class UDataTable;

/** 战斗编辑器工具的脚本入口（编辑器 Python / 蓝图工具调用）。 */
UCLASS()
class UWuwaCombatEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** 打开（或切到）子弹编辑器并显示这张子弹表；RowName 非空时选中那一行。和右键菜单"在子弹编辑器中打开"相同。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Combat")
	static bool OpenBulletEditor(UDataTable* Table, FName RowName);

	/** 在新的 PIE 窗口里开始游戏（客户区 Width x Height，左上角在屏幕 X,Y），不占用关卡视口。录演示视频用。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor")
	static bool StartPIEInNewWindow(int32 Width = 1920, int32 Height = 1080, int32 X = 0, int32 Y = 0);
};
