#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Game/Audio/WuwaWwiseAudio.h"
#include "HAL/IConsoleManager.h"

/**
 * 背景音乐（临时做法）：wuwa.BGM.Toggle 在黎那汐塔和战斗两首循环曲之间切换，旧曲淡出、新曲从头播。
 * DefaultInput.ini 把 O 键绑到这条命令（DebugExecBindings，只在非 Shipping 版本生效）。
 * 曲目和事件由 ArtSource/Audio/build_bgm.py 建到 Wwise（Wuwa_BGM）；声音挂在相机管理器上，切换角色不会断。
 */
namespace WuwaBgm
{
	const TCHAR* const Events[] = {
		TEXT("/Game/WwiseAudio/Events/Default_Work_Unit/Wuwa_BGM/Play_BGM_Rinascita.Play_BGM_Rinascita"),
		TEXT("/Game/WwiseAudio/Events/Default_Work_Unit/Wuwa_BGM/Play_BGM_Battle.Play_BGM_Battle"),
	};
	constexpr float FadeSeconds = 1.5f;

	TWeakObjectPtr<UWorld> PlayingWorld;
	FWuwaWwisePlaying Playing;
	int32 Current = INDEX_NONE;

	APlayerController* FindPlayer()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if ((Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game) && Context.World())
			{
				if (APlayerController* PC = Context.World()->GetFirstPlayerController()) return PC;
			}
		}
		return nullptr;
	}

	static FAutoConsoleCommandWithOutputDevice CmdToggle(TEXT("wuwa.BGM.Toggle"),
		TEXT("背景音乐在黎那汐塔 / 战斗之间切换（没在播时先播黎那汐塔）。O 键绑定了这条命令。"),
		FConsoleCommandWithOutputDeviceDelegate::CreateLambda([](FOutputDevice& Out)
		{
			APlayerController* PC = FindPlayer();
			USceneComponent* Emitter = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetRootComponent() : nullptr;
			if (!Emitter) { Out.Log(TEXT("wuwa.BGM.Toggle: no play world")); return; }
			// 新开一次游戏时上一局的播放记录作废
			if (PlayingWorld.Get() != PC->GetWorld())
			{
				Playing = FWuwaWwisePlaying();
				Current = INDEX_NONE;
			}
			if (Playing.IsValid()) WuwaWwiseAudio::Stop(Playing, FadeSeconds, 4);
			Current = Current == 0 ? 1 : 0;
			Playing = WuwaWwiseAudio::Post(FSoftObjectPath(Events[Current]), Emitter, NAME_None, true, FTransform::Identity);
			PlayingWorld = PC->GetWorld();
			Out.Logf(TEXT("wuwa.BGM.Toggle -> %s (%s)"), Current == 0 ? TEXT("Rinascita") : TEXT("Battle"), Playing.IsValid() ? TEXT("playing") : TEXT("not played"));
		}));
}
