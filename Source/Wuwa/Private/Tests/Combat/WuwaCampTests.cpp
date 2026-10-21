#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/World.h"
#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"
#include "Game/NewWorld/Combat/WuwaCamp.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCampRelationTest, "Wuwa.Combat.Camp.Relations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCampRelationTest::RunTest(const FString& Parameters)
{
	// 项目设置里配的真实阵营表（从原作 DT_Camp 导入）
	const TArray<FName> Camps = UWuwaCampLibrary::GetCampNames();
	if (!TestEqual(TEXT("camp table has the original 18 camps"), Camps.Num(), 18)) return false;
	TestTrue(TEXT("Player and Monster exist"), Camps.Contains(TEXT("Player")) && Camps.Contains(TEXT("Monster")));

	using R = EWuwaCampRelation;
	TestEqual(TEXT("Player -> Monster hostile"), UWuwaCampLibrary::GetRelation(TEXT("Player"), TEXT("Monster")), R::Hostile);
	TestEqual(TEXT("Monster -> Player hostile"), UWuwaCampLibrary::GetRelation(TEXT("Monster"), TEXT("Player")), R::Hostile);
	TestEqual(TEXT("Player -> Player friendly"), UWuwaCampLibrary::GetRelation(TEXT("Player"), TEXT("Player")), R::Friendly);
	TestEqual(TEXT("Monster -> Monster friendly"), UWuwaCampLibrary::GetRelation(TEXT("Monster"), TEXT("Monster")), R::Friendly);
	// 原作表不对称：Common_enemy 那一行对 Common_friend 敌对，反过来友好——查询方向有意义
	TestEqual(TEXT("Common_enemy -> Common_friend hostile"), UWuwaCampLibrary::GetRelation(TEXT("Common_enemy"), TEXT("Common_friend")), R::Hostile);
	TestEqual(TEXT("Common_friend -> Common_enemy friendly"), UWuwaCampLibrary::GetRelation(TEXT("Common_friend"), TEXT("Common_enemy")), R::Friendly);
	TestEqual(TEXT("unknown camp -> neutral"), UWuwaCampLibrary::GetRelation(TEXT("NoSuchCamp"), TEXT("Player")), R::Neutral);
	TestEqual(TEXT("no camp -> neutral"), UWuwaCampLibrary::GetRelation(NAME_None, TEXT("Player")), R::Neutral);

	// Actor：角色默认 Player；改成 Monster 后与玩家互为敌对；不是角色的 Actor 没有阵营
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	AWuwaCharactorBase* Player = World->SpawnActor<AWuwaCharactorBase>();
	AWuwaCharactorBase* Monster = World->SpawnActor<AWuwaCharactorBase>();
	AWuwaCharactorBase* Ally = World->SpawnActor<AWuwaCharactorBase>();
	ASkeletalMeshActor* Prop = World->SpawnActor<ASkeletalMeshActor>();
	if (!TestNotNull(TEXT("actors spawned"), Player) || !Monster || !Ally || !Prop) return false;
	TestEqual(TEXT("character default camp is Player"), UWuwaCampLibrary::GetCamp(Player), FName(TEXT("Player")));
	Monster->Camp = TEXT("Monster");
	TestTrue(TEXT("monster is hostile to the player"), UWuwaCampLibrary::IsHostile(Player, Monster));
	TestTrue(TEXT("player is hostile to the monster"), UWuwaCampLibrary::IsHostile(Monster, Player));
	TestFalse(TEXT("teammate is not hostile"), UWuwaCampLibrary::IsHostile(Player, Ally));
	TestFalse(TEXT("self is not hostile"), UWuwaCampLibrary::IsHostile(Player, Player));
	TestEqual(TEXT("non-character actor has no camp"), UWuwaCampLibrary::GetCamp(Prop), FName(NAME_None));
	TestFalse(TEXT("actor without camp is never hostile"), UWuwaCampLibrary::IsHostile(Player, Prop));
	TestFalse(TEXT("null actor is never hostile"), UWuwaCampLibrary::IsHostile(Player, nullptr));
	return true;
}

#endif
