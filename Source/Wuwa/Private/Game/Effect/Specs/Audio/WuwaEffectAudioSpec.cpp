#include "Game/Effect/Specs/Audio/WuwaEffectAudioSpec.h"

#include "Engine/World.h"
#include "Game/Effect/Types/WuwaEffectSpawnRequest.h"
#include "Game/EffectModel/Models/WuwaEffectModelAudio.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaEffectAudio, Log, All);

bool UWuwaEffectAudioSpec::Play_Implementation(const FWuwaEffectSpawnRequest& Request)
{
	UWuwaEffectModelAudio* Model = Cast<UWuwaEffectModelAudio>(Request.Model);
	UWorld* World = GetWorld();
	if (!Model || !World)
	{
		return false;
	}
	if (Model->AudioBackend == EWuwaEffectAudioBackend::WwiseEvent)
	{
		return PlayWwise(*Model, Request);
	}
	if (!Model->Sound)
	{
		UE_LOG(LogWuwaEffectAudio, Warning, TEXT("%s: no Sound is configured."), *Model->GetName());
		return false;
	}

	// 第一阶段只实现主声音。原作 Wwise 多位置、遮挡、保留主音及拖尾后续单独适配。
	// 每个 Model 只提示一次，避免脚步通知反复刷同一条日志。
	if (!Model->LocationOffsets.IsEmpty() || Model->KeepAlive || Model->TrailingSound || Model->EnableOcclusion)
	{
		static TSet<TWeakObjectPtr<UWuwaEffectModelAudio>> WarnedModels;
		if (!WarnedModels.Contains(Model))
		{
			WarnedModels.Add(Model);
			UE_LOG(LogWuwaEffectAudio, Warning, TEXT("%s: this stage plays the main Sound only; LocationOffsets, KeepAlive, TrailingSound and EnableOcclusion are not applied."), *Model->GetName());
		}
	}

	FadeOutSeconds = FMath::Max(0.f, Model->FadeOutTime);
	FadeOutCurve = Model->UnrealFadeOutCurve;

	if (!IsValid(AudioComponent))
	{
		AudioComponent = NewObject<UAudioComponent>(this);
		AudioComponent->bAutoActivate = false;
		AudioComponent->bAutoDestroy = false;
		AudioComponent->RegisterComponentWithWorld(World);
	}
	UAudioComponent* Component = AudioComponent;
	Component->SetSound(Model->Sound);
	Component->OnAudioPlayStateChangedNative.AddUObject(this, &ThisClass::HandlePlayStateChanged);

	if (USceneComponent* AttachTo = Request.AttachTo.Get())
	{
		Component->AttachToComponent(AttachTo, FAttachmentTransformRules::KeepRelativeTransform, Request.SocketName);
		Component->SetRelativeTransform(Request.Transform);
	}
	else
	{
		Component->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		Component->SetWorldTransform(Request.Transform);
	}

	// 先绑定再播放：Editor 中音频完成/失败可能同步回调。
	Component->Play();
	if (IsFinished())
	{
		return false;
	}
	if (!Component->IsPlaying())
	{
		// 没有音频设备等情况可能直接返回，不会发送完成事件。
		Stop_Implementation(true);
		return false;
	}
	return true;
}

bool UWuwaEffectAudioSpec::PlayWwise(const UWuwaEffectModelAudio& Model, const FWuwaEffectSpawnRequest& Request)
{
	if (Model.AudioEvent.IsNull())
	{
		UE_LOG(LogWuwaEffectAudio, Warning, TEXT("%s: no Wwise AudioEvent is configured."), *Model.GetName());
		return false;
	}
	// 原作 EffectModelAudio 在特效挂点的 AkComponent 上播放；没有挂点时在请求位置一次性播放。
	WwiseAttachTo = Request.AttachTo.Get();
	WwiseSocket = Request.SocketName;
	WwiseTransform = Request.Transform;
	WwisePlaying = WuwaWwiseAudio::Post(Model.AudioEvent, WwiseAttachTo.Get(), WwiseSocket, WwiseAttachTo.IsValid(), WwiseTransform);
	if (!WwisePlaying.IsValid())
	{
		return false;
	}
	FadeOutSeconds = FMath::Max(0.f, Model.FadeOutTime);
	WwiseFadeCurve = Model.FadeOutCurve;
	bWwiseKeepAlive = Model.KeepAlive;
	WwiseTrailingEvent = Model.TrailingAudioEvent;
	return true;
}

