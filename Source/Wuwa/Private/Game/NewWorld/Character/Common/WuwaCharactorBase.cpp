// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"
#include "Engine/DataTable.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"

#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Framework/WuwaPlayerState.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaCombatAttributeSet.h"
#include "Game/NewWorld/Combat/WuwaHitComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Combat/WuwaCamp.h"
#include "Game/NewWorld/Combat/WuwaCreatureSubsystem.h"
#include "Game/UI/WuwaHUD.h"
#include "Game/UI/WuwaWidgetController.h"

class AWuwaPlayerController;
class AWuwaPlayerState;
class UWuwaGameplayAbilityBase;
// Sets default values
AWuwaCharactorBase::AWuwaCharactorBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	//初始化
	AbilitySystemComponent = CreateDefaultSubobject<UWuwaAbilitySystemComponent>(TEXT("WuwaAbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);//设置网络同步
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	AttributeSet = CreateDefaultSubobject<UWuwaAttributeSet>(TEXT("WuwaAttributeSet"));
	// 伤害公式用的加成 / 抗性；和 AttributeSet 一样是角色的子对象，ASC 初始化时自动收进来
	CombatAttributeSet = CreateDefaultSubobject<UWuwaCombatAttributeSet>(TEXT("WuwaCombatAttributeSet"));
	HitComponent = CreateDefaultSubobject<UWuwaHitComponent>(TEXT("WuwaHitComponent"));
	WuwaMovementComponent = Cast<UWuwaMovementComponent>(GetCharacterMovement());//重置MovementComponent组件
	
	//设Pawn默认的频率就是100所以不需要删除
	//SetNetUpdateFrequency(100.f);
}

void AWuwaCharactorBase::BeginPlay()
{
	Super::BeginPlay();
	AbilitySystemComponent->InitAbilityActorInfo(this, this);   // 每台机器都要做
	AbilitySystemComponent->InitAbilitySystemCompoent();
	InitInitialAbilities();
	// 怪物等单位的属性初始值（服务器写，属性复制给客户端）；Health 跟着 MaxHealth 写满
	if (HasAuthority())
	{
		for (const FWuwaInitialAttribute& Initial : InitialAttributes)
		{
			if (Initial.Attribute.IsValid() && AbilitySystemComponent->HasAttributeSetForAttribute(Initial.Attribute))
			{
				AbilitySystemComponent->SetNumericAttributeBase(Initial.Attribute, Initial.Value);
			}
		}
		if (InitialAttributes.ContainsByPredicate([](const FWuwaInitialAttribute& I) { return I.Attribute == UWuwaAttributeSet::GetMaxHealthAttribute(); }))
		{
			AbilitySystemComponent->SetNumericAttributeBase(UWuwaAttributeSet::GetHealthAttribute(),
				AbilitySystemComponent->GetNumericAttributeBase(UWuwaAttributeSet::GetMaxHealthAttribute()));
		}
	}
	// 登记到本世界的战斗单位表（锁定、AI 从这里找周围的单位）
	if (UWuwaCreatureSubsystem* Creatures = UWuwaCreatureSubsystem::Get(this)) Creatures->Register(this);
}

void AWuwaCharactorBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWuwaCreatureSubsystem* Creatures = UWuwaCreatureSubsystem::Get(this)) Creatures->Unregister(this);
	Super::EndPlay(EndPlayReason);
}


#pragma region Replicate
//服务端侧
void AWuwaCharactorBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	RefreshAbilityActorInfo();
	// 旧 UMG 血条每次调用都会新建一层 Widget，切人会叠加；由 LGUI 的 HUD 替代后不再在这里初始化。
}

void AWuwaCharactorBase::UnPossessed()
{
	Super::UnPossessed();
	RefreshAbilityActorInfo();
}

//客户端侧面
void AWuwaCharactorBase::OnRep_Controller()
{
	Super::OnRep_Controller();
	RefreshAbilityActorInfo();
	// 同上：不再初始化旧 UMG 血条。
}

void AWuwaCharactorBase::InitHUD()
{
	if (AWuwaPlayerController* PC = Cast<AWuwaPlayerController>(GetController()))
	{
		if (AWuwaHUD* HUD = Cast<AWuwaHUD>(PC->GetHUD()))
		{
			HUD->InitCtrAndWidget(AbilitySystemComponent, AttributeSet, PC);
			HUD->WuwaWidgetController->BroadInitialValues();
			HUD->WuwaWidgetController->BindCallBackDependencies();
		}
	}
}

#pragma endregion


#pragma region ASC

//在服务端初始化
void AWuwaCharactorBase::InitInitialAbilities()
{
	if (!HasAuthority()) return;
    
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();

	if (!ASC)
	{
		return;
	}
    
	// 技能：按技能表授予，行名就是技能 ID（技能的身份）
	if (SkillTable && AbilitySystemComponent)
	{
		SkillTable->ForeachRow<FWuwaSkillInfo>(TEXT("InitInitialAbilities"), [this](const FName& RowName, const FWuwaSkillInfo& Info)
		{
			const int32 SkillId = WuwaSkill::ParseSkillId(RowName);
			if (!AbilitySystemComponent->GiveSkill(SkillId, Info, this).IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("%s: skill row %s in %s was not granted (row name must be a positive skill ID, Ability must be set, IDs unique)."),
					*GetName(), *RowName.ToString(), *SkillTable->GetName());
			}
		});
	}

	// 其他非技能能力：没有技能 ID
	for (const TSubclassOf<UGameplayAbility> Ability : CharacterAbilities)
	{
		if (Ability) ASC->GiveAbility(FGameplayAbilitySpec(Ability, 1, INDEX_NONE, this));
	}
}

void AWuwaCharactorBase::RefreshAbilityActorInfo() const
{
	//刷新防止更新的时候拿不到controller
	if (!AbilitySystemComponent) return;
	AbilitySystemComponent->RefreshAbilityActorInfo();
}

TArray<FName> AWuwaCharactorBase::GetCampOptions()
{
	return UWuwaCampLibrary::GetCampNames();
}

UAbilitySystemComponent* AWuwaCharactorBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
#pragma endregion

