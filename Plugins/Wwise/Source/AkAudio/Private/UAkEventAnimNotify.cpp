/*******************************************************************************
The content of this file includes portions of the proprietary AUDIOKINETIC Wwise
Technology released in source code form as part of the game integration package.
The content of this file may not be used without valid licenses to the
AUDIOKINETIC Wwise Technology.
Note that the use of the game engine is subject to the Unreal(R) Engine End User
License Agreement at https://www.unrealengine.com/en-US/eula/unreal

License Usage

Licensees holding valid licenses to the AUDIOKINETIC Wwise Technology may use
this file in accordance with the end user license agreement provided with the
software or, alternatively, in accordance with the terms contained
in a written agreement between you and Audiokinetic Inc.
Copyright (c) 2026 Audiokinetic Inc.
*******************************************************************************/

#include "UAkEventAnimNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "AkAudio/Classes/AkGameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "AkComponent.h"

void UAkEventAnimNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	UAnimNotify::Notify(MeshComp, Animation, EventReference);

	if (Follow)
	{
		bool bCreated;
		UAkComponent* component = UAkGameplayStatics::GetOrCreateAkComponent(MeshComp, bCreated, FName(AttachName));

		if (bCreated)
		{
			UKismetSystemLibrary::PrintString(GetWorld(), FString(TEXT("WARNING: AkComponent has been created in the notify. It will be using default values for all properties.")), true, true, FLinearColor::Yellow, 10.0f);
			UKismetSystemLibrary::PrintString(
				GetWorld(),
				FString::Format(
					TEXT("WARNING: Should you wish to use different values, please attach an AkComponent to the {0} component on the {1} actor beforehand."),
					{
						UKismetSystemLibrary::GetDisplayName(MeshComp),
						UKismetSystemLibrary::GetDisplayName(MeshComp->GetOwner())
					}
				),
				true, true, FLinearColor::Yellow, 10.0f
			);
		}

		if (IsValid(component))
		{
			component->PostAkEvent(Event, 0, FOnAkPostEventCallback());
		}
	}
	else
	{
		UAkGameplayStatics::PostEventAtLocation(Event, MeshComp->GetComponentLocation(), FRotator::ZeroRotator, MeshComp->GetWorld());
	}
}
