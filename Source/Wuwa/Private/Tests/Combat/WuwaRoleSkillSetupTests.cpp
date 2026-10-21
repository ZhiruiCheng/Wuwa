#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_ChangeSkillPriority.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Formation/WuwaFormationComponent.h"
#include "Game/Input/WuwaInputRouterComponent.h"
#include "Game/Input/WuwaInputTypes.h"
#include "Game/Common/WuwaGameTags.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaSkillMontageAbility.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

namespace WuwaRoleSkillSetupTest
{
	const TCHAR* ChangliTable = TEXT("/Game/Characters/Role/changli/Data/DT_SkillInfo_Changli.DT_SkillInfo_Changli");

	struct FExpected { int32 Id; const TCHAR* Montage; float AcceptInput; float ReadyEnd; bool bPriority; };
	// 原作 AM_AirAttack0N：NextAtt 开始、EndSkill 时间；空中普攻 4 在 0.75 秒把打断等级改成 2
	const FExpected AirAttacks[] = {
		{ 1205101, TEXT("AM_AirAttack01"), 0.363f, 1.247f, false },
		{ 1205102, TEXT("AM_AirAttack02"), 0.602f, 1.585f, false },
		{ 1205103, TEXT("AM_AirAttack03"), 0.716f, 1.681f, false },
		{ 1205104, TEXT("AM_AirAttack04"), 0.750f, 1.923f, true },
	};

	UWorld* FindPlayWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) return Context.World();
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRoleSkillSetupAssetsTest, "Wuwa.Combat.RoleSkills.ChangliAirAttackAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRoleSkillSetupAssetsTest::RunTest(const FString& Parameters)
{
	using namespace WuwaRoleSkillSetupTest;
	const UDataTable* Table = LoadObject<UDataTable>(nullptr, ChangliTable);
	if (!TestNotNull(TEXT("Changli skill table"), Table)) return false;
	for (const FExpected& Expected : AirAttacks)
	{
		const FString Id = FString::FromInt(Expected.Id);
		const FWuwaSkillInfo* Row = Table->FindRow<FWuwaSkillInfo>(*Id, TEXT("Test"), false);
		if (!TestNotNull(*FString::Printf(TEXT("%s row"), *Id), Row)) continue;
		TestTrue(*FString::Printf(TEXT("%s is a normal attack of Changli"), *Id), Row->SkillTags.HasTagExact(FWuwaGameTags::Get().Skill_Type_NormalAttack)
			&& Row->SkillTags.HasTagExact(FWuwaGameTags::Get().Skill_Role_Changli));
		TestFalse(*FString::Printf(TEXT("%s no ledge protection (original WalkOffLedge false)"), *Id), Row->bWalkOffLedge);
		const UWuwaSkillMontageAbility* Ability = Row->Ability ? Cast<UWuwaSkillMontageAbility>(Row->Ability->GetDefaultObject()) : nullptr;
		if (!TestNotNull(*FString::Printf(TEXT("%s GA derives from WuwaSkillMontageAbility"), *Id), Ability)) continue;
		TestEqual(*FString::Printf(TEXT("%s interrupt level (original 2)"), *Id), Ability->InterruptLevel, 2);
		const UAnimMontage* Montage = Ability->SkillMontages.Num() > 0 ? Ability->SkillMontages[0].Get() : nullptr;
		if (!TestNotNull(*FString::Printf(TEXT("%s montage"), *Id), Montage)) continue;
		TestEqual(*FString::Printf(TEXT("%s montage name"), *Id), Montage->GetName(), FString(Expected.Montage));
		TestEqual(*FString::Printf(TEXT("%s table montage matches GA"), *Id), Row->Montages.Num() > 0 ? Row->Montages[0].ToSoftObjectPath() : FSoftObjectPath(), FSoftObjectPath(Montage));
		float ReadyEnd = -1.f, Accept = -1.f, CancelStart = -1.f;
		bool bPriority = false;
		for (const FAnimNotifyEvent& Event : Montage->Notifies)
		{
			const FString Class = Event.Notify ? Event.Notify->GetClass()->GetName() : Event.NotifyStateClass ? Event.NotifyStateClass->GetClass()->GetName() : FString();
			if (Class.Contains(TEXT("SkillReadyEnd"))) ReadyEnd = Event.GetTime();
			if (Class.Contains(TEXT("SkillAcceptInput"))) Accept = Event.GetTime();
			if (Class.Contains(TEXT("MovementCancelWindow"))) CancelStart = Event.GetTime();
			bPriority |= Cast<UWuwaAnimNotify_ChangeSkillPriority>(Event.Notify) != nullptr;
		}
		TestTrue(*FString::Printf(TEXT("%s ReadyEnd at original EndSkill %.3f (got %.3f)"), *Id, Expected.ReadyEnd, ReadyEnd), FMath::IsNearlyEqual(ReadyEnd, Expected.ReadyEnd, .02f));
		TestTrue(*FString::Printf(TEXT("%s accept-input window at original NextAtt %.3f (got %.3f)"), *Id, Expected.AcceptInput, Accept), FMath::IsNearlyEqual(Accept, Expected.AcceptInput, .02f));
		TestTrue(*FString::Printf(TEXT("%s movement cancel from ReadyEnd"), *Id), FMath::IsNearlyEqual(CancelStart, ReadyEnd, .001f));
		TestEqual(*FString::Printf(TEXT("%s change-priority notify"), *Id), bPriority, Expected.bPriority);
		const FAnimSegment* Segment = Montage->SlotAnimTracks.Num() ? &Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0] : nullptr;
		const UAnimSequence* Sequence = Segment ? Cast<UAnimSequence>(Segment->GetAnimReference()) : nullptr;
		TestTrue(*FString::Printf(TEXT("%s anim root motion on (original)"), *Id), Sequence && Sequence->bEnableRootMotion);
	}
	return true;
}

