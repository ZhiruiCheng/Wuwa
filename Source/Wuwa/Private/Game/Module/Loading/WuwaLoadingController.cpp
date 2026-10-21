#include "Game/Module/Loading/WuwaLoadingController.h"
#include "Game/Common/Resource/WuwaCombatWarmupSubsystem.h"

#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Game/Common/Resource/WuwaResourceSubsystem.h"
#include "Game/Formation/WuwaFormationComponent.h"
#include "Game/LGUI/Core/WuwaUISettings.h"
#include "Game/LGUI/Core/WuwaUiLog.h"
#include "Game/LGUI/Core/WuwaUiManager.h"
#include "Game/LGUI/View/WuwaUiState.h"
#include "Game/LGUI/View/WuwaUiView.h"
#include "Game/Module/Hud/WuwaMainHudController.h"
#include "Game/Module/Loading/WuwaLoadingModel.h"
#include "HAL/PlatformTime.h"
#include "Subsystems/SubsystemCollection.h"
#include "UObject/UObjectGlobals.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

const FName UWuwaLoadingController::LoadingViewName(TEXT("LoadingView"));

void UWuwaLoadingController::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UWuwaLoadingModel>();
	Collection.InitializeDependency<UWuwaMainHudController>();
	Super::Initialize(Collection);
}

// —— 加载界面 ——

void UWuwaLoadingController::BeginLoading()
{
	UWuwaLoadingModel* Model = GetModel<UWuwaLoadingModel>();
	UWuwaUiManager* UiManager = GetUiManager();
	if (!Model || !UiManager || Model->IsLoading())
	{
		return;
	}
	Model->Begin(PickShow());
	UE_LOG(LogWuwaUI, Log, TEXT("Loading: begin \"%s\""), *Model->GetShow().Title.ToString());
	UiManager->OpenView(LoadingViewName);
}

void UWuwaLoadingController::SetPhaseProgress(EWuwaLoadingPhase Phase, float Alpha)
{
	if (UWuwaLoadingModel* Model = GetModel<UWuwaLoadingModel>())
	{
		Model->SetPhaseProgress(Phase, Alpha);
	}
}

void UWuwaLoadingController::FinishLoading()
{
	if (UWuwaLoadingModel* Model = GetModel<UWuwaLoadingModel>())
	{
		Model->Finish();
	}
	// 界面没打开（配置缺失、打开失败）就没人会回调 HandleLoadingViewFinished，直接收尾。
	UWuwaUiManager* UiManager = GetUiManager();
	if (!UiManager || !UiManager->GetView(LoadingViewName))
	{
		HandleLoadingViewFinished();
	}
}

void UWuwaLoadingController::HandleLoadingViewFinished()
{
	if (UWuwaLoadingModel* Model = GetModel<UWuwaLoadingModel>())
	{
		Model->Reset();
	}
	if (UWuwaUiManager* UiManager = GetUiManager())
	{
		UiManager->CloseView(LoadingViewName);
	}
	UE_LOG(LogWuwaUI, Log, TEXT("Loading: closed"));
}

bool UWuwaLoadingController::IsLoading() const
{
	const UWuwaLoadingModel* Model = GetModel<UWuwaLoadingModel>();
	return Model && Model->IsLoading();
}

FWuwaLoadingShowRow UWuwaLoadingController::PickShow() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UGameInstance* GameInstance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
	UWuwaResourceSubsystem* Resources = GameInstance ? GameInstance->GetSubsystem<UWuwaResourceSubsystem>() : nullptr;
	const TSoftObjectPtr<UDataTable>& TableRef = GetDefault<UWuwaUISettings>()->LoadingShowTable;
	// 展示表很小（只有文字和背景图的软引用），同步加载即可。
	const UDataTable* Table = Resources ? Resources->LoadSync(TableRef, TEXT("LoadingShowTable")) : TableRef.LoadSynchronous();
	if (!Table || Table->GetRowStruct() != FWuwaLoadingShowRow::StaticStruct())
	{
		return FWuwaLoadingShowRow();
	}
	// 按权重随机（≈ 鸣潮 LoadingShowData 按 Weight 抽一张图）。
	TArray<const FWuwaLoadingShowRow*> Rows;
	float TotalWeight = 0.f;
	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		const FWuwaLoadingShowRow* Row = reinterpret_cast<const FWuwaLoadingShowRow*>(Pair.Value);
		if (Row && Row->Weight > 0.f)
		{
			Rows.Add(Row);
			TotalWeight += Row->Weight;
		}
	}
	float Pick = FMath::FRandRange(0.f, TotalWeight);
	for (const FWuwaLoadingShowRow* Row : Rows)
	{
		Pick -= Row->Weight;
		if (Pick <= 0.f)
		{
			return *Row;
		}
	}
	return Rows.IsEmpty() ? FWuwaLoadingShowRow() : *Rows.Last();
}

// —— 进入游戏地图的加载流程 ——

