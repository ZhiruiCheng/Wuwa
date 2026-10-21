#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WuwaCamp.generated.h"

/** 两个阵营之间的关系（数值与原作 CampUtils.GetCampRelationship 一致：锁定、子弹等只认 Hostile）。 */
UENUM(BlueprintType)
enum class EWuwaCampRelation : uint8
{
	/** 表里没配（原作表里只有敌对 / 友好两种值，其它值都按这个算）。 */
	Neutral = 0,
	Friendly = 1,
	Hostile = 2,
};

/**
 * 阵营表的一行，对应原作 DT_Camp（行结构 SCamp）。行名就是阵营名（Player、Monster……）。
 * 原作每行是按行序排的数组（0 = 敌对，1 = 友好）；这里改成按阵营名查，表的行顺序随便调都不影响。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCampRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 本阵营对各阵营的关系；没列出的阵营按 Neutral。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camp")
	TMap<FName, EWuwaCampRelation> Relations;
};

/** 阵营查询（≈ 原作 CampUtils + FightLibrary.Init 读表）。 */
UCLASS()
class WUWA_API UWuwaCampLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** From 阵营那一行里对 To 的关系（原作 CampUtils.Camp[From][To]）；任一阵营不在表里时 Neutral。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Camp")
	static EWuwaCampRelation GetRelation(FName FromCamp, FName ToCamp);

	/** Actor 的阵营：AWuwaCharactorBase 的 Camp；其它 Actor（场景物件、没改成角色的 Boss 网格）没有阵营。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Camp")
	static FName GetCamp(const AActor* Actor);

	/**
	 * Target 对 Viewer 是不是敌对（原作锁定组件的写法：GetCampRelationship(目标阵营, 自己阵营) == 2）。
	 * 任一方没有阵营时不算敌对。
	 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Camp")
	static bool IsHostile(const AActor* Viewer, const AActor* Target);

	/** 表里所有阵营名（编辑器下拉用）。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Combat|Camp")
	static TArray<FName> GetCampNames();

private:
	static const UDataTable* GetCampTable();
};
