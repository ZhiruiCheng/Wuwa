#pragma once

#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputIntentComponent.h"
#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaMoveInputConfig.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaMoveInputHandler.h"
#include "Game/NewWorld/Character/Common/Component/Input/MoveActions/WuwaMoveInputAction_ToggleWalkPreference.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Core/Tag/WuwaGameTags.h"

namespace WuwaTestInput
{
	/** 模拟输入来源写入移动轴，再像本帧 CMC 物理更新前那样让 RoleGait 读取一次；测试不跑世界 Tick。 */
	inline void SetMoveAxis(AWuwaCharacter* Character, const FVector2D& Axis)
	{
		Character->InputIntent->SetMoveAxis(Axis);
		if (IsValid(Character->RoleGaitComponent))
		{
			Character->RoleGaitComponent->RefreshPolicy();
		}
	}

	/** 与项目资产 DA_WuwaMoveInputConfig 相同的一行（走跑键按下 -> 切换走跑偏好）；测试不依赖内容资产。 */
	inline UWuwaMoveInputConfig* MakeMoveInputConfig(UObject* Outer)
	{
		UWuwaMoveInputConfig* Config = NewObject<UWuwaMoveInputConfig>(Outer);
		FWuwaMoveInputBinding& Binding = Config->Bindings.AddDefaulted_GetRef();
		Binding.InputTag = FWuwaGameTags::Get().Player_Common_Movement_WalkRun;
		Binding.Phase = EWuwaInputPhase::Pressed;
		Binding.Action = NewObject<UWuwaMoveInputAction_ToggleWalkPreference>(Config);
		return Config;
	}

	/** 让控制器的移动输入处理器使用测试配置表。 */
	inline void UseTestMoveInputConfig(AWuwaPlayerController* Controller)
	{
		Controller->MoveInputHandler->Config = MakeMoveInputConfig(Controller);
	}

	/** 直接对某个角色执行一次指令，不经过路由。 */
	inline bool ExecuteMoveAction(const UWuwaMoveInputAction& Action, AWuwaCharacter* Character)
	{
		FWuwaMoveInputContext Context;
		Context.Character = Character;
		return Action.Execute(Context);
	}
}
