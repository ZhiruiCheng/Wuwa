#include "Game/Module/Formation/WuwaFormationUiController.h"

#include "AbilitySystemComponent.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Game/Formation/WuwaFormationComponent.h"
#include "Game/LGUI/Core/WuwaUiLog.h"
#include "Game/Module/Formation/WuwaFormationEvents.h"
#include "Game/Module/Formation/WuwaFormationModel.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Subsystems/SubsystemCollection.h"
#include "Algo/AllOf.h"
#include "GameplayEffect.h"
#include "Game/NewWorld/Buff/WuwaBuffLibrary.h"

namespace WuwaFormationUi
{
	/** 血量以外、界面要显示的属性。 */
	const TArray<FGameplayAttribute>& DisplayAttributes()
	{
		using S = UWuwaAttributeSet;
		static const TArray<FGameplayAttribute> List = {
			S::GetLevelAttribute(), S::GetElementTypeAttribute(),
			S::GetEnergyAttribute(), S::GetMaxEnergyAttribute(),
			S::GetConcertoEnergyAttribute(), S::GetMaxConcertoEnergyAttribute(),
			S::GetSpecialEnergy1Attribute(), S::GetMaxSpecialEnergy1Attribute(),
			S::GetSpecialEnergy2Attribute(), S::GetMaxSpecialEnergy2Attribute(),
			S::GetSpecialEnergy3Attribute(), S::GetMaxSpecialEnergy3Attribute(),
			S::GetSpecialEnergy4Attribute(), S::GetMaxSpecialEnergy4Attribute(),
			S::GetSpecialEnergy5Attribute(), S::GetMaxSpecialEnergy5Attribute(),
		};
		return List;
	}

	void ReadAttributes(const UAbilitySystemComponent* ASC, FWuwaTeamMemberUiData& Data)
	{
		using S = UWuwaAttributeSet;
		Data.Level = FMath::RoundToInt(ASC->GetNumericAttribute(S::GetLevelAttribute()));
		Data.Element = WuwaElement::FromAttributeValue(ASC->GetNumericAttribute(S::GetElementTypeAttribute()));
		Data.Energy = ASC->GetNumericAttribute(S::GetEnergyAttribute());
		Data.MaxEnergy = ASC->GetNumericAttribute(S::GetMaxEnergyAttribute());
		Data.ConcertoEnergy = ASC->GetNumericAttribute(S::GetConcertoEnergyAttribute());
		Data.MaxConcertoEnergy = ASC->GetNumericAttribute(S::GetMaxConcertoEnergyAttribute());
		const FGameplayAttribute Special[] = {S::GetSpecialEnergy1Attribute(), S::GetSpecialEnergy2Attribute(), S::GetSpecialEnergy3Attribute(),
			S::GetSpecialEnergy4Attribute(), S::GetSpecialEnergy5Attribute()};
		const FGameplayAttribute MaxSpecial[] = {S::GetMaxSpecialEnergy1Attribute(), S::GetMaxSpecialEnergy2Attribute(), S::GetMaxSpecialEnergy3Attribute(),
			S::GetMaxSpecialEnergy4Attribute(), S::GetMaxSpecialEnergy5Attribute()};
		Data.SpecialEnergy.SetNum(UE_ARRAY_COUNT(Special));
		Data.MaxSpecialEnergy.SetNum(UE_ARRAY_COUNT(MaxSpecial));
		for (int32 I = 0; I < UE_ARRAY_COUNT(Special); ++I)
		{
			Data.SpecialEnergy[I] = ASC->GetNumericAttribute(Special[I]);
			Data.MaxSpecialEnergy[I] = ASC->GetNumericAttribute(MaxSpecial[I]);
		}
	}
}

void UWuwaFormationUiController::Initialize(FSubsystemCollectionBase& Collection)
{
	//因为Controller要读取Model所以要先让他们初始化完成
	Collection.InitializeDependency<UWuwaFormationModel>();
	Super::Initialize(Collection);
}

void UWuwaFormationUiController::OnInit()
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}
	// 本地玩家先于 PC 存在：PC 生成并和本地玩家关联时（ULocalPlayer::ReceivedPlayerController）再绑队伍组件。
	PlayerControllerChangedHandle = LocalPlayer->OnPlayerControllerChanged().AddUObject(this, &UWuwaFormationUiController::HandlePlayerControllerChanged);
	if (APlayerController* Existing = LocalPlayer->GetPlayerController(nullptr))//查询当前PC关联的Localplayer如果绑定监听时就已经存在了
	{
		HandlePlayerControllerChanged(Existing);
	}
}

