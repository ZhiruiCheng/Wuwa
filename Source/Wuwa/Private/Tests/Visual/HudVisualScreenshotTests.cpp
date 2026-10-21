#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Core/ActorComponent/UIItem.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Game/LGUI/Core/WuwaUiManager.h"
#include "Game/LGUI/Component/WuwaUINiagara.h"
#include "NiagaraSystem.h"
#include "HAL/FileManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "LGUIBPLibrary.h"
#include "Misc/Paths.h"
#include "Core/ActorComponent/UIText.h"
#include "Core/ActorComponent/UITexture.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Game/LGUI/Component/WuwaUiComponentsRegistry.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Formation/WuwaFormationComponent.h"
#include "Game/LGUI/View/WuwaUiState.h"
#include "Game/Module/Formation/WuwaFormationUiController.h"
#include "Game/Module/Hud/WuwaHudRoleStatePanel.h"
#include "Game/Module/Hud/WuwaMainHudController.h"
#include "Game/Module/Hud/WuwaMainHudView.h"
#include "Engine/LocalPlayer.h"
#include "Game/Input/WuwaInputTypes.h"
#include "AbilitySystemComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
#include "Game/Module/Loading/WuwaLoadingTypes.h"
#include "PrefabSystem/LGUIPrefab.h"
#include "RHI.h"
#include "TextureCompiler.h"
#include "UnrealClient.h"

