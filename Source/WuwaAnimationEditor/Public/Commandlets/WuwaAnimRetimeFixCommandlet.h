#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaAnimRetimeFixCommandlet.generated.h"

/**
 * WuwaAnimFbxImport 覆盖已有动画后，动画数据从 24fps 变成 30fps（时长 ×0.8），
 * 但资产上按秒存的通知（Notify）和同步标记（Sync Marker）还在旧时间上；写入数据时清空的 UE 内手加曲线也没了。
 * 这个工具对照导入前的备份（Saved/Backups/AnimFbxImport/<时间>/，把它挂成 /WuwaBackup/ 直接读取）：
 * - 通知、同步标记的时间按 新时长/原时长 缩放；
 * - 原资产有、现在没有的浮点曲线按同样比例恢复；
 * 默认只检查并报告，-Apply 才修改并保存。
 *
 * UnrealEditor-Cmd.exe Wuwa.uproject -run=WuwaAnimRetimeFix -Backup=C:/UEProjects/Wuwa/Saved/Backups/AnimFbxImport/<时间> [-Apply]
 */
UCLASS()
class WUWAANIMATIONEDITOR_API UWuwaAnimRetimeFixCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaAnimRetimeFixCommandlet();
	virtual int32 Main(const FString& Params) override;
};