void UWuwaFormationUiController::OnClear()
{
	UnbindFormation();
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		LocalPlayer->OnPlayerControllerChanged().Remove(PlayerControllerChangedHandle);
	}
	PlayerControllerChangedHandle.Reset();
}

void UWuwaFormationUiController::OnLeaveLevel()
{
	// 队伍组件和队员都属于正在清理的世界；新地图的 PC 出现时会经 OnPlayerControllerChanged 重新绑定。
	UnbindFormation();
}

void UWuwaFormationUiController::RequestSwitchTo(int32 Index)
{
	if (UWuwaFormationComponent* Current = Formation.Get())
	{
		Current->RequestSwitch(Index);
	}
}

void UWuwaFormationUiController::HandlePlayerControllerChanged(APlayerController* NewController)
{
	BindFormation(NewController ? NewController->FindComponentByClass<UWuwaFormationComponent>() : nullptr);
}

void UWuwaFormationUiController::BindFormation(UWuwaFormationComponent* NewFormation)
{
	if (Formation.Get() == NewFormation)
	{
		return;
	}
	UnbindFormation();
	if (!NewFormation)
	{
		return;
	}
	Formation = NewFormation;
	MembersChangedHandle = NewFormation->OnMembersChanged.AddUObject(this, &UWuwaFormationUiController::HandleMembersChanged);
	RoleSwitchedHandle = NewFormation->OnRoleSwitched.AddUObject(this, &UWuwaFormationUiController::HandleRoleSwitched);
	// 绑上时队伍可能已经生成好了（比如 Controller 晚于队伍初始化），先同步一次。
	if (!NewFormation->GetMembers().IsEmpty())
	{
		RebuildMembers();
	}
}

void UWuwaFormationUiController::UnbindFormation()
{
	UnbindMemberHealth();
	if (UWuwaFormationComponent* Old = Formation.Get())
	{
		Old->OnMembersChanged.Remove(MembersChangedHandle);
		Old->OnRoleSwitched.Remove(RoleSwitchedHandle);
	}
	MembersChangedHandle.Reset();
	RoleSwitchedHandle.Reset();
	Formation.Reset();
	if (UWuwaFormationModel* Model = GetModel<UWuwaFormationModel>())
	{
		Model->SetMembers({}, INDEX_NONE);
	}
}

void UWuwaFormationUiController::UnbindMemberHealth()
{
	for (FHealthBinding& Binding : HealthBindings)
	{
		if (UAbilitySystemComponent* ASC = Binding.ASC.Get())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UWuwaAttributeSet::GetHealthAttribute()).Remove(Binding.Health);
			ASC->GetGameplayAttributeValueChangeDelegate(UWuwaAttributeSet::GetMaxHealthAttribute()).Remove(Binding.MaxHealth);
			for (const TPair<FGameplayAttribute, FDelegateHandle>& Attribute : Binding.Attributes)
			{
				ASC->GetGameplayAttributeValueChangeDelegate(Attribute.Key).Remove(Attribute.Value);
			}
			ASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(Binding.EffectAdded);
			ASC->OnAnyGameplayEffectRemovedDelegate().Remove(Binding.EffectRemoved);
		}
	}
	HealthBindings.Reset();
}

void UWuwaFormationUiController::HandleMembersChanged(const TArray<TObjectPtr<AWuwaCharacter>>& NewMembers)
{
	RebuildMembers();
}

