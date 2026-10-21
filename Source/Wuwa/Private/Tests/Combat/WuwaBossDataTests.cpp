#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Engine/DataTable.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"

// 芙露德莉丝 Boss 的数据（WuwaBossSetup 按 ArtSource/Boss/fuludelisi.boss.json 生成）：技能表、蒙太奇、根运动。
namespace WuwaBossDataTests
{
	const TCHAR* SkillTable = TEXT("/Game/Characters/Monster/Boss/Fuludelisi/Data/DT_SkillInfo_Fuludelisi.DT_SkillInfo_Fuludelisi");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaBossFuludelisiSkillsTest, "Wuwa.Combat.Boss.Fuludelisi.SkillsAndMontages",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaBossFuludelisiSkillsTest::RunTest(const FString& Parameters)
{
	using namespace WuwaBossDataTests;
	UDataTable* Table = LoadObject<UDataTable>(nullptr, SkillTable);
	if (!TestNotNull(TEXT("Skill table loads"), Table)) return false;
	if (!TestTrue(TEXT("Row struct is FWuwaSkillInfo"), Table->GetRowStruct() == FWuwaSkillInfo::StaticStruct())) return false;
	// 第一步：潮汐 / 流云 / 急涌 / 冲霄 + 入战 / 出生，行名沿用原作技能 ID
	for (const TCHAR* Id : {TEXT("80025001"), TEXT("80025002"), TEXT("80025003"), TEXT("80025004"), TEXT("80025025"), TEXT("80025026")})
	{
		const FWuwaSkillInfo* Row = Table->FindRow<FWuwaSkillInfo>(Id, TEXT("BossDataTest"));
		if (!TestNotNull(*FString::Printf(TEXT("Skill %s exists"), Id), Row)) continue;
		TestTrue(*FString::Printf(TEXT("%s is a skill ID"), Id), WuwaSkill::ParseSkillId(Id) > 0);
		if (!TestEqual(*FString::Printf(TEXT("Skill %s has one montage"), Id), Row->Montages.Num(), 1)) continue;
		UAnimMontage* Montage = Row->Montages[0].LoadSynchronous();
		if (!TestNotNull(*FString::Printf(TEXT("Skill %s montage loads"), Id), Montage)) continue;
		TestTrue(*FString::Printf(TEXT("%s has segments"), *Montage->GetName()), Montage->SlotAnimTracks.Num() == 1 && Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() > 0);
		for (const FAnimSegment& Segment : Montage->SlotAnimTracks[0].AnimTrack.AnimSegments)
		{
			const UAnimSequence* Sequence = Cast<UAnimSequence>(Segment.GetAnimReference());
			// 原作这些动画都开了根运动（攻击的前冲、位移靠它）
			TestTrue(*FString::Printf(TEXT("%s uses root motion"), *GetNameSafe(Sequence)), Sequence && Sequence->bEnableRootMotion);
		}
	}
	// 冲霄 = 潮汐开头 + 0.167 秒过渡（1.5 倍速）+ 急涌结尾，总长和原作一致
	const FWuwaSkillInfo* Rise = Table->FindRow<FWuwaSkillInfo>(TEXT("80025004"), TEXT("BossDataTest"));
	UAnimMontage* Montage = Rise && Rise->Montages.Num() ? Rise->Montages[0].LoadSynchronous() : nullptr;
	if (TestNotNull(TEXT("Rise montage"), Montage))
	{
		const TArray<FAnimSegment>& Segments = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments;
		if (TestEqual(TEXT("Three segments"), Segments.Num(), 3))
		{
			TestEqual(TEXT("Middle segment plays at 1.5x"), Segments[1].AnimPlayRate, 1.5f);
			TestEqual(TEXT("Last segment starts at the rush tail"), Segments[2].AnimStartTime, 3.46f, 0.001f);
		}
		TestEqual(TEXT("Length matches the original"), Montage->GetPlayLength(), 4.3078f, 0.01f);
	}
	return true;
}

#endif
