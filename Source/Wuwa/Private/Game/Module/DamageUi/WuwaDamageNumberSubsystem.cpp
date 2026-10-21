#include "Game/Module/DamageUi/WuwaDamageNumberSubsystem.h"

#include "Core/Actor/UIContainerActor.h"
#include "Core/ActorComponent/LGUICanvas.h"
#include "Core/ActorComponent/UIItem.h"
#include "Core/ActorComponent/UIText.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Game/LGUI/Component/WuwaUiComponentsRegistry.h"
#include "Game/LGUI/Component/WuwaUiTweenGroup.h"
#include "Game/LGUI/Core/WuwaUiManager.h"
#include "Game/Module/BattleUi/WuwaBattleUiSettings.h"
#include "Game/NewWorld/Combat/WuwaDamage.h"
#include "GeometryModifier/UIEffectOutline.h"
#include "Kismet/GameplayStatics.h"
#include "Layout/LGUICanvasScaler.h"
#include "Misc/App.h"
#include "PrefabSystem/LGUIPrefab.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaDamageNumber, Log, All);

namespace WuwaDamageNumber
{
	/** 原作飘字的设计分辨率（高 1440）→ 项目画布（高 1080）。 */
	constexpr float CanvasScale = 1080.f / 1440.f;
	/** 原作 IMMUNITY_DAMAGE_TEXT_ID / CURE_DAMAGE_TEXT / ATK_DAMAGE_TEXT。 */
	constexpr int32 ImmunityTextId = 9;
	constexpr int32 CureTextId = 8;
	constexpr int32 AttackTextId = 10;

	/** 动画名 → 注册表下标（原作 DamageView.LFt，去掉 UINiagara 后的顺序）。 */
	const TMap<FName, int32>& AnimationIndices()
	{
		static const TMap<FName, int32> Indices = {
			{TEXT("Ani_OwnDamageSequence"), 4}, {TEXT("Ani_OwnCriticalDamageSequence"), 5}, {TEXT("Ani_MonsterDamageSequence"), 6},
			{TEXT("Ani_MonsterCriticalDamageSequence"), 7}, {TEXT("Ani_BuffSequence"), 8}, {TEXT("Ani_SpecialDamage"), 9}, {TEXT("Ani_SpecialCriticalDamage"), 10}};
		return Indices;
	}
}

// ---------------------------------------------------------------- UWuwaDamageNumber

bool UWuwaDamageNumber::Init(AActor* InActor)
{
	Actor = InActor;
	Root = InActor ? Cast<UUIItem>(InActor->GetRootComponent()) : nullptr;
	const UWuwaUiComponentsRegistry* Registry = InActor ? InActor->FindComponentByClass<UWuwaUiComponentsRegistry>() : nullptr;
	if (!Root || !Registry) return false;
	auto ItemAt = [Registry](int32 Index) -> UUIItem*
	{
		const AActor* Entry = Registry->GetActorAt(Index);
		return Entry ? Cast<UUIItem>(Entry->GetRootComponent()) : nullptr;
	};
	NumberText = Cast<UUIText>(ItemAt(0));
	WordText = Cast<UUIText>(ItemAt(2));
	CriticalItem = ItemAt(3);
	NumberOutline = NumberText ? NumberText->GetOwner()->FindComponentByClass<UUIEffectOutline>() : nullptr;
	for (const TPair<FName, int32>& Pair : WuwaDamageNumber::AnimationIndices())
	{
		if (const AActor* Entry = Registry->GetActorAt(Pair.Value))
		{
			if (UWuwaUiTweenGroup* Group = Entry->FindComponentByClass<UWuwaUiTweenGroup>()) Animations.Add(Pair.Key, Group);
		}
	}
	// 根节点锚在画布左下角，位置 = 投影后的画布坐标 + 偏移
	FUIAnchorData Data = Root->GetAnchorData();
	Data.AnchorMin = Data.AnchorMax = FVector2D::ZeroVector;
	Root->SetAnchorData(Data);
	Hide();
	return NumberText != nullptr;
}

