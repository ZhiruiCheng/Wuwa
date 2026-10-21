#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaAssetDumpCommandlet.generated.h"

/**
 * 只读：把一个资产（蓝图时取生成类的默认对象）的属性按 UE 文本格式打印到日志，排查配置用。
 *   -run=WuwaAssetDump -Asset=/Game/Characters/Role/changli/Input/DA_Changli_InputCommands [-Property=Rules]
 */
UCLASS()
class WUWA_API UWuwaAssetDumpCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaAssetDumpCommandlet();
	virtual int32 Main(const FString& Params) override;
};
