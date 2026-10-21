#include "Game/Module/Hud/Buff/WuwaHudBuffItem.h"

#include "Core/ActorComponent/UIItem.h"
#include "Core/ActorComponent/UISprite.h"
#include "Core/ActorComponent/UIText.h"
#include "Core/ActorComponent/UITexture.h"
#include "Engine/Texture2D.h"
#include "Game/LGUI/Component/WuwaUiComponentsRegistry.h"
#include "Game/LGUI/Component/WuwaUiTweenGroup.h"
#include "Core/ActorComponent/UICanvasGroup.h"

namespace WuwaHudBuffItem
{
	// 注册表下标（原作 BuffItem.ComponentRegisterInfos）
	constexpr int32 Ctrl = 0;
	constexpr int32 Icon = 1;
	constexpr int32 Fill = 2;
	constexpr int32 Num = 3;
	constexpr int32 DebuffMark = 4;
	constexpr int32 AnimStart = 5;
	constexpr int32 AnimClose = 6;
	constexpr int32 AnimFade = 7;
	constexpr int32 BuffMark = 8;
	constexpr int32 Background = 9;
	constexpr int32 BigIcon = 10;
	constexpr int32 RoundBackground = 11;
	constexpr int32 RoundFill = 12;
	constexpr int32 AnimCombine = 13;
	constexpr int32 FrameColour = 14;

	/** 原作 CLOSE_ANIM_TIME（毫秒）。 */
	constexpr float CloseAnimSeconds = 0.2f;
	/** 原作 FADE_ANIM_PERCENT：剩余不到 20% 时闪烁。 */
	constexpr float FadePercent = 0.2f;

	/** 容器的透明度在 CanvasGroup 上（补间也是改它）。 */
	void SetAlpha(UUIItem* Item, float Alpha)
	{
		if (UUICanvasGroup* Group = Item ? Item->GetOwner()->FindComponentByClass<UUICanvasGroup>() : nullptr) Group->SetAlpha(Alpha);
	}

	FLinearColor Hex(const FString& Text, const TCHAR* Default)
	{
		return FLinearColor(FColor::FromHex(Text.IsEmpty() ? FString(Default) : Text));
	}
}

bool UWuwaHudBuffItem::Init(AActor* InActor)
{
	using namespace WuwaHudBuffItem;
	Actor = InActor;
	Root = InActor ? Cast<UUIItem>(InActor->GetRootComponent()) : nullptr;
	const UWuwaUiComponentsRegistry* Registry = InActor ? InActor->FindComponentByClass<UWuwaUiComponentsRegistry>() : nullptr;
	if (!Root || !Registry) return false;
	for (int32 Index = 0; Index <= FrameColour; ++Index)
	{
		const AActor* Entry = Registry->GetActorAt(Index);
		if (UUIItem* Item = Entry ? Cast<UUIItem>(Entry->GetRootComponent()) : nullptr) Items.Add(Index, Item);
		if (UWuwaUiTweenGroup* Group = Entry ? Entry->FindComponentByClass<UWuwaUiTweenGroup>() : nullptr) Animations.Add(Index, Group);
	}
	Ctrl = Items.FindRef(WuwaHudBuffItem::Ctrl);
	Icon = Cast<UUITexture>(Items.FindRef(WuwaHudBuffItem::Icon));
	BigIcon = Cast<UUITexture>(Items.FindRef(WuwaHudBuffItem::BigIcon));
	NumText = Cast<UUIText>(Items.FindRef(WuwaHudBuffItem::Num));
	Root->SetIsUIActive(false);
	return Ctrl && Icon && NumText && Cast<UUISprite>(Items.FindRef(WuwaHudBuffItem::Fill));
}