void UWuwaFormationUiController::RebuildMembers()
{
	UWuwaFormationComponent* Current = Formation.Get();
	UWuwaFormationModel* Model = GetModel<UWuwaFormationModel>();
	if (!Current || !Model)
	{
		return;
	}

	UnbindMemberHealth();
	TArray<FWuwaTeamMemberUiData> Snapshot;
	const TArray<TObjectPtr<AWuwaCharacter>>& Members = Current->GetMembers();
	for (int32 Index = 0; Index < Members.Num(); ++Index)
	{
		const AWuwaCharacter* Role = Members[Index];
		FWuwaTeamMemberUiData& Data = Snapshot.AddDefaulted_GetRef();
		Data.Index = Index;
		// 每个序号都占一项，HealthBindings[Index] 始终对应第 Index 个队员。
		FHealthBinding& Binding = HealthBindings.AddDefaulted_GetRef();
		// 客户端上数组里的角色可能还没复制过来（为空）；它到了以后 OnRep_Members 会再触发一次重建。
		if (!IsValid(Role))
		{
			continue;
		}
		// RoleData 是角色蓝图的类默认值，每台机器都一样，客户端也能直接读。
		Data.RoleData = Role->RoleData;

		UAbilitySystemComponent* ASC = Role->GetAbilitySystemComponent();
		if (!ASC)
		{
			continue;
		}
		Data.Health = ASC->GetNumericAttribute(UWuwaAttributeSet::GetHealthAttribute());
		Data.MaxHealth = ASC->GetNumericAttribute(UWuwaAttributeSet::GetMaxHealthAttribute());
		Binding.ASC = ASC;
		// 不在场的队员也要更新：它们的 ASC 仍然归本玩家所有，客户端照样收到属性复制。
		Binding.Health = ASC->GetGameplayAttributeValueChangeDelegate(UWuwaAttributeSet::GetHealthAttribute())
			.AddWeakLambda(this, [this, Index](const FOnAttributeChangeData&) { HandleHealthChanged(Index); });
		Binding.MaxHealth = ASC->GetGameplayAttributeValueChangeDelegate(UWuwaAttributeSet::GetMaxHealthAttribute())
			.AddWeakLambda(this, [this, Index](const FOnAttributeChangeData&) { HandleHealthChanged(Index); });
		WuwaFormationUi::ReadAttributes(ASC, Data);
		for (const FGameplayAttribute& Attribute : WuwaFormationUi::DisplayAttributes())
		{
			Binding.Attributes.Emplace(Attribute, ASC->GetGameplayAttributeValueChangeDelegate(Attribute)
				.AddWeakLambda(this, [this, Index](const FOnAttributeChangeData&) { HandleAttributesChanged(Index); }));
		}
		Binding.EffectAdded = ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddWeakLambda(this,
			[this, Index](UAbilitySystemComponent*, const FGameplayEffectSpec&, FActiveGameplayEffectHandle) { HandleBuffsChanged(Index); });
		Binding.EffectRemoved = ASC->OnAnyGameplayEffectRemovedDelegate().AddWeakLambda(this,
			[this, Index](const FActiveGameplayEffect& Effect) { HandleBuffsChanged(Index, Effect.Handle); });
	}
	Model->SetMembers(MoveTemp(Snapshot), Current->GetCurrentIndex());
	for (int32 Index = 0; Index < HealthBindings.Num(); ++Index)
	{
		if (const UAbilitySystemComponent* ASC = HealthBindings[Index].ASC.Get()) Model->SetBuffs(Index, CollectBuffs(ASC));
	}

	FWuwaFormationMembersEvent Event;
	Event.Members = Model->GetMembers();
	Event.CurrentIndex = Model->GetCurrentIndex();
	UE_LOG(LogWuwaUI, Log, TEXT("Formation UI: %d members, current %d"), Event.Members.Num(), Event.CurrentIndex);
	EmitEvent(WuwaFormationEvents::Members, Event);
}

TArray<FWuwaBuffUiData> UWuwaFormationUiController::CollectBuffs(const UAbilitySystemComponent* ASC, FActiveGameplayEffectHandle Excluded)
{
	TArray<FWuwaBuffUiData> Buffs;
	const UWorld* World = ASC ? ASC->GetWorld() : nullptr;
	if (!World) return Buffs;
	const double Now = World->GetTimeSeconds();
	for (const FActiveGameplayEffectHandle& Handle : ASC->GetActiveGameplayEffects().GetAllActiveEffectHandles())
	{
		const FActiveGameplayEffect* Effect = Handle == Excluded ? nullptr : ASC->GetActiveGameplayEffect(Handle);
		const UWuwaBuffInfoComponent* Info = Effect && !Effect->IsPendingRemove ? UWuwaBuffLibrary::GetBuffInfo(Effect->Spec.Def) : nullptr;
		if (!Info) continue;
		for (const int64 CueId : Info->CueIds)
		{
			// 原作 BuffItemContainer 只显示这几种 cue（其余是特效、材质等，不是图标）
			const FWuwaBuffCueRow* Cue = UWuwaBuffLibrary::FindCue(CueId);
			if (!Cue || !(Cue->CueType == 2 || Cue->CueType == 14 || Cue->CueType == 24 || Cue->CueType == 37)) continue;
			FWuwaBuffUiData& Data = Buffs.AddDefaulted_GetRef();
			Data.Handle = Handle;
			Data.BuffId = Info->BuffId;
			Data.CueId = CueId;
			Data.CueType = Cue->CueType;
			Data.Icon = Cue->Icon;
			Data.Priority = Cue->Priority;
			Data.Parameters = Cue->Parameters;
			Data.Duration = Effect->GetDuration();
			Data.EndWorldTime = Data.Duration > 0.f ? Now + Effect->GetTimeRemaining(static_cast<float>(Now)) : 0.0;
			Data.StackCount = Effect->Spec.GetStackCount();
		}
	}
	return Buffs;
}

