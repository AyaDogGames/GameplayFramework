// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "AbilitySystem/DaAbilitySet.h"
#include "DaEquipmentList.generated.h"

class UDaEquipmentManagerComponent;

struct FDaEquipmentList;

/**
 * FDaAppliedEquipmentEntry
 * One equipped item. Replicated wire state is {ItemID, ItemDefinitionID, SlotTag,
 * SpawnedActors}; GrantedHandles stays server-side only (Lyra pattern).
 */
USTRUCT(BlueprintType)
struct GAMEPLAYFRAMEWORK_API FDaAppliedEquipmentEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	// Inventory entry this equipment came from (UDaInventoryComponent::FindEntryByItemID).
	UPROPERTY(BlueprintReadOnly, Category="Equipment")
	FGuid ItemID;

	UPROPERTY(BlueprintReadOnly, Category="Equipment")
	FPrimaryAssetId ItemDefinitionID;

	// Which Equip.Slot.* this entry occupies. One entry per slot, enforced on equip.
	UPROPERTY(BlueprintReadOnly, Category="Equipment")
	FGameplayTag SlotTag;

	// Actors spawned server-side on equip; reach clients via normal actor replication.
	UPROPERTY(BlueprintReadOnly, Category="Equipment")
	TArray<TObjectPtr<AActor>> SpawnedActors;

	// Authority-only bookkeeping for revoking the granted ability set.
	UPROPERTY(NotReplicated)
	FDaAbilitySet_GrantedHandles GrantedHandles;

	// ----- FastArraySerializer callbacks -----

	void PreReplicatedRemove(const FDaEquipmentList& InArraySerializer);
	void PostReplicatedAdd(const FDaEquipmentList& InArraySerializer);
	void PostReplicatedChange(const FDaEquipmentList& InArraySerializer);
};

/**
 * FDaEquipmentList
 *
 * FFastArraySerializer wrapper owning the replicated array of applied-equipment
 * entries. Lives inside UDaEquipmentManagerComponent.
 */
USTRUCT(BlueprintType)
struct GAMEPLAYFRAMEWORK_API FDaEquipmentList : public FFastArraySerializer
{
	GENERATED_BODY()

	FDaEquipmentList()
		: OwnerComponent(nullptr)
	{
	}

	// ----- Serialisation -----

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FDaAppliedEquipmentEntry, FDaEquipmentList>(Entries, DeltaParms, *this);
	}

	// The replicated item array
	UPROPERTY()
	TArray<FDaAppliedEquipmentEntry> Entries;

	// Owning component — set during initialisation, never replicated. Deliberately not a
	// UPROPERTY: the component owns this struct, so the back-pointer needs no GC reference,
	// and UHT cannot reflect a class that is only forward-declared here.
	UDaEquipmentManagerComponent* OwnerComponent;
};

/** Enable NetDeltaSerialize for FDaEquipmentList. */
template<>
struct TStructOpsTypeTraits<FDaEquipmentList> : public TStructOpsTypeTraitsBase2<FDaEquipmentList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};