void UWuwaHudBuffItem::Activate(const FWuwaBuffUiData& InData, bool bPlayStart)
{
	using namespace WuwaHudBuffItem;
	Data = InData;
	const TArray<FString>& Params = Data.Parameters;
	auto Param = [&Params](int32 Index) { return Params.IsValidIndex(Index) ? Params[Index] : FString(); };
	auto SetActive = [this](int32 Index, bool bOn) { if (UUIItem* Item = Items.FindRef(Index)) Item->SetIsUIActive(bOn); };

	// 原作 Ost：[3] 图标样式 —— 1 大图标（不要背景和倒计时）；2 / 4 不显示倒计时；3 / 4 彩色边框
	const int32 Mode = Param(3).IsEmpty() ? 0 : FCString::Atoi(*Param(3));
	const bool bFrame = Mode == 3 || Mode == 4;
	bNoCountdown = Mode == 2 || Mode == 4;
	// 合并计层的图标（CueType 14，原作没有单个 buff 时）用圆形样式；项目目前每个图标都对应一个 GE
	const bool bRound = false;
	SetActive(FrameColour, bFrame);
	Icon->SetIsUIActive(false);
	if (BigIcon) BigIcon->SetIsUIActive(false);
	if (Mode == 1)
	{
		SetActive(Background, false);
		SetActive(WuwaHudBuffItem::Fill, false);
		SetActive(RoundBackground, false);
		SetActive(RoundFill, false);
		if (BigIcon && Data.Icon)
		{
			BigIcon->SetTexture(Data.Icon);
			BigIcon->SetIsUIActive(true);
		}
		FillTarget = nullptr;
	}
	else
	{
		SetActive(Background, !bRound && !bFrame);
		SetActive(WuwaHudBuffItem::Fill, !bRound);
		SetActive(RoundBackground, bRound && !bFrame);
		SetActive(RoundFill, bRound);
		if (Data.Icon)
		{
			Icon->SetTexture(Data.Icon);
			Icon->SetIsUIActive(true);
		}
		FillTarget = Cast<UUISprite>(Items.FindRef(bRound ? RoundFill : WuwaHudBuffItem::Fill));
	}
	// [0] 增益 / 减益角标；[1] 倒计时填充色；[2] 图标颜色
	SetActive(BuffMark, Param(0) == TEXT("1"));
	SetActive(DebuffMark, Param(0) == TEXT("2"));
	if (FillTarget) FillTarget->SetColor(Hex(Param(1), TEXT("FFFFFF7F")).ToFColorSRGB());
	Icon->SetColor(Hex(Param(2), TEXT("FFFFFFFF")).ToFColorSRGB());

	ShownNum = 0;
	SetNum(Data.StackCount);
	DisplayedFill = -1.f;
	SetFill(1.f);
	bActive = true;
	HideRemaining = 0.f;
	Root->SetIsUIActive(true);
	Stop(AnimClose);
	if (bPlayStart)
	{
		Play(AnimStart);
	}
	else
	{
		WuwaHudBuffItem::SetAlpha(Root, 1.f);
		Root->SetRelativeScale3D(FVector::OneVector);
	}
}

void UWuwaHudBuffItem::UpdateData(const FWuwaBuffUiData& InData)
{
	Data = InData;
	SetNum(Data.StackCount);
}

void UWuwaHudBuffItem::Tick(double WorldTime)
{
	if (!bActive) return;
	SetFill(Data.Duration > 0.f && !bNoCountdown ? Data.GetRemainPercent(WorldTime) : 1.f);
}

void UWuwaHudBuffItem::Deactivate(bool bWithCloseAnim)
{
	using namespace WuwaHudBuffItem;
	bActive = false;
	Stop(AnimStart);
	Root->SetRelativeScale3D(FVector::OneVector);
	SetFading(false);
	if (bWithCloseAnim)
	{
		Play(AnimClose);
		HideRemaining = CloseAnimSeconds;
		return;
	}
	Stop(AnimClose);
	Root->SetIsUIActive(false);
	HideRemaining = 0.f;
}

bool UWuwaHudBuffItem::TickHiding(float DeltaSeconds)
{
	if ((HideRemaining -= DeltaSeconds) > 0.f) return true;
	Stop(WuwaHudBuffItem::AnimClose);
	Root->SetIsUIActive(false);
	return false;
}

void UWuwaHudBuffItem::SetNum(int32 Num)
{
	if (Num == ShownNum) return;
	if (Num > ShownNum && ShownNum > 0) Play(WuwaHudBuffItem::AnimStart);
	ShownNum = Num;
	NumText->SetText(FText::FromString(Num <= 1 ? FString() : FString::FromInt(Num)));
}

void UWuwaHudBuffItem::SetFill(float Fill)
{
	if (Fill == DisplayedFill) return;
	DisplayedFill = Fill;
	if (FillTarget) FillTarget->SetFillAmount(Fill);
	SetFading(Fill <= WuwaHudBuffItem::FadePercent);
}

void UWuwaHudBuffItem::SetFading(bool bFade)
{
	if (bFading == bFade) return;
	bFading = bFade;
	if (bFade)
	{
		Play(WuwaHudBuffItem::AnimFade);
	}
	else
	{
		Stop(WuwaHudBuffItem::AnimFade);
		WuwaHudBuffItem::SetAlpha(Ctrl, 1.f);
	}
}

void UWuwaHudBuffItem::Play(int32 Index)
{
	if (UWuwaUiTweenGroup* Group = Animations.FindRef(Index)) Group->Play();
}

void UWuwaHudBuffItem::Stop(int32 Index)
{
	if (UWuwaUiTweenGroup* Group = Animations.FindRef(Index)) Group->Stop();
}
