#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Core/Tag/WuwaGameTags.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Formation/WuwaFormationComponent.h"
#include "Game/Input/WuwaInputRouterComponent.h"
#include "Game/Input/WuwaInputTypes.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "ShaderCompiler.h"
#include "ProfilingDebugging/MiscTrace.h"

// 只用于排查（不随常规测试套件运行）：第一次放技能的卡顿。按两次普攻（中间等连段重置），记录每次之后 2 秒内最长的一帧和着色器编译数量。
namespace WuwaEffectFirstCastDiag
{
	UWorld* FindPlayWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) return Context.World();
		}
		return nullptr;
	}

	class FMeasure final : public IAutomationLatentCommand
	{
	public:
		explicit FMeasure(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			if (Now - StartedAt > 300.0) { Test->AddError(TEXT("Timed out.")); return true; }
			UWorld* World = FindPlayWorld();
			AWuwaPlayerController* PC = World ? Cast<AWuwaPlayerController>(World->GetFirstPlayerController()) : nullptr;
			UWuwaFormationComponent* Formation = PC ? PC->FormationComponent.Get() : nullptr;
			if (!Formation || !Formation->IsFormationReady()) return false;
			if (ReadyAt < 0.0) ReadyAt = Now;
			if (Now - ReadyAt < 5.0) { LastFrame = Now; return false; }   // 进场后的加载先过去
			const double Frame = Now - LastFrame;
			LastFrame = Now;
			if (CastIndex >= 0) MaxFrame = FMath::Max(MaxFrame, Frame);
			if (CastIndex >= 0 && Now - CastAt < 2.0) return false;
			if (CastIndex >= 0)
			{
				const int32 Jobs = GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
				Test->AddInfo(FString::Printf(TEXT("cast %d (%s): longest frame %.0f ms, shader jobs remaining %d"), CastIndex + 1,
					*GetNameSafe(PC->GetPawn()), MaxFrame * 1000.0, Jobs));
				UE_LOG(LogTemp, Display, TEXT("[EffectFirstCast] cast %d longest frame %.0f ms"), CastIndex + 1, MaxFrame * 1000.0);
				if (!bRegionEnded)
				{
					// 用 -trace=cpu 跑时可以按区域导出这 2 秒的计时（UnrealInsights TimingInsights.ExportTimingEvents -region=FirstCast1）
					if (CastIndex == 0) { TRACE_END_REGION(TEXT("FirstCast1")); } else { TRACE_END_REGION(TEXT("FirstCast2")); }
					bRegionEnded = true;
				}
				if (CastIndex == 1) return true;
				if (Now - CastAt < 4.0) return false;   // 等连段重置
			}
			++CastIndex;
			MaxFrame = 0.0;
			CastAt = Now;
			UE_LOG(LogTemp, Display, TEXT("[EffectFirstCast] press attack %d"), CastIndex + 1);
			if (CastIndex == 0) { TRACE_BEGIN_REGION(TEXT("FirstCast1")); } else { TRACE_BEGIN_REGION(TEXT("FirstCast2")); }
			bRegionEnded = false;
			FWuwaInputEvent Event;
			Event.InputTag = FWuwaGameTags::Get().Input_Combat_Attack;
			Event.RouteTag = FWuwaGameTags::Get().Input_Route_Ability;
			Event.Phase = EWuwaInputPhase::Pressed;
			Event.Timestamp = World->GetTimeSeconds();
			PC->GetInputRouter()->DispatchInput(Event);
			return false;
		}

	private:
		FAutomationTestBase* Test;
		double StartedAt = -1.0;
		double ReadyAt = -1.0;
		double LastFrame = 0.0;
		double CastAt = 0.0;
		double MaxFrame = 0.0;
		int32 CastIndex = -1;
		bool bRegionEnded = true;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaEffectFirstCastDiag, "Diag.EffectFirstCast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaEffectFirstCastDiag::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Environment/DaKa/Maps/L_DaKa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(WuwaEffectFirstCastDiag::FMeasure(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
