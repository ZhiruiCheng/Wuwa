#include "Game/Common/Resource/WuwaCombatWarmupSubsystem.h"

#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/Common/Resource/WuwaResourceSubsystem.h"
#include "Game/EffectModel/Base/WuwaEffectModelBase.h"
#include "Game/Module/BattleUi/WuwaBattleUiSettings.h"
#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"
#include "Game/NewWorld/Combat/WuwaCombatSettings.h"
#include "NiagaraSystem.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaWarmup, Log, All);

UWuwaCombatWarmupSubsystem* UWuwaCombatWarmupSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UWuwaCombatWarmupSubsystem>() : nullptr;
}

bool UWuwaCombatWarmupSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

namespace WuwaWarmup
{
	void CollectValue(const FProperty* Property, const void* Value, TSet<FSoftObjectPath>& Out)
	{
		if (const FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(Property))
		{
			const FSoftObjectPath Path = Soft->GetPropertyValue(Value).ToSoftObjectPath();
			if (Path.IsValid()) Out.Add(Path);
		}
		else if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
		{
			if (Struct->Struct == TBaseStructure<FSoftObjectPath>::Get() || Struct->Struct == TBaseStructure<FSoftClassPath>::Get())
			{
				const FSoftObjectPath& Path = *static_cast<const FSoftObjectPath*>(Value);
				if (Path.IsValid()) Out.Add(Path);
			}
			else
			{
				UWuwaCombatWarmupSubsystem::CollectSoftReferences(Struct->Struct, Value, Out);
			}
		}
		else if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper Helper(Array, Value);
			for (int32 Index = 0; Index < Helper.Num(); ++Index) CollectValue(Array->Inner, Helper.GetRawPtr(Index), Out);
		}
		else if (const FMapProperty* Map = CastField<FMapProperty>(Property))
		{
			FScriptMapHelper Helper(Map, Value);
			for (FScriptMapHelper::FIterator It(Helper); It; ++It)
			{
				CollectValue(Map->KeyProp, Helper.GetKeyPtr(It), Out);
				CollectValue(Map->ValueProp, Helper.GetValuePtr(It), Out);
			}
		}
		else if (const FSetProperty* Set = CastField<FSetProperty>(Property))
		{
			FScriptSetHelper Helper(Set, Value);
			for (FScriptSetHelper::FIterator It(Helper); It; ++It) CollectValue(Set->ElementProp, Helper.GetElementPtr(It), Out);
		}
	}

	void CollectTable(const UDataTable* Table, TSet<FSoftObjectPath>& Out)
	{
		if (!Table || !Table->GetRowStruct()) return;
		for (const TPair<FName, uint8*>& Row : Table->GetRowMap()) UWuwaCombatWarmupSubsystem::CollectSoftReferences(Table->GetRowStruct(), Row.Value, Out);
	}

	bool IsInstance(const UObject* Object)
	{
		return Object && !Object->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && !Object->IsTemplate();
	}
}

void UWuwaCombatWarmupSubsystem::CollectSoftReferences(const UStruct* Struct, const void* Data, TSet<FSoftObjectPath>& Out)
{
	if (!Struct || !Data) return;
	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		for (int32 Index = 0; Index < It->ArrayDim; ++Index) WuwaWarmup::CollectValue(*It, It->ContainerPtrToValuePtr<void>(Data, Index), Out);
	}
}

void UWuwaCombatWarmupSubsystem::Begin()
{
	if (bBegun) return;
	bBegun = true;
	using namespace WuwaWarmup;
	TSet<FSoftObjectPath> Paths;
	CollectSoftReferences(UWuwaCombatSettings::StaticClass(), GetDefault<UWuwaCombatSettings>(), Paths);
	CollectSoftReferences(UWuwaBattleUiSettings::StaticClass(), GetDefault<UWuwaBattleUiSettings>(), Paths);
	// 已加载的动画通知 / 通知状态 / 特效模型（都来自进场时加载的角色、Boss 的蒙太奇）
	for (TObjectIterator<UAnimNotify> It; It; ++It) if (IsInstance(*It)) CollectSoftReferences(It->GetClass(), *It, Paths);
	for (TObjectIterator<UAnimNotifyState> It; It; ++It) if (IsInstance(*It)) CollectSoftReferences(It->GetClass(), *It, Paths);
	for (TObjectIterator<UWuwaEffectModelBase> It; It; ++It) if (IsInstance(*It)) CollectSoftReferences(It->GetClass(), *It, Paths);
	// 场景里的战斗单位（含队伍里离场的角色）：自身的软引用（受击蒙太奇……）、子弹表（顿帧曲线……）、受击效果表
	for (TActorIterator<AWuwaCharactorBase> It(GetWorld()); It; ++It)
	{
		CollectSoftReferences(It->GetClass(), *It, Paths);
		CollectTable(It->BulletTable, Paths);
		CollectTable(It->HitEffectTable, Paths);
	}
	TArray<FSoftObjectPath> ToLoad;
	for (const FSoftObjectPath& Path : Paths)
	{
		if (!Path.GetLongPackageName().StartsWith(TEXT("/Script/"))) ToLoad.Add(Path);
	}
	AssetCount = ToLoad.Num();
	if (UWuwaResourceSubsystem* Resources = UWuwaResourceSubsystem::Get(this))
	{
		Handle = Resources->LoadAsync(ToLoad, TEXT("CombatWarmup"), nullptr, UWuwaResourceSubsystem::HighPriority);
	}
#if WITH_EDITORONLY_DATA
	for (TObjectIterator<UNiagaraSystem> It; It; ++It)
	{
		UNiagaraSystem* System = *It;
		if (!IsInstance(System) || !System->NeedsRequestCompile()) continue;
		System->RequestCompile(false);
		CompilingSystems.Add(System);
	}
	CompileTotal = CompilingSystems.Num();
#endif
	UE_LOG(LogWuwaWarmup, Display, TEXT("Combat warmup: %d soft references to load, %d Niagara systems to compile."), AssetCount, CompileTotal);
}

float UWuwaCombatWarmupSubsystem::GetProgress() const
{
	if (!bBegun) return 0.f;
	const float Load = Handle.IsValid() ? Handle->GetProgress() : 1.f;
	const float Compile = CompileTotal > 0 ? 1.f - static_cast<float>(CompilingSystems.Num()) / CompileTotal : 1.f;
	return 0.8f * Load + 0.2f * Compile;
}

bool UWuwaCombatWarmupSubsystem::IsDone() const
{
	if (!bBegun) return false;
#if WITH_EDITORONLY_DATA
	UWuwaCombatWarmupSubsystem* Self = const_cast<UWuwaCombatWarmupSubsystem*>(this);
	Self->CompilingSystems.RemoveAll([](const TWeakObjectPtr<UObject>& Weak)
	{
		UNiagaraSystem* System = Cast<UNiagaraSystem>(Weak.Get());
		return !System || System->PollForCompilationComplete();
	});
#endif
	return (!Handle.IsValid() || Handle->HasLoadCompleted() || Handle->WasCanceled()) && CompilingSystems.IsEmpty();
}

void UWuwaCombatWarmupSubsystem::Deinitialize()
{
	if (Handle.IsValid()) Handle->ReleaseHandle();
	Handle.Reset();
	Super::Deinitialize();
}
