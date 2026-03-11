// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "Inventory/DaInventoryList.h"
#include "DaInventoryComponent.generated.h"

struct FDaInventoryEntry;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInventoryEntryEvent, const FDaInventoryEntry&, Entry, int32, SlotIndex);

UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GAMEPLAYFRAMEWORK_API UDaInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UDaInventoryComponent();

	// ----- Public API (BlueprintCallable) -----

	/** Main entry point for adding an item. Routes to server if called on client. */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool AddItem(FPrimaryAssetId ItemDefinitionID, int32 StackCount = 1, int32 SlotHint = -1);

	/** Remove item at SlotIndex. Count=0 removes entire stack. Routes to server if called on client. */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool RemoveItem(int32 SlotIndex, int32 Count = 0);

	/** Swap or move an item from one slot to another. Routes to server if called on client. */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool MoveItem(int32 FromSlot, int32 ToSlot);

	/** Returns a copy of all inventory entries. */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	TArray<FDaInventoryEntry> GetAllEntries() const;

	/** Returns a read-only pointer to the entry at the given slot, or nullptr if empty. */
	const FDaInventoryEntry* GetEntryAtSlot(int32 SlotIndex) const;

	/** Returns true if the given slot has no entry. */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool IsSlotEmpty(int32 SlotIndex) const;

	/** Returns the number of occupied slots. */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	int32 GetFilledSlotCount() const;

	/** Returns the maximum number of inventory slots. */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	int32 GetMaxSlots() const;

	// ----- Persistence (BlueprintCallable) -----

	/** Returns a copy of entries for save serialization. */
	UFUNCTION(BlueprintCallable, Category="Inventory|Persistence")
	TArray<FDaInventoryEntry> SaveInventory() const;

	/** Server-only. Populates the inventory from previously saved data. */
	UFUNCTION(BlueprintCallable, Category="Inventory|Persistence")
	void LoadInventory(const TArray<FDaInventoryEntry>& SavedEntries);

	// ----- Static helpers -----

	/** Find the inventory component on the given actor. */
	UFUNCTION(BlueprintCallable, Category="Inventory", meta=(DefaultToSelf="Actor"))
	static UDaInventoryComponent* GetInventoryFromActor(AActor* Actor);

	// ----- Delegates -----

	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FOnInventoryEntryEvent OnEntryAdded;

	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FOnInventoryEntryEvent OnEntryRemoved;

	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FOnInventoryEntryEvent OnEntryChanged;

	// ----- Internal callbacks (called by FDaInventoryEntry FastArray callbacks) -----

	void OnEntryAddedInternal(const FDaInventoryEntry& Entry);
	void OnEntryRemovedInternal(const FDaInventoryEntry& Entry);
	void OnEntryChangedInternal(const FDaInventoryEntry& Entry);

protected:

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	// ----- Replicated properties -----

	/** The FastArray serializer that owns all inventory entries. */
	UPROPERTY(Replicated)
	FDaInventoryList InventoryList;

	/** Maximum number of inventory slots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Inventory")
	int32 MaxSlots = 20;

	/** Tags describing this inventory itself (e.g. equipment, backpack, hotbar). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Inventory")
	FGameplayTagContainer InventoryTags;

private:

	// ----- Server RPCs -----

	UFUNCTION(Server, Reliable)
	void Server_AddItem(FPrimaryAssetId ItemDefinitionID, int32 StackCount, int32 SlotHint);

	UFUNCTION(Server, Reliable)
	void Server_RemoveItem(int32 SlotIndex, int32 Count);

	UFUNCTION(Server, Reliable)
	void Server_MoveItem(int32 FromSlot, int32 ToSlot);

	// ----- Internal helpers -----

	/** Server-only logic: loads definition, finds or creates slot, adds or stacks entry. */
	bool Internal_AddItem(FPrimaryAssetId ItemDefinitionID, int32 StackCount, int32 SlotHint);
};

/**
 * UDaMasterInventory
 *
 * Empty subclass used by Blueprints for a "master" inventory variant.
 */
UCLASS(Blueprintable)
class GAMEPLAYFRAMEWORK_API UDaMasterInventory : public UDaInventoryComponent
{
	GENERATED_BODY()
};