void UWuwaLoadingController::OnInit()
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}
	// 本地玩家先于 PC 存在；PC 生成并关联到本地玩家时（ULocalPlayer::ReceivedPlayerController）开始进入地图的流程。
	PlayerControllerChangedHandle = LocalPlayer->OnPlayerControllerChanged().AddUObject(this, &UWuwaLoadingController::HandlePlayerControllerChanged);
	if (APlayerController* Existing = LocalPlayer->GetPlayerController(nullptr))
	{
		HandlePlayerControllerChanged(Existing);
	}
}

void UWuwaLoadingController::OnClear()
{
	StopWorldLoad();
	UnbindWorldBeginPlay();
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		LocalPlayer->OnPlayerControllerChanged().Remove(PlayerControllerChangedHandle);
	}
	PlayerControllerChangedHandle.Reset();
}

void UWuwaLoadingController::OnLeaveLevel()
{
	// 旧世界的界面由 UiManager 清理；流程跟着旧世界一起结束，新地图的 PC 出现时重新开始。
	StopWorldLoad();
	UnbindWorldBeginPlay();
	if (UWuwaLoadingModel* Model = GetModel<UWuwaLoadingModel>())
	{
		Model->Reset();
	}
}

void UWuwaLoadingController::HandlePlayerControllerChanged(APlayerController* NewController)
{
	UnbindWorldBeginPlay();
	if (!UWuwaMainHudController::ShouldShowMainHud(NewController))
	{
		return;
	}
	UWorld* World = NewController->GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	// PC 往往在世界 BeginPlay 之前就生成了，等世界开始运行再打开界面。
	if (!World->HasBegunPlay())
	{
		PendingWorld = World;
		PendingController = NewController;
		WorldBeginPlayHandle = World->OnWorldBeginPlay.AddUObject(this, &UWuwaLoadingController::HandleWorldBeginPlay);
		return;
	}
	StartWorldLoad(NewController);
}

void UWuwaLoadingController::HandleWorldBeginPlay()
{
	APlayerController* PlayerController = PendingController.Get();
	UnbindWorldBeginPlay();
	if (PlayerController)
	{
		StartWorldLoad(PlayerController);
	}
}

void UWuwaLoadingController::UnbindWorldBeginPlay()
{
	if (UWorld* World = PendingWorld.Get())
	{
		World->OnWorldBeginPlay.Remove(WorldBeginPlayHandle);
	}
	WorldBeginPlayHandle.Reset();
	PendingWorld.Reset();
	PendingController.Reset();
}

void UWuwaLoadingController::StartWorldLoad(APlayerController* PlayerController)
{
	StopWorldLoad();
	LoadingPlayer = PlayerController;
	WorldLoadStartTime = FPlatformTime::Seconds();
	LastProgressTime = WorldLoadStartTime;
	LastAsyncPackages = -1;
	PeakAsyncPackages = 0;
	LastPhaseAlpha = -1.f;
	SetHighPriorityLoading(PlayerController->GetWorld(), true);
	BeginLoading();
	WorldLoadTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaSeconds)
	{
		return TickWorldLoad(DeltaSeconds);
	}));
}

void UWuwaLoadingController::StopWorldLoad()
{
	if (WorldLoadTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(WorldLoadTicker);
		WorldLoadTicker.Reset();
	}
	SetHighPriorityLoading(HighPriorityWorld.Get(), false);
	LoadingPlayer.Reset();
}

void UWuwaLoadingController::SetHighPriorityLoading(UWorld* World, bool bEnable)
{
	// 引擎默认每帧只给异步加载 5ms（s.AsyncLoadingTimeLimit），而且遇到磁盘等待就让出；
	// 高优先级加载时每帧多给 s.PriorityAsyncLoadingExtraTime（默认 15ms）并用满（LevelTick.cpp）。加载界面挡着画面，正适合用。
	AWorldSettings* Settings = World ? World->GetWorldSettings(false, false) : nullptr;
	if (bEnable)
	{
		if (!Settings || HighPriorityWorld.IsValid()) return;
		HighPriorityWorld = World;
		bPreviousHighPriorityLoading = Settings->bHighPriorityLoadingLocal;
		Settings->bHighPriorityLoadingLocal = true;
#if WITH_EDITOR
		// 编辑器切到后台时整个编辑器（含 PIE）限到 3 帧（EditorEngine.cpp GetMaxTickRate），加载期间不让它降
		if (GEditor && !EditorThrottleHandle.IsValid())
		{
			GEditor->ShouldDisableCPUThrottlingDelegates.Add(UEditorEngine::FShouldDisableCPUThrottling::CreateLambda([]() { return true; }));
			EditorThrottleHandle = GEditor->ShouldDisableCPUThrottlingDelegates.Last().GetHandle();
		}
#endif
		return;
	}
	if (Settings && HighPriorityWorld.Get() == World)
	{
		Settings->bHighPriorityLoadingLocal = bPreviousHighPriorityLoading;
	}
	HighPriorityWorld.Reset();
#if WITH_EDITOR
	if (GEditor && EditorThrottleHandle.IsValid())
	{
		GEditor->ShouldDisableCPUThrottlingDelegates.RemoveAll([this](const UEditorEngine::FShouldDisableCPUThrottling& Delegate)
		{
			return Delegate.GetHandle() == EditorThrottleHandle;
		});
	}
	EditorThrottleHandle.Reset();
#endif
}