void UWuwaDamageNumber::Show(const FWuwaDamageNumberRequest& Request, const FWuwaDamageTextInfo& Info, const FVector2D& CanvasOffset)
{
	WorldLocation = Request.WorldLocation;
	Offset = CanvasOffset;
	Remaining = Lifetime;
	const bool bText = !Request.Text.IsEmpty();
	// 动画（原作 GetSequencePath）：文字型 / 自己（暴击）/ 打怪（暴击）
	PlayingAnimation = bText ? Info.DamageTextSequence
		: Request.bOwnPlayer ? (Request.bCritical ? Info.OwnCriticalDamageSequence : Info.OwnDamageSequence)
		: (Request.bCritical ? Info.MonsterCriticalDamageSequence : Info.MonsterDamageSequence);
	// 文字和颜色（原作 IFt / PFt）：数字是取整后的绝对值，治疗前面加 +
	const FColor TextColor = Request.bCritical ? Info.CritTextColor : Info.TextColor;
	const FColor StrokeColor = Request.bCritical ? Info.CritStrokeColor : Info.StrokeColor;
	const FString Number = FString::Printf(TEXT("%s%lld"), Request.bCure ? TEXT("+") : TEXT(""), static_cast<int64>(FMath::FloorToDouble(FMath::Abs(Request.Damage))));
	NumberText->SetColor(TextColor);
	if (NumberOutline) NumberOutline->SetOutlineColor(StrokeColor);
	NumberText->SetText(FText::FromString(bText ? Request.Text : Number));
	NumberText->SetIsUIActive(true);
	if (WordText) WordText->SetIsUIActive(false);
	if (CriticalItem) CriticalItem->SetIsUIActive(Request.bCritical);
	Actor->SetActorHiddenInGame(false);
	Root->SetIsUIActive(true);
	Root->SetAsLastHierarchy();   // 新的数字在上面
	NumberText->SetAlpha(0.f);     // 原作先置 0，由动画的透明度曲线显示出来
	if (Playing) Playing->Stop();
	Playing = Animations.FindRef(PlayingAnimation);
	if (Playing) Playing->Play();
}

void UWuwaDamageNumber::Hide()
{
	if (Playing) Playing->Stop();
	Playing = nullptr;
	if (Root) Root->SetIsUIActive(false);
}

bool UWuwaDamageNumber::Tick(float DeltaSeconds)
{
	Remaining -= DeltaSeconds;
	return Remaining > 0.f;
}

void UWuwaDamageNumber::SetCanvasPosition(const FVector2D& CanvasPosition)
{
	if (Root) Root->SetAnchoredPosition(CanvasPosition + Offset);
}

// ---------------------------------------------------------------- UWuwaDamageNumberSubsystem

UWuwaDamageNumberSubsystem* UWuwaDamageNumberSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UWuwaDamageNumberSubsystem>() : nullptr;
}

bool UWuwaDamageNumberSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UWuwaDamageNumberSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UWuwaDamageSubsystem* Damage = Collection.InitializeDependency<UWuwaDamageSubsystem>())
	{
		DamageHandle = Damage->OnDamage.AddUObject(this, &UWuwaDamageNumberSubsystem::HandleDamage);
	}
}

void UWuwaDamageNumberSubsystem::Deinitialize()
{
	if (UWuwaDamageSubsystem* Damage = GetWorld() ? GetWorld()->GetSubsystem<UWuwaDamageSubsystem>() : nullptr)
	{
		Damage->OnDamage.Remove(DamageHandle);
	}
	Super::Deinitialize();
}

TStatId UWuwaDamageNumberSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWuwaDamageNumberSubsystem, STATGROUP_Tickables);
}

int32 UWuwaDamageNumberSubsystem::SelectTextId(int32 Element, bool bCure, float Damage, int32 DamageTextType)
{
	if (DamageTextType != 0 && DamageTextType != -1 && DamageTextType != 2) return DamageTextType;
	if (Damage == 0.f) return WuwaDamageNumber::ImmunityTextId;
	if (Element > 0) return Element;
	return bCure ? WuwaDamageNumber::CureTextId : WuwaDamageNumber::AttackTextId;
}

