// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Input/IWuwaInputRouteHandler.h"
#include "WuwaMoveInputHandler.generated.h"

class UWuwaMoveInputConfig;

/**
 * 处理移动类的一次性指令（比如走跑切换）：按配置表把语义输入映射到指令对象执行，本身不含任何按键分支。
 * 移动轴不经过这里，由角色的输入意图组件记录、CMC 每帧读取。
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class WUWA_API UWuwaMoveInputHandler : public UActorComponent,public IIWuwaInputRouteHandler
{
	GENERATED_BODY()

public:
	UWuwaMoveInputHandler();

	virtual bool HandleWuwaInput_Implementation(const FWuwaInputEvent& InputEvent) override;

	/** 移动类输入 -> 指令的配置表；在 PC 蓝图的 MoveInputHandler 组件中指定。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Input")
	TSoftObjectPtr<UWuwaMoveInputConfig> Config;
	
private:
	// 配置缺失只提示一次，避免连续输入重复刷日志。
	bool bReportedMissingConfig = false;
};
