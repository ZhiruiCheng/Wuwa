#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AkAudioDevice.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_AudioEvent.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_Footstep.h"
#include "Game/Animation/Notifies/WuwaAnimNotifyState_AudioEvent.h"
#include "Game/Audio/WuwaWwiseAudio.h"

// 长离普攻的音频通知已装上，事件资产能加载，并且在有 Wwise 音频设备时能播出来（拿到播放 ID）、能停止。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaWwiseChangliAttackAudioTest,
	"Wuwa.Audio.Wwise.ChangliAttackEventsPlay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaWwiseChangliAttackAudioTest::RunTest(const FString& Parameters)
{
	const UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Characters/Role/changli/AnimMontage/AM_Attack01.AM_Attack01"));
	const UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Characters/Role/changli/NewModel/changli2_0R2T1ChangLiMd10011_LOD0_Skeleton_Attack01.changli2_0R2T1ChangLiMd10011_LOD0_Skeleton_Attack01"));
	if (!TestNotNull(TEXT("AM_Attack01 loads"), Montage) || !TestNotNull(TEXT("Attack01 sequence loads"), Sequence)) return false;

	FSoftObjectPath Voice, Swing;
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		if (const auto* Notify = Cast<UWuwaAnimNotify_AudioEvent>(Event.Notify)) Voice = Notify->AudioEvent;
	}
	for (const FAnimNotifyEvent& Event : Sequence->Notifies)
	{
		if (const auto* State = Cast<UWuwaAnimNotifyState_AudioEvent>(Event.NotifyStateClass)) Swing = State->AudioEvent;
	}
	TestEqual(TEXT("Voice notify event"), Voice.GetAssetName(), FString(TEXT("play_vo_changli_bat_atk01")));
	TestEqual(TEXT("Swing notify event"), Swing.GetAssetName(), FString(TEXT("play_role_changli_bat__am_attack01")));
	TestNotNull(TEXT("Swing event asset loads"), Swing.TryLoad());

	if (!FAkAudioDevice::Get())
	{
		AddInfo(TEXT("No Wwise audio device in this process; playback not checked."));
		return true;
	}
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(true).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("Test world exists"), World)) return false;
	// Wwise 的遮挡检测会按世界查 WorldContext（引擎断言要求存在），测试世界要注册一个
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	USceneComponent* Emitter = NewObject<USceneComponent>(World->GetWorldSettings());
	Emitter->RegisterComponentWithWorld(World);

	const FWuwaWwisePlaying Playing = WuwaWwiseAudio::Post(Swing, Emitter, NAME_None, true, FTransform::Identity);
	TestTrue(TEXT("Swing event posts (playing ID)"), Playing.IsValid());
	TestTrue(TEXT("Swing event is playing"), WuwaWwiseAudio::IsPlaying(Playing));
	WuwaWwiseAudio::Stop(Playing, 0.f, 4);

	// 脚步：长离在用的跑步动画（地面移动混合空间的样本）上有 run 步态的脚步通知，设置开关后脚步事件能播出来
	const UAnimSequence* Run = LoadObject<UAnimSequence>(nullptr,
		TEXT("/Game/Characters/Role/changli/NewModel/changli2_0R2T1ChangLiMd10011_LOD0_Skeleton_Run_F.changli2_0R2T1ChangLiMd10011_LOD0_Skeleton_Run_F"));
	const UWuwaAnimNotify_Footstep* Footstep = nullptr;
	int32 Footsteps = 0;
	for (const FAnimNotifyEvent& Event : Run ? Run->Notifies : TArray<FAnimNotifyEvent>())
	{
		if (const auto* Notify = Cast<UWuwaAnimNotify_Footstep>(Event.Notify)) { Footstep = Notify; ++Footsteps; }
	}
	TestEqual(TEXT("Run_F has two footsteps"), Footsteps, 2);
	if (TestNotNull(TEXT("Footstep notify"), Footstep))
	{
		TestEqual(TEXT("Footstep variant"), Footstep->Variant, FName(TEXT("run")));
		WuwaWwiseAudio::SetSwitch(Emitter, NAME_None, Footstep->SwitchGroup, Footstep->Variant);
		const FWuwaWwisePlaying Step = WuwaWwiseAudio::Post(Footstep->AudioEvent, Emitter, NAME_None, true, FTransform::Identity);
		TestTrue(TEXT("Footstep event posts (playing ID)"), Step.IsValid());
	}

	Emitter->DestroyComponent();
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif
