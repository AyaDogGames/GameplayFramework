// Copyright Dream Awake Solutions LLC

#include "Inventory/DaInventoryList.h"

#include "Inventory/DaInventoryComponent.h"

namespace
{
	// Mutators only run on the authority; FastArray callbacks only run on
	// replicating clients. Broadcasting from the mutators when the owner has
	// authority covers servers, listen-server hosts, and standalone without
	// double-firing on remote clients.
	bool ShouldBroadcastLocally(const UDaInventoryComponent* OwnerComponent)
	{
		return OwnerComponent && OwnerComponent->GetOwnerRole() == ROLE_Authority;
	}
}

void FDaInventoryList::AddEntry(const FDaInventoryEntry& NewEntry)
{
	Entries.Add(NewEntry);
	MarkItemDirty(Entries.Last());

	if (ShouldBroadcastLocally(OwnerComponent))
	{
		OwnerComponent->OnEntryAddedInternal(Entries.Last());
	}
}

void FDaInventoryList::RemoveEntry(int32 SlotIndex)
{
	for (int32 i = Entries.Num() - 1; i >= 0; --i)
	{
		if (Entries[i].SlotIndex == SlotIndex)
		{
			const FDaInventoryEntry RemovedEntry = Entries[i];
			Entries.RemoveAt(i);
			MarkArrayDirty();

			if (ShouldBroadcastLocally(OwnerComponent))
			{
				OwnerComponent->OnEntryRemovedInternal(RemovedEntry);
			}
			return;
		}
	}
}

void FDaInventoryList::UpdateEntry(int32 SlotIndex, const FDaInventoryEntry& Updated)
{
	if (FDaInventoryEntry* Entry = FindBySlotMutable(SlotIndex))
	{
		*Entry = Updated;
		MarkItemDirty(*Entry);

		if (ShouldBroadcastLocally(OwnerComponent))
		{
			OwnerComponent->OnEntryChangedInternal(*Entry);
		}
	}
}

int32 FDaInventoryList::FindFirstEmptySlot(int32 MaxSize) const
{
	TSet<int32> OccupiedSlots;
	OccupiedSlots.Reserve(Entries.Num());

	for (const FDaInventoryEntry& Entry : Entries)
	{
		OccupiedSlots.Add(Entry.SlotIndex);
	}

	for (int32 i = 0; i < MaxSize; ++i)
	{
		if (!OccupiedSlots.Contains(i))
		{
			return i;
		}
	}

	return INDEX_NONE;
}

int32 FDaInventoryList::FindStackableSlot(const FDaInventoryEntry& ForEntry) const
{
	for (const FDaInventoryEntry& Entry : Entries)
	{
		if (Entry.CanStackWith(ForEntry))
		{
			return Entry.SlotIndex;
		}
	}

	return INDEX_NONE;
}

const FDaInventoryEntry* FDaInventoryList::FindBySlot(int32 SlotIndex) const
{
	for (const FDaInventoryEntry& Entry : Entries)
	{
		if (Entry.SlotIndex == SlotIndex)
		{
			return &Entry;
		}
	}

	return nullptr;
}

FDaInventoryEntry* FDaInventoryList::FindBySlotMutable(int32 SlotIndex)
{
	for (FDaInventoryEntry& Entry : Entries)
	{
		if (Entry.SlotIndex == SlotIndex)
		{
			return &Entry;
		}
	}

	return nullptr;
}
