// Copyright Dream Awake Solutions LLC

#include "Inventory/DaInventoryEntry.h"
#include "Inventory/DaInventoryList.h"
#include "Inventory/DaInventoryComponent.h"

void FDaInventoryEntry::PreReplicatedRemove(const FDaInventoryList& OwnerList)
{
	if (OwnerList.OwnerComponent)
	{
		OwnerList.OwnerComponent->OnEntryRemovedInternal(*this);
	}
}

void FDaInventoryEntry::PostReplicatedAdd(const FDaInventoryList& OwnerList)
{
	if (OwnerList.OwnerComponent)
	{
		OwnerList.OwnerComponent->OnEntryAddedInternal(*this);
	}
}

void FDaInventoryEntry::PostReplicatedChange(const FDaInventoryList& OwnerList)
{
	if (OwnerList.OwnerComponent)
	{
		OwnerList.OwnerComponent->OnEntryChangedInternal(*this);
	}
}
