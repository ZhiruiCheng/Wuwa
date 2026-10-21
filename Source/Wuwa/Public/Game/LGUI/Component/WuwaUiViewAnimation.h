#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LTweener.h"
#include "WuwaUiViewAnimation.generated.h"

class UUICanvasGroup;
class UUIItem;

/** 预设的打开/关闭效果。 */
UENUM()
enum class EWuwaUiAnimEffect : uint8
{
	None,
	Fade,
	Scale,
	FadeScale,
	SlideFromBottom,
	SlideFromRight,
};

/**
 * 界面打开/关闭动画（≈ 鸣潮预制体里的 Start/Close 序列）：挂在界面预制体的根节点上，在编辑器里选效果和时长。
 * 框架在 Show 时播放打开动画、Hide 时播放关闭动画，播完才算 Shown/Hidden。
 * 动画不受时间缩放和游戏暂停影响（界面可能正好把游戏暂停了）。
 * 需要更复杂的动画时，在界面类里重写 PlayShowAnimation / PlayHideAnimation。
 */
UCLASS(ClassGroup = (WuwaUI), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaUiViewAnimation : public UActorComponent
{
	GENERATED_BODY()
public:
	UWuwaUiViewAnimation();

	UPROPERTY(EditAnywhere, Category = "Show") EWuwaUiAnimEffect ShowEffect = EWuwaUiAnimEffect::FadeScale;
	UPROPERTY(EditAnywhere, Category = "Show", meta = (ClampMin = "0.0")) float ShowDuration = 0.2f;
	UPROPERTY(EditAnywhere, Category = "Show") ELTweenEase ShowEase = ELTweenEase::OutCubic;

	UPROPERTY(EditAnywhere, Category = "Hide") EWuwaUiAnimEffect HideEffect = EWuwaUiAnimEffect::Fade;
	UPROPERTY(EditAnywhere, Category = "Hide", meta = (ClampMin = "0.0")) float HideDuration = 0.15f;
	UPROPERTY(EditAnywhere, Category = "Hide") ELTweenEase HideEase = ELTweenEase::InCubic;

	/** Scale 效果从多大开始（打开）或缩到多大（关闭）。 */
	UPROPERTY(EditAnywhere, Category = "Effect", meta = (ClampMin = "0.0")) float ScaleFrom = 0.9f;
	/** Slide 效果的位移距离。 */
	UPROPERTY(EditAnywhere, Category = "Effect") float SlideDistance = 200.f;

	/** 从"看不见"播到"完全显示"。没有效果或时长为 0 时立即完成。 */
	void PlayShow(TFunction<void()> OnDone);
	/** 从"完全显示"播到"看不见"，结束后恢复到完全显示的样子（反正接下来会被设成不显示）。 */
	void PlayHide(TFunction<void()> OnDone);
	/** 立刻停止，不回调（界面被立即销毁时用）。 */
	void Stop();

	bool IsPlaying() const { return Tweener != nullptr; }
	/** 0 = 看不见，1 = 完全显示（测试用）。 */
	float GetProgress() const { return Progress; }

private:
	void Play(EWuwaUiAnimEffect Effect, float Duration, ELTweenEase Ease, bool bShow, TFunction<void()> OnDone);
	void Apply(EWuwaUiAnimEffect Effect, float InProgress);
	void CaptureRestState();
	UUIItem* GetRootItem() const;
	UUICanvasGroup* FindOrAddCanvasGroup();

	UPROPERTY(Transient) TObjectPtr<ULTweener> Tweener;
	UPROPERTY(Transient) TObjectPtr<UUICanvasGroup> CanvasGroup;
	float Progress = 1.f;
	EWuwaUiAnimEffect PlayingEffect = EWuwaUiAnimEffect::None;
	bool bCapturedRestState = false;
	FVector RestScale = FVector::OneVector;
	FVector2D RestPosition = FVector2D::ZeroVector;
};
