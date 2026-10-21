#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "AkAudioDevice.h"
#include "AkComponent.h"
#include "AkGameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Game/Audio/WuwaWwiseAudio.h"
#include "Wwise/API/WwiseSoundEngineAPI.h"

// 排查用：PIE 里 Wwise 的监听者（耳朵）在哪、在角色身上发一个音效是否在播。名字不含 "Wuwa"，不随常规测试运行。
//   -ExecCmds="Automation RunTests Diag.Audio.Listener; Quit"（大卡场地），Diag.Audio.ListenerJiaoTing（教廷，对照）
namespace AudioListenerDiag
{
	UWorld* FindPlayWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) return Context.World();
		}
		return nullptr;
	}

	FString WwisePosition(AkGameObjectID Id)
	{
		IWwiseSoundEngineAPI* Engine = IWwiseSoundEngineAPI::Get();
		AkSoundPosition Position;
		if (!Engine || Engine->Query->GetPosition(Id, Position) != AK_Success) return TEXT("(not registered)");
		const AkVector64 P = Position.Position();
		return FString::Printf(TEXT("(%.0f, %.0f, %.0f)"), P.X, P.Y, P.Z);
	}

	class FProbe final : public IAutomationLatentCommand
	{
	public:
		explicit FProbe(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			if (Now - StartedAt > 300.0) { Test->AddError(TEXT("Timed out waiting for the pawn.")); return true; }
			UWorld* World = FindPlayWorld();
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			ACharacter* Pawn = Player ? Cast<ACharacter>(Player->GetPawn()) : nullptr;
			if (!Pawn) return false;
			if (PawnAt < 0.0) PawnAt = Now;
			if (Now - PawnAt < 2.0) return false;
			FAkAudioDevice* Device = FAkAudioDevice::Get();
			if (!Test->TestNotNull(TEXT("Wwise audio device"), Device)) return true;

			if (!bPosted)
			{
				Test->AddInfo(FString::Printf(TEXT("World %s, AllowAudioPlayback %d"), *World->GetName(), World->AllowAudioPlayback()));
				Test->AddInfo(FString::Printf(TEXT("Pawn %s at %s"), *Pawn->GetName(), *Pawn->GetActorLocation().ToString()));
				if (APlayerCameraManager* Camera = Player->PlayerCameraManager)
				{
					Test->AddInfo(FString::Printf(TEXT("Camera manager %s actor at %s, POV at %s"), *Camera->GetName(),
						*Camera->GetRootComponent()->GetComponentLocation().ToString(), *Camera->GetCameraLocation().ToString()));
				}
				FVector EarLocation, EarFront, EarRight;
				Player->GetAudioListenerPosition(EarLocation, EarFront, EarRight);
				Test->AddInfo(FString::Printf(TEXT("PlayerController audio listener position %s"), *EarLocation.ToString()));
				int32 Index = 0;
				for (const TWeakObjectPtr<UAkComponent>& Listener : Device->GetDefaultListeners())
				{
					const UAkComponent* Ak = Listener.Get();
					Test->AddInfo(FString::Printf(TEXT("Default listener %d: %s on %s in world %s, component at %s, Wwise position %s"), Index++,
						Ak ? *Ak->GetName() : TEXT("(stale)"), Ak ? *GetNameSafe(Ak->GetOwner()) : TEXT("-"),
						Ak ? *GetNameSafe(Ak->GetWorld()) : TEXT("-"), Ak ? *Ak->GetComponentLocation().ToString() : TEXT("-"),
						Ak ? *WwisePosition(Ak->GetAkGameObjectID()) : TEXT("-")));
				}
				const UAkComponent* Spatial = Device->GetSpatialAudioListener();
				Test->AddInfo(FString::Printf(TEXT("Spatial audio listener: %s on %s"), *GetNameSafe(Spatial), Spatial ? *GetNameSafe(Spatial->GetOwner()) : TEXT("-")));

				// 路径相对 Wwise 的写出目录 Saved/Wwise（FWwiseIOHookImpl::OpenFileForWrite）
				CapturePath = TEXT("AudioDiagCapture.wav");
				Device->StartOutputCapture(CapturePath);
				Test->AddInfo(FString::Printf(TEXT("Output capture -> %s"), *CapturePath));
				Playing = WuwaWwiseAudio::Post(FSoftObjectPath(TEXT("/Game/WwiseAudio/Events/Default_Work_Unit/Wuwa_Changli/play_role_changli_bat__am_attack01.play_role_changli_bat__am_attack01")),
					Pawn->GetMesh(), NAME_None, true, FTransform::Identity);
				Test->AddInfo(FString::Printf(TEXT("Posted attack01 audio: event %u playing id %u"), Playing.EventId, Playing.PlayingId));
				bPosted = true;
				PostedAt = Now;
				return false;
			}
			if (Now - PostedAt < 0.2) return false;
			if (!bCheckedPlaying)
			{
				Test->AddInfo(FString::Printf(TEXT("After 0.2s: playing %d"), WuwaWwiseAudio::IsPlaying(Playing)));
				bCheckedPlaying = true;
			}
			// 录 2 秒主混音，看 Wwise 实际有没有渲染出声音
			if (Now - PostedAt < 2.0) return false;
			Device->StopOutputCapture();
			bool bCreated = false;
			if (UAkComponent* Emitter = UAkGameplayStatics::GetOrCreateAkComponent(Pawn->GetMesh(), bCreated, NAME_None))
			{
				IWwiseSoundEngineAPI* Engine = IWwiseSoundEngineAPI::Get();
				AkUInt32 Count = 8;
				AkGameObjectID Ids[8];
				const AKRESULT Result = Engine ? Engine->Query->GetListeners(Emitter->GetAkGameObjectID(), Ids, Count) : AK_Fail;
				Test->AddInfo(FString::Printf(TEXT("Emitter %s Wwise position %s, listeners result %d count %u"), *Emitter->GetName(),
					*WwisePosition(Emitter->GetAkGameObjectID()), int32(Result), Result == AK_Success ? Count : 0));
				for (AkUInt32 i = 0; Result == AK_Success && i < Count; ++i)
				{
					AkReal32 Obstruction = -1.f, Occlusion = -1.f;
					Engine->Query->GetObjectObstructionAndOcclusion(Emitter->GetAkGameObjectID(), Ids[i], Obstruction, Occlusion);
					Test->AddInfo(FString::Printf(TEXT("  listener %llu at %s: obstruction %.2f occlusion %.2f"), uint64(Ids[i]),
						*WwisePosition(Ids[i]), Obstruction, Occlusion));
				}
				// 和 Wwise 遮挡服务一样的射线（AkObstructionAndOcclusionService：Visibility、复杂碰撞、忽略发声者和视角目标）
				FVector EarLocation, EarFront, EarRight;
				Player->GetAudioListenerPosition(EarLocation, EarFront, EarRight);
				FCollisionQueryParams Params(TEXT("SoundOcclusion"), true, Pawn);
				Params.AddIgnoredActor(Player->GetViewTarget());
				for (const float Lift : {0.f, 20.f, 100.f})
				{
					const FVector Source = Emitter->GetComponentLocation() + FVector(0, 0, Lift);
					FHitResult Hit;
					const bool bHit = World->LineTraceSingleByChannel(Hit, Source, EarLocation, ECC_Visibility, Params);
					Test->AddInfo(FString::Printf(TEXT("  trace from emitter +%.0f %s -> ear: %s"), Lift, *Source.ToCompactString(),
						bHit ? *FString::Printf(TEXT("HIT %s / %s at %s (start penetrating %d)"), *GetNameSafe(Hit.GetActor()),
							*GetNameSafe(Hit.GetComponent()), *Hit.ImpactPoint.ToCompactString(), Hit.bStartPenetrating) : TEXT("clear")));
				}
			}
			return true;
		}

	private:
		FAutomationTestBase* Test;
		double StartedAt = -1.0;
		double PawnAt = -1.0;
		double PostedAt = 0.0;
		bool bPosted = false;
		bool bCheckedPlaying = false;
		FString CapturePath;
		FWuwaWwisePlaying Playing;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioListenerDiagTest, "Diag.Audio.Listener",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAudioListenerDiagTest::RunTest(const FString& Parameters)
{
	const FString Map = Parameters.IsEmpty() ? TEXT("/Game/Environment/DaKa/Maps/L_DaKa") : Parameters;
	if (!AutomationOpenMap(Map)) return false;
	ADD_LATENT_AUTOMATION_COMMAND(AudioListenerDiag::FProbe(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAudioListenerDiagJiaoTingTest, "Diag.Audio.ListenerJiaoTing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAudioListenerDiagJiaoTingTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Environment/JiaoTing/Maps/L_JiaoTing"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(AudioListenerDiag::FProbe(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