// 只用于人工看画面：在 LGUI 测试地图里把 HUD_Main 放到 HUD 层并截图，名字不含 "Wuwa"，不随常规测试套件运行。
namespace HudVisualScreenshots
{
	UWorld* FindPlayWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) return Context.World();
		}
		return nullptr;
	}

	bool SavePlayViewport(UWorld* World, const FString& Path)
	{
		UGameViewportClient* ViewportClient = World->GetGameViewport();
		FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
		if (!Viewport || Viewport->GetSizeXY().X <= 0) return false;
		TArray<FColor> Pixels;
		if (!Viewport->ReadPixels(Pixels)) return false;
		for (FColor& Pixel : Pixels) Pixel.A = 255;
		const FIntPoint Size = Viewport->GetSizeXY();
		return FImageUtils::SaveImageByExtension(*Path, FImageView(Pixels.GetData(), Size.X, Size.Y));
	}

	class FCaptureHud final : public IAutomationLatentCommand
	{
	public:
		FCaptureHud(FAutomationTestBase* InTest, FString InPath) : Test(InTest), Path(MoveTemp(InPath)) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			UWorld* World = FindPlayWorld();
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			UWuwaUiManager* Manager = Player ? UWuwaUiManager::Get(Player) : nullptr;
			if (!Manager)
			{
				if (Now - StartedAt > 20.0) { Test->AddError(TEXT("No UiManager in PIE.")); return true; }
				return false;
			}

			if (!bPlaced)
			{
				// 测试地图开局会打开 TestNormalView（挡在屏幕中间），先关掉；它也顺便让 UiManager 生成了根节点。
				if (Manager->IsViewOpen(TEXT("TestNormalView"))) Manager->CloseView(TEXT("TestNormalView"));
				UUIItem* HudLayer = Manager->GetLayerItem(EWuwaUiLayer::HUD);
				if (!HudLayer)
				{
					if (Now - StartedAt > 20.0) { Test->AddError(TEXT("HUD layer not ready.")); return true; }
					return false;
				}
				// 先加载预制体资源（连带加载它引用的贴图），等贴图编译完再实例化：
				// 编辑器里新贴图在后台编译，LGUI 实例化时拿到的如果是占位图，之后不会自己换掉。
				ULGUIPrefab* Prefab = LoadObject<ULGUIPrefab>(nullptr, TEXT("/Game/LGUI/HUD/HUD_Main.HUD_Main"));
				if (!Test->TestNotNull(TEXT("HUD_Main prefab"), Prefab)) return true;
				FTextureCompilingManager::Get().FinishAllCompilation();
				Hud = Prefab->LoadPrefab(World, HudLayer, true);
				bPlaced = true;
				PlacedAt = Now;
				return false;
			}

			// 多等一会儿：贴图流送、LGUI 第一次布局都需要几帧。
			if (Now - PlacedAt < 2.0) return false;
			Test->TestTrue(TEXT("saved screenshot"), SavePlayViewport(World, Path));
			Test->AddInfo(FString::Printf(TEXT("HUD screenshot: %s"), *Path));
			if (Hud.IsValid()) ULGUIBPLibrary::DestroyActorWithHierarchy(Hud.Get(), true);
			return true;
		}

	private:
		FAutomationTestBase* Test;
		FString Path;
		double StartedAt = -1.0;
		double PlacedAt = 0.0;
		bool bPlaced = false;
		TWeakObjectPtr<AActor> Hud;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHudVisualScreenshotTest, "UIVisual.HudScreenshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHudVisualScreenshotTest::RunTest(const FString& Parameters)
{
	if (GUsingNullRHI)
	{
		AddWarning(TEXT("Needs a real RHI to take screenshots; skipped under NullRHI."));
		return true;
	}
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Diagnostics/HudScreenshots"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	if (!AutomationOpenMap(TEXT("/Game/Map/LGUI")))
	{
		AddError(TEXT("Could not open /Game/Map/LGUI."));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(HudVisualScreenshots::FCaptureHud(this, Directory / TEXT("HUD_Main.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

namespace LoadingVisualScreenshots
{
	// 把 Loading_Prefab 放到加载层，填入展示表第一行、进度设成 31%（和参考图一样），截图。
	class FCaptureLoading final : public IAutomationLatentCommand
	{
	public:
		FCaptureLoading(FAutomationTestBase* InTest, FString InPath) : Test(InTest), Path(MoveTemp(InPath)) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			UWorld* World = HudVisualScreenshots::FindPlayWorld();
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			UWuwaUiManager* Manager = Player ? UWuwaUiManager::Get(Player) : nullptr;
			UUIItem* Layer = Manager ? Manager->GetLayerItem(EWuwaUiLayer::Loading) : nullptr;
			if (!Layer)
			{
				if (Now - StartedAt > 20.0) { Test->AddError(TEXT("Loading layer not ready.")); return true; }
				return false;
			}
			if (!bPlaced)
			{
				ULGUIPrefab* Prefab = LoadObject<ULGUIPrefab>(nullptr, TEXT("/Game/LGUI/Loading/Loading_Prefab.Loading_Prefab"));
				const UDataTable* Shows = LoadObject<UDataTable>(nullptr, TEXT("/Game/LGUI/Loading/DT_LoadingShow.DT_LoadingShow"));
				if (!Test->TestNotNull(TEXT("Loading_Prefab"), Prefab) || !Test->TestNotNull(TEXT("DT_LoadingShow"), Shows)) return true;
				const FWuwaLoadingShowRow* Show = Shows->FindRow<FWuwaLoadingShowRow>(TEXT("Show_01"), TEXT("screenshot"));
				if (Show) Show->Background.LoadSynchronous();
				FTextureCompilingManager::Get().FinishAllCompilation();
				Loading = Prefab->LoadPrefab(World, Layer, true);
				const UWuwaUiComponentsRegistry* Registry = Loading.IsValid() ? Loading->FindComponentByClass<UWuwaUiComponentsRegistry>() : nullptr;
				if (!Test->TestNotNull(TEXT("Loading_Prefab registry"), Registry)) return true;
				auto Comp = [Registry](int32 Index, UClass* Class) { AActor* A = Registry->GetActorAt(Index); return A ? A->GetComponentByClass(Class) : nullptr; };
				if (Show)
				{
					if (UUITexture* Bg = Cast<UUITexture>(Comp(0, UUITexture::StaticClass()))) Bg->SetTexture(Show->Background.Get());
					if (UUIText* Title = Cast<UUIText>(Comp(1, UUIText::StaticClass()))) Title->SetText(Show->Title);
					if (UUIText* Desc = Cast<UUIText>(Comp(2, UUIText::StaticClass()))) Desc->SetText(Show->Description);
				}
				Fill = Cast<UUITexture>(Comp(3, UUITexture::StaticClass()));
				Track = Cast<UUIItem>(Comp(5, UUIItem::StaticClass()));
				if (UUIText* Percent = Cast<UUIText>(Comp(4, UUIText::StaticClass()))) Percent->SetText(FText::FromString(TEXT("31 %")));
				bPlaced = true;
				PlacedAt = Now;
				return false;
			}
			// 底槽的宽度要等第一次布局后才是屏幕宽度，所以每帧都按它重设一次进度条。
			if (Fill.IsValid() && Track.IsValid()) Fill->SetWidth(Track->GetWidth() * 0.31f);
			if (Now - PlacedAt < 2.0) return false;
			Test->TestTrue(TEXT("saved screenshot"), HudVisualScreenshots::SavePlayViewport(World, Path));
			Test->AddInfo(FString::Printf(TEXT("Loading screenshot: %s"), *Path));
			if (Loading.IsValid()) ULGUIBPLibrary::DestroyActorWithHierarchy(Loading.Get(), true);
			return true;
		}

	private:
		FAutomationTestBase* Test;
		FString Path;
		double StartedAt = -1.0;
		double PlacedAt = 0.0;
		bool bPlaced = false;
		TWeakObjectPtr<AActor> Loading;
		TWeakObjectPtr<UUITexture> Fill;
		TWeakObjectPtr<UUIItem> Track;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLoadingVisualScreenshotTest, "UIVisual.LoadingScreenshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLoadingVisualScreenshotTest::RunTest(const FString& Parameters)
{
	if (GUsingNullRHI)
	{
		AddWarning(TEXT("Needs a real RHI to take screenshots; skipped under NullRHI."));
		return true;
	}
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Diagnostics/HudScreenshots"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	if (!AutomationOpenMap(TEXT("/Game/Map/LGUI")))
	{
		AddError(TEXT("Could not open /Game/Map/LGUI."));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(LoadingVisualScreenshots::FCaptureLoading(this, Directory / TEXT("Loading.png")));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}


// 真实游戏里的主 HUD（编队、面板逻辑都在跑）：开局截一张，切到 2 号角色等能量条换好再截一张。
namespace HudInGameScreenshots
{
	class FCaptureInGame final : public IAutomationLatentCommand
	{
	public:
		FCaptureInGame(FAutomationTestBase* InTest, FString InDirectory) : Test(InTest), Directory(MoveTemp(InDirectory)) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			if (Now - StartedAt > 120.0) { Test->AddError(TEXT("HUD never became ready.")); return true; }
			UWorld* World = HudVisualScreenshots::FindPlayWorld();
			AWuwaPlayerController* PC = World ? Cast<AWuwaPlayerController>(World->GetFirstPlayerController()) : nullptr;
			UWuwaFormationComponent* Formation = PC ? PC->FormationComponent.Get() : nullptr;
			UWuwaUiManager* Manager = PC ? UWuwaUiManager::Get(PC) : nullptr;
			const UWuwaMainHudView* Hud = Manager ? Cast<UWuwaMainHudView>(Manager->GetView(UWuwaMainHudController::MainHudViewName)) : nullptr;
			const UWuwaHudRoleStatePanel* RoleState = Hud ? Hud->GetRoleState() : nullptr;
			if (!Formation || Formation->GetMembers().Num() < 2 || !Hud || Hud->GetState() != EWuwaUiState::Shown || !RoleState || !RoleState->GetEnergyBarActor())
			{
				ReadyAt = -1.0;
				return false;
			}
			if (ReadyAt < 0.0)
			{
				FTextureCompilingManager::Get().FinishAllCompilation();
				ReadyAt = Now;
			}
			// 贴图流送、LGUI 布局、加载界面淡出都要一会儿
			if (Now - ReadyAt < 3.0) return false;
			const FString Path = Directory / FString::Printf(TEXT("HUD_InGame_%d.png"), Shot);
			Test->TestTrue(*FString::Printf(TEXT("saved %s"), *Path), HudVisualScreenshots::SavePlayViewport(World, Path));
			Test->AddInfo(FString::Printf(TEXT("HUD screenshot: %s"), *Path));
			if (++Shot >= 4) return true;
			UAbilitySystemComponent* ASC = Formation->GetMembers()[Formation->GetCurrentIndex()]->GetAbilitySystemComponent();
			if (Shot == 3)
			{
				// 第四张：特殊能量满（变身外框换成可用的贴图、每格满）
				if (ASC) ASC->SetNumericAttributeBase(UWuwaAttributeSet::GetSpecialEnergy1Attribute(), ASC->GetNumericAttribute(UWuwaAttributeSet::GetMaxSpecialEnergy1Attribute()));
				ReadyAt = Now - 3.0 + 0.4;
				return false;
			}
			if (Shot == 1)
			{
				// 先按一次 E 预热：编辑器里界面特效的 Niagara 系统第一次用要等脚本编译，切人、加载能量条的这段时间正好编完
				FWuwaInputEvent Warm;
				Warm.InputTag = FGameplayTag::RequestGameplayTag(TEXT("Input.Combat.Skill"));
				Warm.RouteTag = FGameplayTag::RequestGameplayTag(TEXT("Input.Route.Ability"));
				Warm.Phase = EWuwaInputPhase::Pressed;
				PC->RouteInputEvent(Warm);
				PC->GetLocalPlayer()->GetSubsystem<UWuwaFormationUiController>()->RequestSwitchTo(Formation->GetCurrentIndex() == 0 ? 1 : 0);
				ReadyAt = -1.0;
				return false;
			}
			// 第三张：特殊能量 3/4（和原作截图对照）、协奏 60%，再按 E、R，在点击圈还亮的时候截（能量条平滑 0.1 秒）
			if (ASC)
			{
				ASC->SetNumericAttributeBase(UWuwaAttributeSet::GetSpecialEnergy1Attribute(), ASC->GetNumericAttribute(UWuwaAttributeSet::GetMaxSpecialEnergy1Attribute()) * 0.75f);
				ASC->SetNumericAttributeBase(UWuwaAttributeSet::GetConcertoEnergyAttribute(), ASC->GetNumericAttribute(UWuwaAttributeSet::GetMaxConcertoEnergyAttribute()) * 0.6f);
			}
			for (const TCHAR* Tag : {TEXT("Input.Combat.Skill"), TEXT("Input.Combat.Ultimate")})
			{
				FWuwaInputEvent Input;
				Input.InputTag = FGameplayTag::RequestGameplayTag(Tag);
				Input.RouteTag = FGameplayTag::RequestGameplayTag(TEXT("Input.Route.Ability"));
				Input.Phase = EWuwaInputPhase::Pressed;
				PC->RouteInputEvent(Input);
			}
			ReadyAt = Now - 3.0 + 0.12;
			return false;
		}

	private:
		FAutomationTestBase* Test;
		FString Directory;
		double StartedAt = -1.0;
		double ReadyAt = -1.0;
		int32 Shot = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHudInGameScreenshotTest, "UIVisual.HudInGame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHudInGameScreenshotTest::RunTest(const FString& Parameters)
{
	if (GUsingNullRHI)
	{
		AddWarning(TEXT("Needs a real RHI to take screenshots; skipped under NullRHI."));
		return true;
	}
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Diagnostics/HudScreenshots"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString Map = Parameters.IsEmpty() ? TEXT("/Game/Environment/DaKa/Maps/L_DaKa") : Parameters;
	if (!AutomationOpenMap(Map))
	{
		AddError(FString::Printf(TEXT("Could not open %s."), *Map));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(HudInGameScreenshots::FCaptureInGame(this, Directory));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}


// 排查用：LGUI 测试地图里在屏幕中间放一个 UWuwaUINiagara（放大 5 倍）播 NS_UI_FightSkill，截图并打印画布网格的顶点 / 材质。
namespace UINiagaraProbe
{
	class FProbe final : public IAutomationLatentCommand
	{
	public:
		FProbe(FAutomationTestBase* InTest, FString InPath) : Test(InTest), Path(MoveTemp(InPath)) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			if (Now - StartedAt > 120.0) { Test->AddError(TEXT("Probe timed out.")); return true; }
			UWorld* World = HudVisualScreenshots::FindPlayWorld();
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			UWuwaUiManager* Manager = Player ? UWuwaUiManager::Get(Player) : nullptr;
			UUIItem* HudLayer = Manager ? Manager->GetLayerItem(EWuwaUiLayer::HUD) : nullptr;
			if (!HudLayer) return false;
			if (!Effect.IsValid())
			{
				if (Manager->IsViewOpen(TEXT("TestNormalView"))) Manager->CloseView(TEXT("TestNormalView"));
				AWuwaUINiagaraActor* Actor = World->SpawnActor<AWuwaUINiagaraActor>();
				Actor->GetUIItem()->AttachToComponent(HudLayer, FAttachmentTransformRules::KeepRelativeTransform);
				FUIAnchorData Anchor;
				Anchor.AnchorMin = Anchor.AnchorMax = FVector2D(0.5f, 0.5f);
				Actor->GetUIItem()->SetAnchorData(Anchor);
				UWuwaUINiagara* Niagara = Actor->GetUINiagara();
				Niagara->SetNiagaraSystem(LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/LGUI/HUD/Kuro/Effects/NS_UI_FightSkill.NS_UI_FightSkill")));
				Niagara->SetNiagaraScale(5.f);
				Niagara->Play();
				Effect = Niagara;
				return false;
			}
			if (Effect->IsWaitingForCompile()) return false;
			if (ReadyAt < 0.0) ReadyAt = Now;
			// -ProbeDelay=N：系统编译好后再等 N 秒才播（给材质的 LGUI 着色器异步编译留时间）
			if (Now - ReadyAt < Delay) return false;
			if (PlayedAt < 0.0)
			{
				Effect->Play();   // 编译完后从头播一次
				PlayedAt = Now;
				return false;
			}
			if (Now - PlayedAt < 0.1) return false;
			const UWuwaUINiagaraRenderer* Renderer = Effect->GetRenderers().Num() > 0 ? Effect->GetRenderers()[0].Get() : nullptr;
			Test->AddInfo(FString::Printf(TEXT("Probe: playing %d simulated %d rendered %d | %s"), Effect->IsPlaying(), Effect->GetSimulatedParticleCount(),
				Effect->GetRenderedParticleCount(), Renderer ? *Renderer->DescribeMesh(0) : TEXT("no renderer")));
			Test->TestTrue(TEXT("saved probe screenshot"), HudVisualScreenshots::SavePlayViewport(World, Path));
			return true;
		}

	private:
		FAutomationTestBase* Test;
		FString Path;
		double StartedAt = -1.0;
		double PlayedAt = -1.0;
		TWeakObjectPtr<UWuwaUINiagara> Effect;
		double ReadyAt = -1.0;
	public:
		float Delay = 0.f;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUINiagaraProbeTest, "UIVisual.UINiagaraProbe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUINiagaraProbeTest::RunTest(const FString& Parameters)
{
	if (GUsingNullRHI)
	{
		AddWarning(TEXT("Needs a real RHI; skipped under NullRHI."));
		return true;
	}
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Diagnostics/HudScreenshots"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	if (!AutomationOpenMap(TEXT("/Game/Map/LGUI"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	TSharedRef<IAutomationLatentCommand> Probe = MakeShared<UINiagaraProbe::FProbe>(this, Directory / TEXT("UINiagaraProbe.png"));
	FParse::Value(FCommandLine::Get(), TEXT("ProbeDelay="), StaticCastSharedRef<UINiagaraProbe::FProbe>(Probe)->Delay);
	FAutomationTestFramework::Get().EnqueueLatentCommand(Probe);
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif

