#include "Game/Audio/WuwaWwiseAudio.h"

#include "AkAudioDevice.h"
#include "AkAudioEvent.h"
#include "AkComponent.h"
#include "AkGameplayStatics.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Wwise/API/WwiseSoundEngineAPI.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaWwise, Log, All);

namespace WuwaWwiseAudio
{
	namespace
	{
		/** Wwise 的遮挡计算会按 World 查 WorldContext（查不到直接断言）；没有 WorldContext 的临时 World（如自动化测试）里不播。 */
		bool CanPostIn(const USceneComponent* AttachTo)
		{
			const UWorld* World = AttachTo ? AttachTo->GetWorld() : nullptr;
			return !World || (GEngine && GEngine->GetWorldContextFromWorld(World));
		}

		/**
		 * 挂在角色网格上的发声组件。关掉 Wwise 的射线遮挡（默认每 0.2s 从发声点向监听者打 Visibility 射线）：
		 * 发声点在网格根部 = 脚底，常常比地面低零点几厘米，射线一出发就打到双面碰撞的地面，算成完全遮挡，声音被压没。
		 */
		UAkComponent* GetEmitter(USceneComponent* AttachTo, FName Socket)
		{
			bool bCreated = false;
			UAkComponent* Ak = UAkGameplayStatics::GetOrCreateAkComponent(AttachTo, bCreated, Socket);
			if (Ak && bCreated)
			{
				Ak->OcclusionRefreshInterval = 0.f;
			}
			return Ak;
		}
	}

	FWuwaWwisePlaying Post(const FSoftObjectPath& EventPath, USceneComponent* AttachTo, FName Socket, bool bFollow,
		const FTransform& WorldTransform)
	{
		FWuwaWwisePlaying Playing;
		if (EventPath.IsNull() || !FAkAudioDevice::Get() || !CanPostIn(AttachTo)) return Playing;
		// 事件资产只有几百字节（媒体在 SoundBank 里），同步加载
		UAkAudioEvent* Event = Cast<UAkAudioEvent>(EventPath.TryLoad());
		if (!Event)
		{
			UE_LOG(LogWuwaWwise, Warning, TEXT("Wwise event %s is missing; reconcile the Wwise assets."), *EventPath.ToString());
			return Playing;
		}
		Playing.EventId = Event->GetShortID();
		if (bFollow && AttachTo)
		{
			if (UAkComponent* Ak = GetEmitter(AttachTo, Socket))
			{
				Playing.PlayingId = Ak->PostAkEvent(Event, 0, nullptr, nullptr);
			}
		}
		else
		{
			const UObject* Context = AttachTo ? static_cast<const UObject*>(AttachTo) : nullptr;
			const FTransform At = AttachTo ? AttachTo->GetSocketTransform(Socket) : WorldTransform;
			Playing.PlayingId = static_cast<uint32>(UAkGameplayStatics::PostEventAtLocation(Event, At.GetLocation(), At.Rotator(),
				const_cast<UObject*>(Context)));
		}
		return Playing;
	}

	void Stop(const FWuwaWwisePlaying& Playing, float FadeSeconds, int32 Curve)
	{
		FAkAudioDevice* Device = FAkAudioDevice::Get();
		if (!Device || !Playing.IsValid()) return;
		const AkTimeMs FadeMs = static_cast<AkTimeMs>(FMath::Max(0.f, FadeSeconds) * 1000.f);
		const AkCurveInterpolation Interpolation = static_cast<AkCurveInterpolation>(FMath::Clamp(Curve, 0, static_cast<int32>(AkCurveInterpolation_LastFadeCurve)));
		Device->ExecuteActionOnPlayingID(AkActionOnEventType_Stop, Playing.PlayingId, FadeMs, Interpolation);
	}

	void SetSwitch(USceneComponent* AttachTo, FName Socket, FName Group, FName Value)
	{
		IWwiseSoundEngineAPI* SoundEngine = FAkAudioDevice::Get() ? IWwiseSoundEngineAPI::Get() : nullptr;
		if (!SoundEngine || !AttachTo || Group.IsNone() || Value.IsNone() || !CanPostIn(AttachTo)) return;
		if (UAkComponent* Ak = GetEmitter(AttachTo, Socket))
		{
			SoundEngine->SetSwitch(SoundEngine->GetIDFromString(TCHAR_TO_ANSI(*Group.ToString())),
				SoundEngine->GetIDFromString(TCHAR_TO_ANSI(*Value.ToString())), Ak->GetAkGameObjectID());
		}
	}

	bool IsPlaying(const FWuwaWwisePlaying& Playing)
	{
		FAkAudioDevice* Device = FAkAudioDevice::Get();
		return Device && Playing.IsValid() && Device->IsPlayingIDActive(Playing.EventId, Playing.PlayingId);
	}
}
