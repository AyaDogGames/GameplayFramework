// Copyright Dream Awake Solutions LLC


#include "Inventory/DaInventoryWidgetController.h"

#include "GameplayFramework.h"
#include "Inventory/DaInventoryComponent.h"
#include "Inventory/DaInventoryItemBase.h"

void UDaInventoryWidgetController::InitializeInventory(AActor* Actor)
{
	InventoryComponent = UDaInventoryComponent::GetInventoryFromActor(Actor);
	if (!InventoryComponent)
	{
		LOG_WARNING("UDaInventoryWidgetController::InitializeInventory: InventoryComponent is null, Actor: %s", *GetNameSafe(Actor));
		return;
	}
	
	// Subscribe to inventory change notifications
	InventoryComponent->OnInventoryChanged.AddDynamic(this, &UDaInventoryWidgetController::HandleInventoryChanged);
	
}

void UDaInventoryWidgetController::HandleInventoryChanged(const TArray<UDaInventoryItemBase*>& Items, int32 SlotIndex)
{
	if (SlotIndex == INDEX_NONE)
	{
		OnInventoryChanged.Broadcast(Items);
	} else
	{
		FOnInventoryItemChanged.Broadcast(Items, SlotIndex);
	}
}