bool UWuwaLoadingController::TickWorldLoad(float DeltaSeconds)
{
	APlayerController* PlayerController = LoadingPlayer.Get();
	UWuwaFormationComponent* Formation = PlayerController ? PlayerController->FindComponentByClass<UWuwaFormationComponent>() : nullptr;
	if (!Formation)
	{
		WorldLoadTicker.Reset();
		SetHighPriorityLoading(HighPriorityWorld.Get(), false);
		FinishLoading();
		return false;
	}
	// 进展：异步加载队列的包数有变化（在加载 / 有新请求），或者流程往前走了（见下面 Advance）。
	const double Now = FPlatformTime::Seconds();
	const int32 AsyncPackages = GetNumAsyncPackages();
	if (AsyncPackages != LastAsyncPackages)
	{
		LastAsyncPackages = AsyncPackages;
		LastProgressTime = Now;
	}
	PeakAsyncPackages = FMath::Max(PeakAsyncPackages, AsyncPackages);
	if (Now - LastProgressTime > WorldLoadTimeoutSeconds)
	{
		UE_LOG(LogWuwaUI, Warning, TEXT("Loading: no loading progress for %.0fs (%d async packages, %.0fs in total); closing the loading screen."),
			WorldLoadTimeoutSeconds, AsyncPackages, Now - WorldLoadStartTime);
		WorldLoadTicker.Reset();
		SetHighPriorityLoading(HighPriorityWorld.Get(), false);
		FinishLoading();
		return false;
	}
	auto Advance = [this, Now](EWuwaLoadingPhase Phase, float Alpha)
	{
		if (Alpha > LastPhaseAlpha + KINDA_SMALL_NUMBER || Alpha < LastPhaseAlpha - KINDA_SMALL_NUMBER)
		{
			LastPhaseAlpha = Alpha;
			LastProgressTime = Now;
		}
		SetPhaseProgress(Phase, Alpha);
	};

	// ① 角色资源：异步加载句柄只按请求的资源个数算进度（头像很快、角色蓝图要等全部依赖），
	//    中间按异步队列里剩下的包数补上，进度条才会连续往前走。
	const float PackageAlpha = PeakAsyncPackages > 0 ? 1.f - static_cast<float>(AsyncPackages) / PeakAsyncPackages : 0.f;
	Advance(EWuwaLoadingPhase::Formation, FMath::Max(Formation->GetLoadProgress(), PackageAlpha * 0.95f));
	// ② 队员生成好、控制了第一个角色（客户端：复制过来）。
	if (!Formation->IsFormationReady())
	{
		return true;
	}
	SetPhaseProgress(EWuwaLoadingPhase::Formation, 1.f);
	Advance(EWuwaLoadingPhase::SpawnRoles, 1.f);

	// ③ 战斗预热：队员和场景里的单位都在了，把技能、命中时才会同步加载的资源提前加载好（加载界面挡着，卡一下也看不见）。
	if (UWuwaCombatWarmupSubsystem* Warmup = UWuwaCombatWarmupSubsystem::Get(PlayerController))
	{
		Warmup->Begin();
		if (!Warmup->IsDone())
		{
			Advance(EWuwaLoadingPhase::Warmup, Warmup->GetProgress());
			return true;
		}
	}
	Advance(EWuwaLoadingPhase::Warmup, 1.f);

	// ④ 主 HUD（≈ 鸣潮 OpenBattleViewFromLoading）：在加载界面下面打开，显示出来才算完成。
	UWuwaMainHudController* HudController = GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UWuwaMainHudController>() : nullptr;
	const UWuwaUiView* Hud = HudController ? HudController->OpenMainHud() : nullptr;
	if (Hud && Hud->GetState() != EWuwaUiState::Shown)
	{
		Advance(EWuwaLoadingPhase::MainHud, Hud->GetPrefabLoadProgress() * 0.9f);
		return true;
	}

	// ⑤ 全部就绪（≈ GameModeCloseLoading）：进度走满，界面显示到 100% 后自己关闭。
	SetPhaseProgress(EWuwaLoadingPhase::MainHud, 1.f);
	UE_LOG(LogWuwaUI, Log, TEXT("Loading: world ready in %.2fs"), FPlatformTime::Seconds() - WorldLoadStartTime);
	WorldLoadTicker.Reset();
	SetHighPriorityLoading(HighPriorityWorld.Get(), false);
	FinishLoading();
	return false;
}
