// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Framework/WuwaGameMode.h"

#include "Game/Formation/WuwaFormationComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

void AWuwaGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!IsValid(NewPlayer))
	{
		return;
	}
	
	APlayerController* PC = Cast<APlayerController>(NewPlayer);
	UWuwaFormationComponent* Formation = PC?PC->FindComponentByClass<UWuwaFormationComponent>():nullptr;
	
	if (!Formation)
	{
		Super::RestartPlayer(NewPlayer);
		return;
	}
	if (!Formation->GetMembers().IsEmpty())
	{
		return;
	}
	
	AActor* Start = FindPlayerStart(NewPlayer);

	// 和引擎一样，找不到时尝试之前分配的出生点。
	if (!Start)
	{
		Start = NewPlayer->StartSpot.Get();
	}

	if (!Start)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("RestartPlayer: no PlayerStart for %s."),
			*GetNameSafe(NewPlayer));
		return;
	}
	
	// 地面角色只使用出生点的 Yaw，不继承倾斜和缩放。
	const FRotator SpawnRotation(
		0.f,
		Start->GetActorRotation().Yaw,
		0.f);

	const FTransform SpawnTransform(
		SpawnRotation,
		Start->GetActorLocation(),
		FVector::OneVector);
	
	// 队伍组件先异步加载角色资源（这段时间加载界面在显示进度），加载完生成队员并控制第一名，再补齐重生流程。
	TWeakObjectPtr<AWuwaGameMode> WeakThis(this);
	TWeakObjectPtr<APlayerController> WeakPC(PC);
	TWeakObjectPtr<AActor> WeakStart(Start);
	Formation->InitFormation(SpawnTransform, [WeakThis, WeakPC, WeakStart, SpawnRotation](bool bSuccess)
	{
		AWuwaGameMode* Self = WeakThis.Get();
		APlayerController* Player = WeakPC.Get();
		if (!Self || !Player)
		{
			return;
		}
		UWuwaFormationComponent* PlayerFormation = Player->FindComponentByClass<UWuwaFormationComponent>();
		AWuwaCharacter* CurrentRole = PlayerFormation ? PlayerFormation->GetCurrentRole() : nullptr;
		if (!bSuccess || !IsValid(CurrentRole) || Player->GetPawn() != CurrentRole)
		{
			Self->FailedToRestartPlayer(Player);
			return;
		}

		// 保留引擎通知出生点已被使用的接口。
		if (AActor* StartSpot = WeakStart.Get())
		{
			Self->InitStartSpot(StartSpot, Player);
		}

		// 服务器设置控制朝向，并通知拥有这个 PC 的客户端。
		Player->SetControlRotation(SpawnRotation);
		Player->ClientSetRotation(SpawnRotation, true);

		// InitFormation 已经完成 Possess，这里补齐剩余的重生完成流程。
		Self->SetPlayerDefaults(CurrentRole);
		Self->K2_OnRestartPlayer(Player);
	});
}

