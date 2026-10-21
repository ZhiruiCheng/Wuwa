#pragma once

#include "CoreMinimal.h"

class USceneComponent;

/** 一次 Wwise 事件播放：事件短 ID + 播放 ID（0 表示没有播出来）。只在 C++ 里用，不暴露 Wwise 类型给反射。 */
struct FWuwaWwisePlaying
{
	uint32 EventId = 0;
	uint32 PlayingId = 0;

	bool IsValid() const { return PlayingId != 0; }
};

/**
 * 项目对 Wwise 的最小封装，对应原作 AudioSystem.PostEvent / ExecuteAction 的用法。
 * 事件用软路径（/Script/AkAudio.AkAudioEvent），没有音频设备（命令行、NullRHI 测试）时什么也不做。
 */
namespace WuwaWwiseAudio
{
	/**
	 * 播放事件。bFollow：挂在 AttachTo 的插槽上的 AkComponent 上播放，声音跟着角色走（原作 Follow，默认）；
	 * 否则在 WorldTransform 处一次性播放。
	 */
	WUWA_API FWuwaWwisePlaying Post(const FSoftObjectPath& Event, USceneComponent* AttachTo, FName Socket, bool bFollow,
		const FTransform& WorldTransform);

	/** 停止一次播放（原作 ExecuteAction(handle, Stop, TransitionDuration, TransitionFadeCurve)）。Curve 是 AkCurveInterpolation 原值。 */
	WUWA_API void Stop(const FWuwaWwisePlaying& Playing, float FadeSeconds, int32 Curve);

	/**
	 * 在挂点插槽的 AkComponent（与 Post 用的是同一个游戏对象）上设置开关，例如脚步的 footstep_variant = run。
	 * 原作 AudioSystem.SetSwitch(组, 值, Owner)；开关按名字的 Wwise 短 ID 设置。
	 */
	WUWA_API void SetSwitch(USceneComponent* AttachTo, FName Socket, FName Group, FName Value);

	/** 事件是否还在播放（Wwise 的结束回调由集成层在游戏线程更新这张表）。 */
	WUWA_API bool IsPlaying(const FWuwaWwisePlaying& Playing);
}