void UWuwaFormationUiController::HandleBuffsChanged(int32 Index, FActiveGameplayEffectHandle Removed)
{
	UWuwaFormationModel* Model = GetModel<UWuwaFormationModel>();
	const UAbilitySystemComponent* ASC = HealthBindings.IsValidIndex(Index) ? HealthBindings[Index].ASC.Get() : nullptr;
	if (!Model || !ASC) return;
	TArray<FWuwaBuffUiData> Buffs = CollectBuffs(ASC, Removed);
	const TArray<FWuwaBuffUiData>* Old = Model->GetBuffs(Index);
	// 没有带图标的 buff 进出（比如普通 GE）时不发事件
	if (Old && Old->Num() == Buffs.Num() && Algo::AllOf(Buffs, [Old](const FWuwaBuffUiData& B)
		{ return Old->ContainsByPredicate([&B](const FWuwaBuffUiData& O) { return O.IsSameIcon(B) && O.StackCount == B.StackCount; }); }))
	{
		return;
	}
	Model->SetBuffs(Index, MoveTemp(Buffs));
	FWuwaFormationMemberBuffsEvent Event;
	Event.Index = Index;
	Event.Buffs = *Model->GetBuffs(Index);
	EmitEvent(WuwaFormationEvents::MemberBuffs, Event);
}

void UWuwaFormationUiController::HandleRoleSwitched(AWuwaCharacter* OldRole, AWuwaCharacter* NewRole, int32 NewIndex)
{
	UWuwaFormationModel* Model = GetModel<UWuwaFormationModel>();
	if (!Model)
	{
		return;
	}
	FWuwaFormationSwitchedEvent Event;
	Event.OldIndex = Model->GetCurrentIndex();
	Model->SetCurrentIndex(NewIndex);
	Event.NewIndex = Model->GetCurrentIndex();
	EmitEvent(WuwaFormationEvents::Switched, Event);
}

void UWuwaFormationUiController::HandleHealthChanged(int32 Index)
{
	UWuwaFormationModel* Model = GetModel<UWuwaFormationModel>();
	const UAbilitySystemComponent* ASC = HealthBindings.IsValidIndex(Index) ? HealthBindings[Index].ASC.Get() : nullptr;
	if (!Model || !ASC)
	{
		return;
	}
	FWuwaFormationMemberHealthEvent Event;
	Event.Index = Index;
	Event.Health = ASC->GetNumericAttribute(UWuwaAttributeSet::GetHealthAttribute());
	Event.MaxHealth = ASC->GetNumericAttribute(UWuwaAttributeSet::GetMaxHealthAttribute());
	if (Model->SetHealth(Index, Event.Health, Event.MaxHealth))
	{
		EmitEvent(WuwaFormationEvents::MemberHealth, Event);
	}
}

void UWuwaFormationUiController::HandleAttributesChanged(int32 Index)
{
	UWuwaFormationModel* Model = GetModel<UWuwaFormationModel>();
	const UAbilitySystemComponent* ASC = HealthBindings.IsValidIndex(Index) ? HealthBindings[Index].ASC.Get() : nullptr;
	const FWuwaTeamMemberUiData* Current = Model ? Model->GetMember(Index) : nullptr;
	if (!Current || !ASC)
	{
		return;
	}
	FWuwaTeamMemberUiData Values = *Current;
	WuwaFormationUi::ReadAttributes(ASC, Values);
	if (Model->SetAttributes(Index, Values))
	{
		FWuwaFormationMemberAttributesEvent Event;
		Event.Member = *Model->GetMember(Index);
		EmitEvent(WuwaFormationEvents::MemberAttributes, Event);
	}
}
