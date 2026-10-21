#include "Game/Effect/Specs/MaterialController/WuwaEffectMaterialControllerSpec.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Game/Effect/Types/WuwaEffectSpawnRequest.h"
#include "Game/EffectModel/Models/WuwaCharacterMaterialControllerData.h"
#include "Game/EffectModel/Models/WuwaEffectModelMaterialController.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaMaterialController, Log, All);

namespace WuwaMaterialController
{
	// 原作 RenderConfig 的材质参数名。
	const FName UseRim(TEXT("E_Rim_UseRim"));
	const FName RimUseTex(TEXT("E_Rim_UseTex"));
	const FName RimChannel(TEXT("E_Rim_Channel"));
	const FName RimRange(TEXT("E_Rim_RimRange"));
	const FName RimColor(TEXT("E_Rim_RimColor"));
	const FName RimIntensity(TEXT("E_Rim_Intensity"));
}

bool UWuwaEffectMaterialControllerSpec::Play_Implementation(const FWuwaEffectSpawnRequest& Request)
{
	const UWuwaEffectModelMaterialController* Model = Cast<UWuwaEffectModelMaterialController>(Request.Model);
	Data = Model ? Model->ControllerData.Get() : nullptr;
	const USceneComponent* Target = Request.AttachTo.Get();
	AActor* Owner = Target ? Target->GetOwner() : nullptr;
	if (!Data || !Owner)
	{
		UE_LOG(LogWuwaMaterialController, Warning, TEXT("%s: needs ControllerData and a character component as AttachTo."),
			Model ? *Model->GetName() : TEXT("(no model)"));
		return false;
	}
	if (Data->WholeTime() <= 0.f)
	{
		// 原作：材质控制器的总时长需大于 0。
		UE_LOG(LogWuwaMaterialController, Warning, TEXT("%s: controller length must be greater than 0."), *Data->GetName());
		return false;
	}

	Materials.Reset();
	TArray<USkeletalMeshComponent*> Meshes;
	Owner->GetComponents(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
		{
			if (UMaterialInstanceDynamic* Material = Mesh->CreateDynamicMaterialInstance(Slot))
			{
				Materials.AddDefaulted_GetRef().Material = Material;
			}
		}
	}

	CurrentTime = WholeTimeCounter = PhaseTimeCounter = Factor = 0.f;
	Phase = 0;
	bUpdatedOnce = bReadyToDie = false;
	bPlaying = true;
	return true;
}

void UWuwaEffectMaterialControllerSpec::Advance(float DeltaTime)
{
	// 原作 CharMaterialControlRuntimeData.UpdateState：先按累计时间判段（换段时段内计时清零），再累加时间。
	const float Start = Data->StartTime, Loop = Data->LoopTime, Whole = Data->WholeTime();
	auto Enter = [this](uint8 NewPhase)
	{
		if (Phase != NewPhase) PhaseTimeCounter = 0.f;
		Phase = NewPhase;
	};
	if (!bReadyToDie && WholeTimeCounter <= Start - DeltaTime) Enter(0);
	else if (!bReadyToDie && WholeTimeCounter <= Start + Loop - DeltaTime) Enter(1);
	else if (WholeTimeCounter <= Whole) Enter(2);

	CurrentTime += DeltaTime;
	WholeTimeCounter += DeltaTime;
	PhaseTimeCounter += DeltaTime;
	const float PhaseLength = Phase == 0 ? Start : Phase == 1 ? Loop : Data->EndTime;
	if (PhaseLength > 0.f)
	{
		PhaseTimeCounter = FMath::Fmod(PhaseTimeCounter, PhaseLength);
		Factor = FMath::Clamp(PhaseTimeCounter / PhaseLength, 0.f, 1.f);
	}
	// 效果类型：总时长前一帧标记结束（至少更新过一帧）；被要求结束时播完结束段。
	if (bUpdatedOnce && CurrentTime >= Whole - DeltaTime) bPlaying = false;
	bUpdatedOnce = true;
	if (bReadyToDie && CurrentTime >= Data->EndTime) bPlaying = false;
}

void UWuwaEffectMaterialControllerSpec::SetScalar(FWuwaControlledMaterial& Target, FName Name, float Value)
{
	if (!Target.OriginalScalars.Contains(Name))
	{
		float Original = 0.f;
		Target.Material->GetScalarParameterValue(Name, Original);
		Target.OriginalScalars.Add(Name, Original);
	}
	Target.Material->SetScalarParameterValue(Name, Value);
}

void UWuwaEffectMaterialControllerSpec::SetVector(FWuwaControlledMaterial& Target, FName Name, const FLinearColor& Value)
{
	if (!Target.OriginalVectors.Contains(Name))
	{
		FLinearColor Original = FLinearColor::Black;
		Target.Material->GetVectorParameterValue(Name, Original);
		Target.OriginalVectors.Add(Name, Original);
	}
	Target.Material->SetVectorParameterValue(Name, Value);
}

void UWuwaEffectMaterialControllerSpec::Apply()
{
	using namespace WuwaMaterialController;
	if (!Data->bUseRim) return;
	// 原作 CharMaterialContainer 的边缘光更新：UseRim=1，其余按参数组取样。
	const float Range = Data->RimRange.Evaluate(Phase, Factor);
	const FLinearColor Color = Data->RimColor.Evaluate(Phase, Factor);
	const float Intensity = Data->RimIntensity.Evaluate(Phase, Factor);
	for (FWuwaControlledMaterial& Target : Materials)
	{
		if (!IsValid(Target.Material)) continue;
		SetScalar(Target, UseRim, 1.f);
		SetScalar(Target, RimUseTex, Data->bRimUseTex ? 1.f : 0.f);
		SetVector(Target, RimChannel, Data->RimChannel);
		SetScalar(Target, RimRange, Range);
		SetVector(Target, RimColor, Color);
		SetScalar(Target, RimIntensity, Intensity);
	}
}

void UWuwaEffectMaterialControllerSpec::Revert()
{
	// 原作 RevertProperty：恢复材质容器里的原值。
	for (FWuwaControlledMaterial& Target : Materials)
	{
		if (!IsValid(Target.Material)) continue;
		for (const auto& Pair : Target.OriginalScalars) Target.Material->SetScalarParameterValue(Pair.Key, Pair.Value);
		for (const auto& Pair : Target.OriginalVectors) Target.Material->SetVectorParameterValue(Pair.Key, Pair.Value);
	}
	Materials.Reset();
}

void UWuwaEffectMaterialControllerSpec::Tick(float DeltaTime)
{
	Advance(FMath::Max(0.f, DeltaTime));
	if (bPlaying)
	{
		Apply();
		return;
	}
	Revert();
	Finish();
}

void UWuwaEffectMaterialControllerSpec::Stop_Implementation(bool bImmediately)
{
	if (IsFinished()) return;
	if (bImmediately || Data->EndTime <= 0.f)
	{
		bPlaying = false;
		Revert();
		Finish();
		return;
	}
	// 原作 SetReadyToDie：直接进入结束段播完。
	bReadyToDie = true;
	CurrentTime = 0.f;
}

bool UWuwaEffectMaterialControllerSpec::IsTickable() const
{
	return bPlaying && !IsFinished() && Data;
}

ETickableTickType UWuwaEffectMaterialControllerSpec::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

TStatId UWuwaEffectMaterialControllerSpec::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWuwaEffectMaterialControllerSpec, STATGROUP_Tickables);
}
