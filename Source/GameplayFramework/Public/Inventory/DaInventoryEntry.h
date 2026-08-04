// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "DaInventoryEntry.generated.h"

struct FDaInventoryList;

/**
 * FDaTagStack
 * One numeric per-instance stat: tag -> count (e.g. Item.Stat.Grade -> 7).
 */
USTRUCT(BlueprintType)
struct GAMEPLAYFRAMEWORK_API FDaTagStack
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FGameplayTag Tag;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	int32 Count = 0;
};

/**
 * FDaInventoryEntry
 *
 * A single replicated inventory item instance, stored inside FDaInventoryList.
 * Derives from FFastArraySerializerItem so the engine can diff individual entries
 * across the network rather than replicating the entire array.
 */
USTRUCT(BlueprintType)
struct GAMEPLAYFRAMEWORK_API FDaInventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FDaInventoryEntry()
		: SlotIndex(INDEX_NONE)
		, StackCount(1)
		, MaxStackCount(1)
	{
	}

	// Unique instance identifier (persists across save / load)
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FGuid ItemID;

	// Primary asset reference to the item definition data asset
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FPrimaryAssetId ItemDefinitionID;

	// Which inventory slot this entry occupies
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	int32 SlotIndex;

	// Current stack count
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	int32 StackCount;

	// Maximum stack size (copied from the item definition)
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	int32 MaxStackCount;

	// Runtime gameplay tags attached to this item instance
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FGameplayTagContainer Tags;

	// Optional ability set granted while this item is active
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FPrimaryAssetId AbilitySetID;

	// Numeric per-instance state as tag->count pairs (grade, durability, charges, ...).
	// A plain array on purpose: a nested FastArray cannot delta-serialize inside
	// another FastArray item. Mutate ONLY through UDaInventoryComponent's stat API.
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	TArray<FDaTagStack> StatTags;

	// Non-replicated query accelerator; server maintains inline, clients rebuild
	// in the FastArray replication callbacks.
	TMap<FGameplayTag, int32> StatCountMap;

	int32 GetStatCount(FGameplayTag Tag) const;
	void SetStatCount(FGameplayTag Tag, int32 Count);
	void RebuildStatCountMap();

	// ----- Queries -----

	/** Returns true when this entry has a valid ItemID. */
	bool IsValid() const { return ItemID.IsValid(); }

	/** Returns true when this entry supports stacking. */
	bool IsStackable() const { return MaxStackCount > 1; }

	/** Returns true when Other has the same definition and this stack still has room. */
	bool CanStackWith(const FDaInventoryEntry& Other) const
	{
		return ItemDefinitionID == Other.ItemDefinitionID
			&& IsStackable()
			&& StackCount < MaxStackCount;
	}

	// ----- FastArraySerializer callbacks -----

	void PreReplicatedRemove(const FDaInventoryList& OwnerList);
	void PostReplicatedAdd(const FDaInventoryList& OwnerList);
	void PostReplicatedChange(const FDaInventoryList& OwnerList);
};
