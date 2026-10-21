#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Game/NewWorld/Buff/WuwaBuffTypes.h"
#include "WuwaHudBuffItem.generated.h"

class AActor;
class UUIItem;
class UUISprite;
class UUIText;
class UUITexture;
class UWuwaUiTweenGroup;

/**
 * 一个 buff 限时图标（≈ 原作 BuffItem），包着一个 UiItem_BuffItem 预制体实例。
 * 预制体根上的 WuwaUiComponentsRegistry 顺序和原作 BuffItem.ComponentRegisterInfos 一样（0~14）：
 *   0 BuffItemCtrl（闪烁时调它的透明度）、1 图标、2 倒计时填充、3 层数、4 减益角标、5 出现动画、6 关闭动画、7 闪烁动画、
 *   8 增益角标、9 背景、10 大图标、11 圆形背景、12 圆形填充、13 叠层动画、14 彩色边框。
 * 显示规则照搬原作 BuffItem.Activate / Ost（见 FWuwaBuffCueRow::Parameters）：倒计时填充 = 剩余 / 总时长，剩余不到 20% 时闪烁。
 */
UCLASS()
class WUWA_API UWuwaHudBuffItem : public UObject
{
	GENERATED_BODY()

public:
	/** 绑定预制体实例；注册表不对时返回 false。 */
	bool Init(AActor* InActor);
	AActor* GetActor() const { return Actor; }
	UUIItem* GetRoot() const { return Root; }

	/** 显示一个 buff；bPlayStart：播出现动画（新加的 buff）。 */
	void Activate(const FWuwaBuffUiData& Data, bool bPlayStart);
	/** 同一个图标的数据更新（层数、剩余时间）；层数变多时播出现动画（原作 SetNum）。 */
	void UpdateData(const FWuwaBuffUiData& Data);
	/** 每帧：倒计时填充和闪烁。 */
	void Tick(double WorldTime);
	/** 收起：bWithCloseAnim 时播关闭动画，GetHideRemaining() 秒后再隐藏（原作 CLOSE_ANIM_TIME 200 毫秒）。 */
	void Deactivate(bool bWithCloseAnim);
	/** 关闭动画还剩多久；返回 false 表示已经隐藏，可以回收。 */
	bool TickHiding(float DeltaSeconds);

	const FWuwaBuffUiData& GetData() const { return Data; }
	float GetDisplayedFill() const { return DisplayedFill; }
	bool IsFading() const { return bFading; }
	bool IsActive() const { return bActive; }

private:
	void SetNum(int32 Num);
	void SetFill(float Fill);
	void SetFading(bool bFade);
	void Play(int32 Index);
	void Stop(int32 Index);

	UPROPERTY() TObjectPtr<AActor> Actor;
	UPROPERTY() TObjectPtr<UUIItem> Root;
	UPROPERTY() TObjectPtr<UUIItem> Ctrl;
	UPROPERTY() TObjectPtr<UUITexture> Icon;
	UPROPERTY() TObjectPtr<UUITexture> BigIcon;
	UPROPERTY() TObjectPtr<UUIText> NumText;
	/** 这次显示用的倒计时填充（方形或圆形）。 */
	UPROPERTY() TObjectPtr<UUISprite> FillTarget;
	UPROPERTY() TMap<int32, TObjectPtr<UUIItem>> Items;
	UPROPERTY() TMap<int32, TObjectPtr<UWuwaUiTweenGroup>> Animations;

	FWuwaBuffUiData Data;
	bool bNoCountdown = false;
	bool bActive = false;
	bool bFading = false;
	int32 ShownNum = 0;
	float DisplayedFill = -1.f;
	float HideRemaining = 0.f;
};