void UWuwaDamageNumberSubsystem::SetVisible(bool bInVisible)
{
	bVisible = bInVisible;
	if (!bVisible)
	{
		Queue.Reset();
		for (UWuwaDamageNumber* Number : Active) Release(Number);
		Active.Reset();
	}
}

void UWuwaDamageNumberSubsystem::HandleDamage(const FWuwaDamageEvent& Event)
{
	AActor* Victim = Event.Victim.Get();
	if (!IsValid(Victim)) return;
	const APlayerController* Player = GetWorld()->GetFirstPlayerController();
	FWuwaDamageNumberRequest Request;
	Request.Damage = Event.Damage;
	Request.Element = Event.Element;
	Request.WorldLocation = Event.HitLocation;
	Request.bOwnPlayer = Player && Player->GetPawn() == Victim;
	Request.bCritical = Event.bCritical;
	Request.DamageTextType = Event.DamageTextType;
	ApplyDamage(Request);
}

void UWuwaDamageNumberSubsystem::ApplyDamage(const FWuwaDamageNumberRequest& Request)
{
	// 原作：DamageTextType < 0 不飘；治疗、类型 1、数值 0 也不飘
	if (!bVisible || Request.DamageTextType < 0 || (Request.DamageTextType == 1 && Request.bCure && Request.Damage == 0.f)) return;
	if (GetWorld()->GetNetMode() == NM_DedicatedServer) return;
	Queue.Add(Request);
}

void UWuwaDamageNumberSubsystem::Tick(float DeltaTime)
{
	// 不受时间膨胀影响（原作按 InverseSelfCenteredTimeDilation 还原）
	const float RealDelta = static_cast<float>(FApp::GetDeltaTime());
	for (int32 Index = Active.Num() - 1; Index >= 0; --Index)
	{
		UWuwaDamageNumber* Number = Active[Index];
		if (!Number->Tick(RealDelta))
		{
			Release(Number);
			Active.RemoveAt(Index);
			continue;
		}
		FVector2D Canvas;
		if (Project(Number->GetWorldLocation(), Canvas)) Number->SetCanvasPosition(Canvas);
	}
	// 每帧最多出一个（原作 MAX_DAMAGE_PER_FRAME）
	if (Queue.Num() > 0 && EnsureContainer())
	{
		const FWuwaDamageNumberRequest Request = Queue[0];
		Queue.RemoveAt(0);
		Spawn(Request);
	}
}

bool UWuwaDamageNumberSubsystem::EnsureContainer()
{
	if (IsValid(Container) && IsValid(Scaler)) return true;
	UWuwaUiManager* Manager = UWuwaUiManager::Get(GetWorld());
	UUIItem* Layer = Manager ? Manager->GetLayerItem(EWuwaUiLayer::HUD) : nullptr;
	ULGUICanvas* RootCanvas = Layer ? Layer->GetRootCanvas() : nullptr;
	Scaler = RootCanvas ? RootCanvas->GetOwner()->FindComponentByClass<ULGUICanvasScaler>() : nullptr;
	if (!Layer || !Scaler)
	{
		UE_LOG(LogWuwaDamageNumber, Verbose, TEXT("No HUD layer / canvas scaler yet (layer %s)."), *GetNameSafe(Layer));
		return false;
	}
	const UWuwaBattleUiSettings* Settings = GetDefault<UWuwaBattleUiSettings>();
	Prefab = Settings->DamageViewPrefab.LoadSynchronous();
	TextTable = Settings->DamageTextTable.LoadSynchronous();
	if (!Prefab || !TextTable)
	{
		UE_LOG(LogWuwaDamageNumber, Warning, TEXT("Damage view prefab / text table missing (WuwaBattleUiSettings)."));
		return false;
	}
	// HUD 层最底下的全屏容器（≈ 原作 BattleViewUnit(0)）：伤害数字在 HUD 下面
	AUIContainerActor* Holder = GetWorld()->SpawnActor<AUIContainerActor>();
	Holder->SetActorLabel(TEXT("DamageNumbers"));
	Container = Holder->GetUIItem();
	Container->AttachToComponent(Layer, FAttachmentTransformRules::KeepRelativeTransform);
	FUIAnchorData Data;
	Data.AnchorMin = FVector2D::ZeroVector;
	Data.AnchorMax = FVector2D::UnitVector;
	Data.Pivot = FVector2D(0.5, 0.5);
	Container->SetAnchorData(Data);
	Container->SetRaycastTarget(false);
	Container->SetAsFirstHierarchy();
	return true;
}

