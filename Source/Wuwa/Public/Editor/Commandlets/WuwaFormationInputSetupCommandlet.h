#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaFormationInputSetupCommandlet.generated.h"

/** 切人输入（按键 1/2/3）的资产配置。默认只读审计；-Apply 才写入 IMC_Character 和 DA_InputActionTagAsset。 */
UCLASS()
class WUWA_API UWuwaFormationInputSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaFormationInputSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
