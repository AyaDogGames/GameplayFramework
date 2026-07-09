// Copyright Dream Awake Solutions LLC


#include "Inventory/DaInventoryWidgetController.h"

#include "GameplayFramework.h"
#include "Inventory/DaInventoryComponent.h"
#include "Inventory/DaInventoryEntry.h"
#include "Inventory/DaInventoryItemBase.h"

void UDaInventoryWidgetController::InitializeInventory(AActor* Actor)
{
	InventoryComponent = UDaInventoryComponent::GetInventoryFromActor(Actor);
	if (!InventoryComponent)
	{
		LOG_WARNING("UDaInventoryWidgetController::InitializeInventory: InventoryComponent is null, Actor: %s", *GetNameSafe(Actor));
		return;
	}

	// Subscribe to the FastArray component's per-entry change notifications.
	if (!InventoryComponent->OnEntryAdded.IsAlreadyBound(this, &UDaInventoryWidgetController::HandleEntryAdded))
	{
		InventoryComponent->OnEntryAdded.AddDynamic(this, &UDaInventoryWidgetController::HandleEntryAdded);
	}
	if (!InventoryComponent->OnEntryRemoved.IsAlreadyBound(this, &UDaInventoryWidgetController::HandleEntryRemoved))
	{
		InventoryComponent->OnEntryRemoved.AddDynamic(this, &UDaInventoryWidgetController::HandleEntryRemoved);
	}
	if (!InventoryComponent->OnEntryChanged.IsAlreadyBound(this, &UDaInventoryWidgetController::HandleEntryChanged))
	{
		InventoryComponent->OnEntryChanged.AddDynamic(this, &UDaInventoryWidgetController::HandleEntryChanged);
	}

	// Build view-models for any items already present and broadcast the initial state.
	RebuildItems();
	OnInventoryChanged.Broadcast(GetItems());
}

TArray<UDaInventoryItemBase*> UDaInventoryWidgetController::GetItems() const
{
	TArray<UDaInventoryItemBase*> Result;
	Result.Reserve(Items.Num());
	for (const TObjectPtr<UDaInventoryItemBase>& Item : Items)
	{
		Result.Add(Item);
	}
	return Result;
}

void UDaInventoryWidgetController::RebuildItems()
{
	Items.Reset();
	if (!InventoryComponent)
	{
		return;
	}

	for (const FDaInventoryEntry& Entry : InventoryComponent->GetAllEntries())
	{
		if (UDaInventoryItemBase* Item = UDaInventoryItemBase::CreateFromEntry(Entry, this))
		{
			Items.Add(Item);
		}
	}
}

void UDaInventoryWidgetController::HandleEntryAdded(const FDaInventoryEntry& Entry, int32 SlotIndex)
{
	// Rebuild keeps the array simple and correct; inventories are small.
	RebuildItems();
	FOnInventoryItemChanged.Broadcast(GetItems(), SlotIndex);
	OnInventoryChanged.Broadcast(GetItems());
}

void UDaInventoryWidgetController::HandleEntryRemoved(const FDaInventoryEntry& Entry, int32 SlotIndex)
{
	RebuildItems();
	FOnInventoryItemChanged.Broadcast(GetItems(), SlotIndex);
	OnInventoryChanged.Broadcast(GetItems());
}

void UDaInventoryWidgetController::HandleEntryChanged(const FDaInventoryEntry& Entry, int32 SlotIndex)
{
	RebuildItems();
	FOnInventoryItemChanged.Broadcast(GetItems(), SlotIndex);
	OnInventoryChanged.Broadcast(GetItems());
}