void UWuwaEffectAudioSpec::StopWwise(bool bImmediately)
{
	// KeepAlive：特效停止后主音频继续播完（不再跟踪，Spec 立即结束）；否则淡出停止。之后播放拖尾事件。
	if (!bWwiseKeepAlive || bImmediately)
	{
		WuwaWwiseAudio::Stop(WwisePlaying, bImmediately ? 0.f : FadeOutSeconds, WwiseFadeCurve);
	}
	if (!bImmediately && !WwiseTrailingEvent.IsNull())
	{
		WuwaWwiseAudio::Post(WwiseTrailingEvent, WwiseAttachTo.Get(), WwiseSocket, WwiseAttachTo.IsValid(), WwiseTransform);
	}
	WwisePlaying = FWuwaWwisePlaying();
	Finish();
}

void UWuwaEffectAudioSpec::Tick(float DeltaTime)
{
	// 事件自然播完：Wwise 集成层在结束回调里移除播放 ID
	if (WwisePlaying.IsValid() && !WuwaWwiseAudio::IsPlaying(WwisePlaying))
	{
		WwisePlaying = FWuwaWwisePlaying();
		Finish();
	}
}

bool UWuwaEffectAudioSpec::IsTickable() const
{
	return WwisePlaying.IsValid() && !IsFinished();
}

ETickableTickType UWuwaEffectAudioSpec::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

TStatId UWuwaEffectAudioSpec::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWuwaEffectAudioSpec, STATGROUP_Tickables);
}

void UWuwaEffectAudioSpec::Stop_Implementation(bool bImmediately)
{
	if (IsFinished()) return;
	if (WwisePlaying.IsValid())
	{
		StopWwise(bImmediately);
		return;
	}
	UAudioComponent* Component = AudioComponent;
	if (!IsValid(Component))
	{
		AudioComponent = nullptr;
		Finish();
		return;
	}

	if (!bImmediately && FadeOutSeconds > 0.f && Component->IsPlaying())
	{
		if (!bStopping)
		{
			bStopping = true;
			Component->FadeOut(FadeOutSeconds, 0.f, FadeOutCurve);
		}
		return;
	}

	// Stop 会同步广播 Stopped；先解绑，Finish 之后由池负责清理。
	Component->OnAudioPlayStateChangedNative.RemoveAll(this);
	Component->Stop();
	Finish();
}

void UWuwaEffectAudioSpec::ResetForPool()
{
	if (IsValid(AudioComponent))
	{
		AudioComponent->OnAudioPlayStateChangedNative.RemoveAll(this);
		AudioComponent->Stop();
		AudioComponent->SetSound(nullptr);
		AudioComponent->ResetParameters();
		AudioComponent->SetPaused(false);
		AudioComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		AudioComponent->SetWorldTransform(FTransform::Identity);
	}
	FadeOutSeconds = 0.f;
	FadeOutCurve = EAudioFaderCurve::Linear;
	bStopping = false;
	WwisePlaying = FWuwaWwisePlaying();
	WwiseFadeCurve = 4;
	bWwiseKeepAlive = false;
	WwiseTrailingEvent.Reset();
	WwiseAttachTo.Reset();
	WwiseSocket = NAME_None;
	WwiseTransform = FTransform::Identity;
	// 不写引擎的 ActiveCount。Stop 的旧音频线程回执尚未到达也能重播：
	// UE 每次 Play 递增计数，PlaybackCompleted 只有在计数归零时才广播 Stopped。
}

void UWuwaEffectAudioSpec::ReleaseAudioComponent()
{
	if (IsValid(AudioComponent))
	{
		AudioComponent->OnAudioPlayStateChangedNative.RemoveAll(this);
		AudioComponent->Stop();
		AudioComponent->DestroyComponent();
	}
	AudioComponent = nullptr;
}

void UWuwaEffectAudioSpec::HandlePlayStateChanged(const UAudioComponent* Component, EAudioComponentPlayState PlayState)
{
	if (PlayState != EAudioComponentPlayState::Stopped || Component != AudioComponent)
	{
		return;
	}

	// OnAudioFinished 不包含启动失败；Stopped 同时覆盖自然结束、外部停止及启动失败。
	AudioComponent->OnAudioPlayStateChangedNative.RemoveAll(this);
	Finish();
}
