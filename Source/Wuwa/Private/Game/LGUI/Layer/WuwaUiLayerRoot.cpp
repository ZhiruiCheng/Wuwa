// Zacray All Rights Reserved


#include "Game/LGUI/Layer/WuwaUiLayerRoot.h"
#include "Core/ActorComponent/UIItem.h"

// Sets default values for this component's properties
UWuwaUiLayerRoot::UWuwaUiLayerRoot()
{
	PrimaryComponentTick.bCanEverTick = false;
	const int32 LayerCount = static_cast<int32>(EWuwaUiLayer::Count);
	Layers.Reserve(LayerCount);
	for (int32 i = 0;i<LayerCount;i++)
	{
		Layers.Add(FLGUIComponentReference(UUIItem::StaticClass()));
	}
}

UUIItem* UWuwaUiLayerRoot::GetLayerItem(EWuwaUiLayer Layer) const
{
	const int32 LayerIndex = static_cast<int32>(Layer);
	return LayerIndex < static_cast<int32>(EWuwaUiLayer::Count) && Layers.IsValidIndex(LayerIndex)
		? Layers[LayerIndex].GetComponent<UUIItem>()
		: nullptr;
}


