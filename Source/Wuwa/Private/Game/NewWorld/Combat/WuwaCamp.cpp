#include "Game/NewWorld/Combat/WuwaCamp.h"

#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"
#include "Game/NewWorld/Combat/WuwaCombatSettings.h"

EWuwaCampRelation UWuwaCampLibrary::GetRelation(FName FromCamp, FName ToCamp)
{
	const UDataTable* Table = GetCampTable();
	const FWuwaCampRow* Row = Table && !FromCamp.IsNone() ? Table->FindRow<FWuwaCampRow>(FromCamp, TEXT("WuwaCamp"), false) : nullptr;
	const EWuwaCampRelation* Relation = Row && !ToCamp.IsNone() ? Row->Relations.Find(ToCamp) : nullptr;
	return Relation ? *Relation : EWuwaCampRelation::Neutral;
}

FName UWuwaCampLibrary::GetCamp(const AActor* Actor)
{
	const AWuwaCharactorBase* Character = Cast<AWuwaCharactorBase>(Actor);
	return Character ? Character->Camp : NAME_None;
}

bool UWuwaCampLibrary::IsHostile(const AActor* Viewer, const AActor* Target)
{
	return GetRelation(GetCamp(Target), GetCamp(Viewer)) == EWuwaCampRelation::Hostile;
}

TArray<FName> UWuwaCampLibrary::GetCampNames()
{
	const UDataTable* Table = GetCampTable();
	return Table ? Table->GetRowNames() : TArray<FName>();
}

const UDataTable* UWuwaCampLibrary::GetCampTable()
{
	// 表很小（十几行），第一次用到时同步加载；软引用加载后由资产自身常驻
	const UDataTable* Table = GetDefault<UWuwaCombatSettings>()->CampTable.LoadSynchronous();
	return Table && Table->GetRowStruct() == FWuwaCampRow::StaticStruct() ? Table : nullptr;
}
