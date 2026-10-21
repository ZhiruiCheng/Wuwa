#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LTweener.h"
#include "WuwaUiTweenGroup.generated.h"

class UCurveFloat;

/** 补间每帧对目标做的事（原作 LGUIPlayTween 事件调用的函数）。 */
UENUM(BlueprintType)
enum class EWuwaUiTweenAction : uint8
{
	/** UIEffectTextAnimation（按组件名找）的 SetSelectorOffset。 */
	SelectorOffset,
	/** 透明度：可渲染组件 SetAlpha；容器用 UICanvasGroup。 */
	Alpha,
	/** 根节点缩放（界面平面 X 水平、Y 竖直）。 */
	Scale,
	/** 锚点偏移（AnchoredPosition 的 X / Y）。 */
	AnchorOffsetX,
	AnchorOffsetY,
	/** 可渲染组件 SetColor。 */
	Color,
	/** SetIsUIActive（值非 0 为显示）。 */
	Active,
};

UENUM(BlueprintType)
enum class EWuwaUiTweenKind : uint8
{
	Float,
	Vector2,
	Vector3,
	Color,
	LinearColor,
};

/** 一条事件：对一个目标做一件事。用补间当前值，或用固定值（原作 UseNativeParameter / ParamBuffer）。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaUiTweenEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tween")
	TObjectPtr<AActor> Target;

	/** 目标 Actor 上的组件名（SelectorOffset 用，其余动作用根组件）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tween")
	FName Component;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tween")
	EWuwaUiTweenAction Action = EWuwaUiTweenAction::Alpha;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tween")
	bool bUseTweenValue = true;

	/** 不用补间值时的值（浮点用 X；颜色是 RGBA 0~1）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tween")
	FVector4 FixedValue = FVector4(0, 0, 0, 0);

	/** 补间值乘的系数（锚点偏移：原作设计分辨率 → 项目画布）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tween")
	float ValueScale = 1.f;
};

/** 一个补间（≈ 原作 LGUIPlayTween_Float / Vector3 / Color）：等 StartDelay 后从 From 到 To 播 Duration 秒，按缓动和循环方式。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaUiTween
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tween")
	EWuwaUiTweenKind Kind = EWuwaUiTweenKind::Float;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tween")
	FVector4 From = FVector4(0, 0, 0, 0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tween")
	FVector4 To = FVector4(1, 1, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tween")
	ELTweenEase Ease = ELTweenEase::Linear;

	/** Ease = CurveFloat 时用（输入 0~1 的进度）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tween")
	TObjectPtr<UCurveFloat> Curve;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tween")
	float Duration = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tween")
	float StartDelay = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tween")
	ELTweenLoop Loop = ELTweenLoop::Once;

	/** 循环次数（Once 时忽略；-1 无限）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tween")
	int32 LoopCount = -1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tween")
	TArray<FWuwaUiTweenEvent> OnUpdate;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tween")
	TArray<FWuwaUiTweenEvent> OnStart;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tween")
	TArray<FWuwaUiTweenEvent> OnComplete;
};

/**
 * 一个节点上的一组补间（≈ 原作一个动画节点上的全部 LGUIPlayTweenComponent，界面代码 PlayTweenAnim 时一起 Play）。
 * 数据由 WuwaKuroHudBuild 从原作预制体导出；不受游戏暂停和时间膨胀影响（原作默认 affectByGamePause / affectByTimeDilation = false）。
 */
UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaUiTweenGroup : public UActorComponent
{
	GENERATED_BODY()

public:
	UWuwaUiTweenGroup();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tween")
	TArray<FWuwaUiTween> Tweens;

	/** 从头播放全部补间（正在播的重新开始）。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|UI")
	void Play();

	/** 停止，不触发完成事件，值停在当前。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|UI")
	void Stop();

	UFUNCTION(BlueprintPure, Category = "Wuwa|UI")
	bool IsPlaying() const { return bPlaying; }

	/** 播放速度倍率（原作伤害数字跟着角色时间膨胀用）。 */
	void SetSpeed(float InSpeed) { Speed = FMath::Max(0.f, InSpeed); }

	/** 推进 DeltaSeconds（测试 / 不走组件 Tick 时手动推进）。 */
	void Advance(float DeltaSeconds);

	/** 把一个补间在播放时间 Time（从 Play 起算，含延迟）时的值算出来。返回 false 表示还在延迟里。 */
	static bool Evaluate(const FWuwaUiTween& Tween, float Time, FVector4& OutValue, bool& bOutFinished);

	/** 执行一条事件。 */
	static void Apply(const FWuwaUiTweenEvent& Event, const FVector4& TweenValue);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	struct FState
	{
		bool bStarted = false;
		bool bFinished = false;
	};
	TArray<FState> States;
	float Elapsed = 0.f;
	float Speed = 1.f;
	bool bPlaying = false;
};
