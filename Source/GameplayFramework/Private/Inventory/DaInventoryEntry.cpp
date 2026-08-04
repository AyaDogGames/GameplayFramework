// Copyright Dream Awake Solutions LLC

#include "Inventory/DaInventoryEntry.h"
#include "Inventory/DaInventoryList.h"
#include "Inventory/DaInventoryComponent.h"

int32 FDaInventoryEntry::GetStatCount(FGameplayTag Tag) const
{
	if (const int32* Found = StatCountMap.Find(Tag))
	{
		return *Found;
	}

	// Fallback covers copies whose accelerator was never built.
	for (const FDaTagStack& Stack : StatTags)
	{
		if (Stack.Tag == Tag)
		{
			return Stack.Count;
		}
	}

	return 0;
}

void FDaInventoryEntry::SetStatCount(FGameplayTag Tag, int32 Count)
{
	for (FDaTagStack& Stack : StatTags)
	{
		if (Stack.Tag == Tag)
		{
			if (Count == 0)
			{
				StatTags.RemoveAll([Tag](const FDaTagStack& S){ return S.Tag == Tag; });
				StatCountMap.Remove(Tag);
			}
			else
			{
				Stack.Count = Count;
				StatCountMap.Add(Tag, Count);
			}
			return;
		}
	}

	if (Count != 0)
	{
		FDaTagStack& NewStack = StatTags.AddDefaulted_GetRef();
		NewStack.Tag = Tag;
		NewStack.Count = Count;
		StatCountMap.Add(Tag, Count);
	}
}

void FDaInventoryEntry::RebuildStatCountMap()
{
	StatCountMap.Reset();
	for (const FDaTagStack& Stack : StatTags)
	{
		StatCountMap.Add(Stack.Tag, Stack.Count);
	}
}

void FDaInventoryEntry::PreReplicatedRemove(const FDaInventoryList& OwnerList)
{
	if (OwnerList.OwnerComponent)
	{
		OwnerList.OwnerComponent->OnEntryRemovedInternal(*this);
	}
}

void FDaInventoryEntry::PostReplicatedAdd(const FDaInventoryList& OwnerList)
{
	// Before the broadcast: listeners read stats through the accelerator.
	RebuildStatCountMap();

	if (OwnerList.OwnerComponent)
	{
		OwnerList.OwnerComponent->OnEntryAddedInternal(*this);
	}
}

void FDaInventoryEntry::PostReplicatedChange(const FDaInventoryList& OwnerList)
{
	RebuildStatCountMap();

	if (OwnerList.OwnerComponent)
	{
		OwnerList.OwnerComponent->OnEntryChangedInternal(*this);
	}
}
