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
	if (!InventoryComponent->OnItemUsed.IsAlreadyBound(this, &UDaInventoryWidgetController::HandleItemUsed))
	{
		InventoryComponent->OnItemUsed.AddDynamic(this, &UDaInventoryWidgetController::HandleItemUsed);
	}
	if (!InventoryComponent->OnItemDropped.IsAlreadyBound(this, &UDaInventoryWidgetController::HandleItemDropped))
	{
		InventoryComponent->OnItemDropped.AddDynamic(this, &UDaInventoryWidgetController::HandleItemDropped);
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

bool UDaInventoryWidgetController::UseItem(int32 SlotIndex)
{
	if (!InventoryComponent)
	{
		LOG_WARNING("UDaInventoryWidgetController::UseItem: not initialized (call InitializeInventory first)");
		return false;
	}
	return InventoryComponent->UseItem(SlotIndex);
}

bool UDaInventoryWidgetController::DropItem(int32 SlotIndex, int32 Count)
{
	if (!InventoryComponent)
	{
		LOG_WARNING("UDaInventoryWidgetController::DropItem: not initialized (call InitializeInventory first)");
		return false;
	}
	return InventoryComponent->DropItem(SlotIndex, Count);
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

void UDaInventoryWidgetController::HandleItemUsed(const FDaInventoryEntry& Entry, int32 SlotIndex)
{
	// Build a one-off view-model from the entry snapshot: the slot may already have
	// been consumed/emptied by the time this confirmation arrives.
	OnItemUsed.Broadcast(UDaInventoryItemBase::CreateFromEntry(Entry, this), SlotIndex);
}

void UDaInventoryWidgetController::HandleItemDropped(const FDaInventoryEntry& Entry, int32 SlotIndex)
{
	OnItemDropped.Broadcast(UDaInventoryItemBase::CreateFromEntry(Entry, this), SlotIndex);
}
