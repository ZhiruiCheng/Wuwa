#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaNiagaraCompileSetupCommandlet.generated.h"

/**
 * 把 Niagara 系统的编译结果存进资产：资产里存的编译结果过期时（例如构建工具生成后没编译就存了），
 * 编辑器每次打开后第一次激活这个系统都要现编（PIE 里第一次放技能卡顿，日志 "LogNiagara: Compiling System ... took"）。
 * 这里加载每个系统、请求编译（只有过期的才会真正编译）、等编译完；-Apply 时把编译过的系统存回去（先备份到 Saved/Backups/NiagaraCompileSetup/<时间>/）。
 *   -run=WuwaNiagaraCompileSetup [-Paths=/Game/Effects,/Game/LGUI] [-Apply]
 */
UCLASS()
class WUWA_API UWuwaNiagaraCompileSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaNiagaraCompileSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
