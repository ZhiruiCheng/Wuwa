#include "Game/Animation/Notifies/WuwaAnimNotify_SpawnBullet.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "Game/NewWorld/Bullet/WuwaBulletSubsystem.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"

FString UWuwaAnimNotify_SpawnBullet::GetNotifyName_Implementation() const
{
	if (BulletIds.Num() == 0) return TEXT("Spawn Bullet");
	if (BulletIds.Num() == 1) return FString::Printf(TEXT("Bullet %s"), *BulletIds[0].ToString());
	return FString::Printf(TEXT("Bullet %s +%d"), *BulletIds[0].ToString(), BulletIds.Num() - 1);
}

void UWuwaAnimNotify_SpawnBullet::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	AActor* Attacker = MeshComp ? MeshComp->GetOwner() : nullptr;
	// 动画编辑器预览没有子弹系统，什么也不做
	UWuwaBulletSubsystem* Bullets = Attacker ? UWuwaBulletSubsystem::Get(Attacker) : nullptr;
	if (!Bullets) return;

	// 原作 SkillBehavior 条件组：全部满足才生成
	if (SkillTargetCondition != EWuwaBulletSkillTargetCondition::Any)
	{
		const AWuwaCharacter* Character = Cast<AWuwaCharacter>(Attacker);
		const UWuwaSkillBridgeComponent* Skills = Character ? Character->SkillComponent.Get() : nullptr;
		const bool bHasTarget = Skills && Skills->GetSkillTarget() != nullptr;
		if (bHasTarget != (SkillTargetCondition == EWuwaBulletSkillTargetCondition::HasTarget)) return;
	}
	if (!RequiredOwnerTags.IsEmpty() || !BlockedOwnerTags.IsEmpty())
	{
		const UAbilitySystemComponent* OwnerASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Attacker);
		if (!OwnerASC || !OwnerASC->HasAllMatchingGameplayTags(RequiredOwnerTags) || OwnerASC->HasAnyMatchingGameplayTags(BlockedOwnerTags)) return;
	}

	FWuwaBulletSpawnRequest Request;
	Request.Attacker = Attacker;
	Request.SkillId = ResolveSkillId(Attacker);
	Request.SourceAnimation = Animation;
	for (const FName& BulletId : BulletIds)
	{
		if (BulletId.IsNone()) continue;
		Request.BulletId = BulletId;
		Bullets->SpawnBullet(Request);
	}
}

int32 UWuwaAnimNotify_SpawnBullet::ResolveSkillId(const AActor* Attacker)
{
	// 原作取"当前蒙太奇对应的技能 ID"；这里是 GAS 记录的、正在播当前蒙太奇的 GA
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Attacker);
	const UWuwaGameplayAbilityBase* Ability = ASC ? Cast<UWuwaGameplayAbilityBase>(ASC->GetAnimatingAbility()) : nullptr;
	return Ability ? Ability->GetSkillId() : 0;
}
