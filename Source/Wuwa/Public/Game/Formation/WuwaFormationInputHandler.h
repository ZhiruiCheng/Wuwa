#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Game/Input/IWuwaInputRouteHandler.h"
#include "WuwaFormationInputHandler.generated.h"

class UWuwaFormationComponent;

/**
 * 切人的输入处理器：挂在 PlayerController 上，由 InputRouter 按 Input.Route.Formation 交过来。
 * 只把"按下 1/2/3"翻译成队员序号交给 UWuwaFormationComponent，能不能切、怎么切都由队伍组件决定。
 */
UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaFormationInputHandler : public UActorComponent, public IIWuwaInputRouteHandler
{
	GENERATED_BODY()

public:
	UWuwaFormationInputHandler();

	/** 只响应 Pressed；按住时的 Triggered/Held 和松开都忽略。返回 true 表示是切人输入。 */
	virtual bool HandleWuwaInput_Implementation(const FWuwaInputEvent& InputEvent) override;

	/** 切人输入 Tag → 队员序号（0 起）；不是切人输入返回 INDEX_NONE。 */
	static int32 GetMemberIndexForInput(const FGameplayTag& InputTag);

private:
	UWuwaFormationComponent* GetFormation() const;
};