UWuwaDamageNumber* UWuwaDamageNumberSubsystem::Acquire()
{
	if (Pool.Num() > 0) return Pool.Pop();
	AActor* Actor = Prefab->LoadPrefab(GetWorld(), Container, true);
	UWuwaDamageNumber* Number = NewObject<UWuwaDamageNumber>(this);
	if (!Number->Init(Actor))
	{
		if (Actor) Actor->Destroy();
		return nullptr;
	}
	return Number;
}

void UWuwaDamageNumberSubsystem::Release(UWuwaDamageNumber* Number)
{
	if (!Number) return;
	Number->Hide();
	Pool.Add(Number);
}

void UWuwaDamageNumberSubsystem::Spawn(const FWuwaDamageNumberRequest& Request)
{
	const int32 TextId = SelectTextId(Request.Element, Request.bCure, Request.Damage, Request.DamageTextType);
	const FWuwaDamageTextInfo* Info = TextTable->FindRow<FWuwaDamageTextInfo>(FName(*FString::FromInt(TextId)), TEXT("DamageNumber"), false);
	FVector2D Canvas;
	if (!Info)
	{
		UE_LOG(LogWuwaDamageNumber, Warning, TEXT("Damage text row %d not found."), TextId);
		return;
	}
	// 原作：出现时投影失败（在身后 / 屏幕外）就不显示
	if (!Project(Request.WorldLocation, Canvas))
	{
		const APlayerController* Player = GetWorld()->GetFirstPlayerController();
		int32 SizeX = 0, SizeY = 0;
		if (Player) Player->GetViewportSize(SizeX, SizeY);
		FVector2D Screen;
		const bool bProjected = Player && UGameplayStatics::ProjectWorldToScreen(Player, Request.WorldLocation, Screen, false);
		UE_LOG(LogWuwaDamageNumber, Verbose, TEXT("Damage number at %s is off screen (viewport %dx%d, projected %d -> %s, camera %s)."),
			*Request.WorldLocation.ToCompactString(), SizeX, SizeY, bProjected, *Screen.ToString(),
			Player && Player->PlayerCameraManager ? *Player->PlayerCameraManager->GetCameraLocation().ToCompactString() : TEXT("-"));
		return;
	}
	UWuwaDamageNumber* Number = Acquire();
	if (!Number)
	{
		UE_LOG(LogWuwaDamageNumber, Warning, TEXT("Could not create a damage number from %s."), *GetNameSafe(Prefab));
		return;
	}
	// 随机偏移（原作 InitializeData：Y 的上限用的是 MaxDeviationX，照做）
	const FVector2D Offset(FMath::FRandRange(Info->MinDeviationX, Info->MaxDeviationX) * WuwaDamageNumber::CanvasScale,
		FMath::FRandRange(Info->MinDeviationY, Info->MaxDeviationX) * WuwaDamageNumber::CanvasScale);
	Number->Show(Request, *Info, Offset);
	Number->SetCanvasPosition(Canvas);
	Active.Add(Number);
}

bool UWuwaDamageNumberSubsystem::Project(const FVector& WorldLocation, FVector2D& OutCanvas) const
{
	APlayerController* Player = GetWorld()->GetFirstPlayerController();
	FVector2D Screen;
	if (!Player || !IsValid(Scaler) || !UGameplayStatics::ProjectWorldToScreen(Player, WorldLocation, Screen, false)) return false;
	if (!FMath::IsFinite(Screen.X) || !FMath::IsFinite(Screen.Y)) return false;
	OutCanvas = Scaler->ConvertPositionFromViewportToLGUICanvas(Screen);
	return true;
}
