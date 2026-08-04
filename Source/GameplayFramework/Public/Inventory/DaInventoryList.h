// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "DaInventoryEntry.h"
#include "DaInventoryList.generated.h"

class UDaInventoryComponent;

/**
 * FDaInventoryList
 *
 * FFastArraySerializer wrapper that owns the replicated array of FDaInventoryEntry.
 * Lives inside UDaInventoryComponent and handles add / remove / update operations
 * while keeping the network delta-serialiser in sync.
 */
USTRUCT(BlueprintType)
struct GAMEPLAYFRAMEWORK_API FDaInventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

	FDaInventoryList()
		: OwnerComponent(nullptr)
	{
	}

	// ----- Serialisation -----

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FDaInventoryEntry, FDaInventoryList>(Entries, DeltaParms, *this);
	}

	// ----- Mutators -----

	/** Append a new entry and mark it dirty for replication. */
	void AddEntry(const FDaInventoryEntry& NewEntry);

	/** Remove the entry occupying SlotIndex and mark the array dirty. */
	void RemoveEntry(int32 SlotIndex);

	/** Replace the entry at SlotIndex with Updated data and mark it dirty. */
	void UpdateEntry(int32 SlotIndex, const FDaInventoryEntry& Updated);

	// ----- Queries -----

	/** Find the lowest slot index in [0, MaxSize) that is not occupied. Returns INDEX_NONE if full. */
	int32 FindFirstEmptySlot(int32 MaxSize) const;

	/** Find an existing entry that can stack with ForEntry. Returns its slot index or INDEX_NONE. */
	int32 FindStackableSlot(const FDaInventoryEntry& ForEntry) const;

	/** Return a read-only pointer to the entry at SlotIndex, or nullptr. */
	const FDaInventoryEntry* FindBySlot(int32 SlotIndex) const;

	/** Return a mutable pointer to the entry at SlotIndex, or nullptr. */
	FDaInventoryEntry* FindBySlotMutable(int32 SlotIndex);

	/** Number of entries currently in the list. */
	int32 GetCount() const { return Entries.Num(); }

	/** Return a read-only reference to the full entries array. */
	const TArray<FDaInventoryEntry>& GetEntries() const { return Entries; }

private:

	friend FDaInventoryEntry;
	friend UDaInventoryComponent;

	// The replicated item array
	UPROPERTY()
	TArray<FDaInventoryEntry> Entries;

	// Owning component — set during initialisation, not replicated
	UPROPERTY(NotReplicated)
	TObjectPtr<UDaInventoryComponent> OwnerComponent;
};

/** Enable NetDeltaSerialize for FDaInventoryList. */
template<>
struct TStructOpsTypeTraits<FDaInventoryList> : public TStructOpsTypeTraitsBase2<FDaInventoryList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};
