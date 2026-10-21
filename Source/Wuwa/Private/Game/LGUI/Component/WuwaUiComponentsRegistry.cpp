#include "Game/LGUI/Component/WuwaUiComponentsRegistry.h"

#include "GameFramework/Actor.h"

UWuwaUiComponentsRegistry::UWuwaUiComponentsRegistry()
{
	PrimaryComponentTick.bCanEverTick = false;
}

AActor* UWuwaUiComponentsRegistry::GetActorAt(int32 Index) const
{
	return Components.IsValidIndex(Index) ? Components[Index].Get() : nullptr;
}

#if WITH_EDITOR
void UWuwaUiComponentsRegistry::SetComponents(const TArray<AActor*>& InComponents)
{
	Components.Reset(InComponents.Num());
	for (AActor* Actor : InComponents)
	{
		Components.Add(Actor);
	}
}
#endif