namespace WuwaRoleSkillSetupTest
{
	/**
	 * 大卡：切到长离 → 起跳，过最高点后直接放空中普攻 1（绕过输入规则，空中链的规则下一阶段加）：
	 * 蒙太奇 AM_AirAttack01 在播，竖直方向跟着动画根运动（原作根骨 Z 只降 ~94 cm），不会按重力一路往下掉；技能播完自己结束。
	 */
	class FAirAttack final : public IAutomationLatentCommand
	{
	public:
		explicit FAirAttack(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			if (Now - StartedAt > 300.0) { Test->AddError(FString::Printf(TEXT("Timed out at step %d."), Step)); return true; }
			UWorld* World = FindPlayWorld();
			AWuwaPlayerController* PC = World ? Cast<AWuwaPlayerController>(World->GetFirstPlayerController()) : nullptr;
			UWuwaFormationComponent* Formation = PC ? PC->FormationComponent.Get() : nullptr;
			if (!Formation || !Formation->IsFormationReady()) return false;
			int32 ChangliIndex = INDEX_NONE;
			for (int32 Index = 0; Index < Formation->GetMembers().Num(); ++Index)
				if (Formation->GetMembers()[Index] && Formation->GetMembers()[Index]->GetClass()->GetName().Contains(TEXT("Changli"))) ChangliIndex = Index;
			if (ChangliIndex == INDEX_NONE) return false;
			AWuwaCharacter* Changli = Formation->GetMembers()[ChangliIndex];
			const double GameNow = World->GetTimeSeconds();
			UWuwaMovementComponent* Movement = Changli->GetWuwaMovementComponent();
			switch (Step)
			{
			case 0:
				if (Formation->GetCurrentIndex() != ChangliIndex) Formation->RequestSwitch(ChangliIndex);
				Step = 1; StepAt = GameNow;
				return false;
			case 1:
			{
				if (GameNow - StepAt < 1.0 || PC->GetPawn() != Changli) return false;
				if (!bPlaced)
				{
					// 不依赖 PlayerStart（可能摆在高台上）：放到大厅地面上、Boss 前方 8 米，站稳再跳。
					for (TActorIterator<AWuwaCharactorBase> It(World); It; ++It)
						if (It->GetClass()->GetName().Contains(TEXT("BP_Monster_Fuludelisi")))
						{
							const FVector Forward = It->GetActorForwardVector();
							Changli->SetActorLocation(It->GetActorLocation() + Forward * 800.0, false, nullptr, ETeleportType::TeleportPhysics);
							Changli->SetActorRotation(FRotator(0.0, (-Forward).Rotation().Yaw, 0.0));
						}
					bPlaced = true;
					StepAt = GameNow;
					return false;
				}
				if (GameNow - StepAt < 1.0 || !Movement->IsMovingOnGround()) return false;
				FWuwaInputEvent Event;
				Event.InputTag = FWuwaGameTags::Get().Player_Common_Movement_Jump;
				Event.RouteTag = FWuwaGameTags::Get().Input_Route_Movement;
				Event.Phase = EWuwaInputPhase::Pressed;
				Event.Timestamp = GameNow;
				PC->GetInputRouter()->DispatchInput(Event);
				if (!Movement->IsJumpRootMotionActive())
				{
					const FWuwaSkillData Skill = Changli->SkillComponent->GetCurrentSkillData();
					Test->AddInfo(FString::Printf(TEXT("jump refused: ground %d, skill %d, montage %s"), Movement->IsMovingOnGround(), Skill.SkillId,
						*GetNameSafe(Changli->GetMesh()->GetAnimInstance()->GetCurrentActiveMontage())));
					if (GameNow - StepAt < 3.0) return false;   // 重试到 3 秒
				}
				Test->TestTrue(TEXT("Changli jumped"), Movement->IsJumpRootMotionActive());
				Step = 2; StepAt = GameNow;
				return false;
			}
			case 2:
			{
				if (GameNow - StepAt < 0.36) return false;   // 过 Jump_Walk 的最高点（0.35 秒）
				UWuwaAbilitySystemComponent* ASC = Cast<UWuwaAbilitySystemComponent>(Changli->GetAbilitySystemComponent());
				const TArray<FGameplayAbilitySpecHandle> Handles = ASC ? ASC->FindSkillHandles(1205101) : TArray<FGameplayAbilitySpecHandle>();
				if (!Test->TestEqual(TEXT("air attack 1 granted"), Handles.Num(), 1)) return true;
				StartZ = Changli->GetActorLocation().Z;
				Test->TestTrue(TEXT("air attack 1 activates in the air"), ASC->TryActivateAbility(Handles[0]));
				UAnimInstance* Anim = Changli->GetMesh()->GetAnimInstance();
				Test->TestEqual(TEXT("AM_AirAttack01 playing"), GetNameSafe(Anim ? Anim->GetCurrentActiveMontage() : nullptr), FString(TEXT("AM_AirAttack01")));
				Step = 3; StepAt = GameNow;
				return false;
			}
			case 3:
			{
				// 原作 AirAttack01 根骨 Z：先 -11，回升到 +16，结尾降到 -94；EndSkill 在 1.247 秒
				if (Movement->IsFalling() && GameNow - StepAt <= 1.2) MinZ = FMath::Min(MinZ, float(Changli->GetActorLocation().Z - StartZ));
				const FWuwaSkillData Skill = Changli->SkillComponent->GetCurrentSkillData();
				if (GameNow - StepAt < 1.2) return false;
				Test->AddInfo(FString::Printf(TEXT("air attack 1: lowest %.0f cm below the start within 1.2 s, skill %d readyEnd %d"), -MinZ, Skill.SkillId, Skill.bMainSkillReadyEnd));
				Test->TestTrue(TEXT("vertical motion follows the anim root motion (free fall would drop > 600 cm)"), MinZ > -150.f);
				Step = 4; StepAt = GameNow;
				return false;
			}
			case 4:
			{
				const FWuwaSkillData Skill = Changli->SkillComponent->GetCurrentSkillData();
				if (IsValid(Skill.ActiveAbility) && GameNow - StepAt < 3.0) return false;
				Test->TestFalse(TEXT("air attack 1 ended by itself"), IsValid(Skill.ActiveAbility));
				return true;
			}
			default:
				return true;
			}
		}

	private:
		FAutomationTestBase* Test;
		double StartedAt = -1.0;
		double StepAt = 0.0;
		float StartZ = 0.f;
		float MinZ = 0.f;
		bool bPlaced = false;
		int32 Step = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRoleSkillAirAttackTest, "Wuwa.Combat.RoleSkills.ChangliAirAttackPlays",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRoleSkillAirAttackTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Environment/DaKa/Maps/L_DaKa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(WuwaRoleSkillSetupTest::FAirAttack(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
