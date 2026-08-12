// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "Inventory/DaInventoryList.h"
#include "DaInventoryComponent.generated.h"

struct FDaInventoryEntry;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInventoryEntryEvent, const FDaInventoryEntry&, Entry, int32, SlotIndex);

/**
 * Deliberately parameterless: "the loadout changed, re-read it".
 * Replication gives us the array AFTER the fact, so OnRep_Loadout cannot say which slot moved
 * without diffing against a shadow copy — and a delegate whose payload depends on whether it
 * fired on the server or on a client is worse than no payload at all. Listeners call
 * GetLoadout / GetLoadoutItemID.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoadoutChanged);

/**
 * FDaLoadoutEntry
 * One loadout assignment: which inventory item the player wants in which Equip.Slot.*.
 * An assignment is intent, not state — the pawn's UDaEquipmentManagerComponent turns it
 * into equipped state on possess (see UDaEquipmentManagerComponent::ApplyLoadout).
 */
USTRUCT(BlueprintType)
struct GAMEPLAYFRAMEWORK_API FDaLoadoutEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Inventory|Loadout")
	FGameplayTag SlotTag;

	UPROPERTY(BlueprintReadOnly, Category="Inventory|Loadout")
	FGuid ItemID;
};

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

	/**
	 * Use the item at SlotIndex. Server-authoritative: sends an Action.UseItem gameplay
	 * event to the owner's ability system (OptionalObject = UDaItemDefinition,
	 * EventMagnitude = SlotIndex), consumes one from the stack when the definition has
	 * bConsumeOnUse, and notifies the owning client through OnItemUsed.
	 * Routes to server if called on client.
	 */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool UseItem(int32 SlotIndex);

	/**
	 * Drop Count items from SlotIndex into the world. Server-authoritative: removes them
	 * from the inventory and spawns the definition's PickupActorClass (ADaItemActor by
	 * default) in front of the owner's pawn so it can be picked back up.
	 * Count=0 drops the entire stack. Routes to server if called on client.
	 */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	bool DropItem(int32 SlotIndex, int32 Count = 1);

	/** Returns a copy of all inventory entries. */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	TArray<FDaInventoryEntry> GetAllEntries() const;

	/**
	 * Blueprint-compatibility bridge: build UI view-models for the current entries.
	 * Existing inventory widgets were authored against the pre-FastArray component,
	 * which exposed GetItems() returning UDaInventoryItemBase*. This rebuilds those
	 * transient view-models on demand from the FastArray so those widgets keep working.
	 * Prefer binding UMG to UDaInventoryWidgetController for new work.
	 * BlueprintPure: legacy widgets call this as a data source without wiring
	 * exec pins, and impure unconnected nodes get pruned by the compiler.
	 */
	UFUNCTION(BlueprintPure, Category="Inventory")
	TArray<class UDaInventoryItemBase*> GetItems();

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

	// ----- Per-instance stats (see FDaInventoryEntry::StatTags) -----

	/** Find the entry with the given ItemID, or nullptr. */
	const FDaInventoryEntry* FindEntryByItemID(const FGuid& ItemID) const;

	/** Set a per-instance stat to an absolute value (0 removes it). Routes to server.
	 *  StatTag must be under Item.Stat; negative counts are clamped to 0. */
	UFUNCTION(BlueprintCallable, Category="Inventory|Stats")
	bool SetItemStat(FGuid ItemID, FGameplayTag StatTag, int32 Count);

	/** Add Delta to a per-instance stat. Routes the DELTA to the server, which does the
	 *  read-modify-write against its own value. Same tag/clamp rules as SetItemStat. */
	UFUNCTION(BlueprintCallable, Category="Inventory|Stats")
	bool AddItemStat(FGuid ItemID, FGameplayTag StatTag, int32 Delta);

	/** Read a per-instance stat; 0 when the item or stat is absent. */
	UFUNCTION(BlueprintPure, Category="Inventory|Stats")
	int32 GetItemStat(FGuid ItemID, FGameplayTag StatTag) const;

	/** Deterministic per-instance seed derived from the ItemID (stable across save/load). */
	UFUNCTION(BlueprintPure, Category="Inventory|Stats")
	static int32 GetItemSeed(FGuid ItemID);

	// ----- Condition repair (the credits sink; see FDaConditionConfig) -----

	/**
	 * Spend the owning player's credits to raise this item's Item.Stat.Condition.
	 * Points = 0 buys as many points as the player can afford, up to the item's grade-derived cap;
	 * an explicit Points is clamped to what is missing and is REJECTED outright when the player
	 * cannot afford all of it (a partial charge for an explicit order would be a surprise).
	 * Cost = max(0, ceil(Points * RepairCreditsPerPoint * (1 + Grade * RepairGradeCostScale) - 1e-3)),
	 * charged against ADaPlayerState::AdjustCredits. That thousandth-of-a-credit slack is not
	 * cosmetic: the curve is authored as floats, so a price whose exact arithmetic is a whole number
	 * of credits usually is not (grade 7 at the shipped defaults gives 3.40000002086... per point),
	 * and a plain ceiling would charge one credit more than the content author priced.
	 * Grade is provenance and never changes.
	 * Routes to server if called on client (optimistic true; the server validates and may refuse —
	 * and see EquipItem's caveat: a Server_* body dispatched locally never reaches the authority at
	 * all, which is why the python MP harness cannot verify a client-issued repair).
	 */
	UFUNCTION(BlueprintCallable, Category="Inventory|Condition")
	bool RepairItem(FGuid ItemID, int32 Points = 0);

	/**
	 * Credit cost of buying Points condition points for this item, 0 when the item cannot be
	 * repaired (unknown, not a condition user) or Points <= 0. UI-facing: same arithmetic the
	 * server charges — including the clamp to what is actually missing, so quoting 100 points on an
	 * 8-point gap prices the 8 the till will charge for.
	 */
	UFUNCTION(BlueprintPure, Category="Inventory|Condition")
	int32 GetRepairCost(FGuid ItemID, int32 Points) const;

	/** How many condition points this item is missing (cap - current); 0 when it is at the cap
	 *  or does not use condition. */
	UFUNCTION(BlueprintPure, Category="Inventory|Condition")
	int32 GetMissingCondition(FGuid ItemID) const;

	// ----- Loadout (which item the player wants in which Equip.Slot.*) -----
	// Lives here (PlayerState side) so it survives pawn death and rides the save file.
	// The pawn's UDaEquipmentManagerComponent applies it on possess.

	/** Assign ItemID to SlotTag (invalid ItemID clears the slot). Routes to server. */
	UFUNCTION(BlueprintCallable, Category="Inventory|Loadout")
	bool SetLoadoutSlot(FGameplayTag SlotTag, FGuid ItemID);

	/** Which item is assigned to SlotTag; invalid Guid when the slot is unassigned. */
	UFUNCTION(BlueprintPure, Category="Inventory|Loadout")
	FGuid GetLoadoutItemID(FGameplayTag SlotTag) const;

	/** Copy of the whole loadout as slot -> item. */
	UFUNCTION(BlueprintPure, Category="Inventory|Loadout")
	TMap<FGameplayTag, FGuid> GetLoadout() const;

	// ----- Persistence (BlueprintCallable) -----

	/** Returns a copy of entries for save serialization. */
	UFUNCTION(BlueprintCallable, Category="Inventory|Persistence")
	TArray<FDaInventoryEntry> SaveInventory() const;

	/** Server-only. Populates the inventory from previously saved data. Entries whose definition uses
	 *  condition but which carry no Item.Stat.Condition (a save written before that item type opted
	 *  in) are filled to their grade cap on the way in — otherwise they would read as Broken and
	 *  could never be equipped again. */
	UFUNCTION(BlueprintCallable, Category="Inventory|Persistence")
	void LoadInventory(const TArray<FDaInventoryEntry>& SavedEntries);

	/**
	 * Server-only. Put a single previously-removed entry back, keeping it the SAME instance:
	 * ItemID, StatTags, Tags, AbilitySetID and StackCount are copied verbatim (LoadInventory's
	 * discipline, one entry at a time). Used by a dropped ADaItemActor on re-pickup.
	 * Fails when the entry has no ItemID, when this inventory already holds that ItemID
	 * (an instance cannot exist twice), or when there is no free slot. The entry lands in its
	 * saved SlotIndex if that slot is free, otherwise in the first free slot.
	 */
	UFUNCTION(BlueprintCallable, Category="Inventory|Persistence")
	bool RestoreEntry(const FDaInventoryEntry& SavedEntry);

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

	/** Fires on the owning client (and standalone/listen host) after an item is successfully used. */
	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FOnInventoryEntryEvent OnItemUsed;

	/** Fires on the owning client (and standalone/listen host) after an item is successfully dropped. */
	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FOnInventoryEntryEvent OnItemDropped;

	/**
	 * The loadout changed: a slot was assigned, reassigned or cleared. Fires on the authority the
	 * moment the mutation lands AND on every client from OnRep_Loadout, so a hotbar built on it
	 * shows the same thing in both places. Only real changes broadcast — clearing an already
	 * empty slot is accepted but silent.
	 */
	UPROPERTY(BlueprintAssignable, Category="Inventory|Loadout")
	FOnLoadoutChanged OnLoadoutChanged;

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

	/** Slot assignments, replicated as an array because a TMap cannot replicate. */
	UPROPERTY(ReplicatedUsing=OnRep_Loadout)
	TArray<FDaLoadoutEntry> Loadout;

	/** Client-side half of OnLoadoutChanged. */
	UFUNCTION()
	void OnRep_Loadout();

private:

	// ----- Server RPCs -----

	UFUNCTION(Server, Reliable)
	void Server_AddItem(FPrimaryAssetId ItemDefinitionID, int32 StackCount, int32 SlotHint);

	UFUNCTION(Server, Reliable)
	void Server_RemoveItem(int32 SlotIndex, int32 Count);

	UFUNCTION(Server, Reliable)
	void Server_MoveItem(int32 FromSlot, int32 ToSlot);

	UFUNCTION(Server, Reliable)
	void Server_UseItem(int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void Server_DropItem(int32 SlotIndex, int32 Count);

	UFUNCTION(Server, Reliable)
	void Server_SetItemStat(FGuid ItemID, FGameplayTag StatTag, int32 Count);

	/** Delta form: the server does the read-modify-write, so two clients adjusting the same
	 *  stat cannot clobber each other with stale absolute values. */
	UFUNCTION(Server, Reliable)
	void Server_AddItemStat(FGuid ItemID, FGameplayTag StatTag, int32 Delta);

	UFUNCTION(Server, Reliable)
	void Server_SetLoadoutSlot(FGameplayTag SlotTag, FGuid ItemID);

	UFUNCTION(Server, Reliable)
	void Server_RepairItem(FGuid ItemID, int32 Points);

	// ----- Client notifications (run on the owning client; locally on standalone/listen host) -----

	UFUNCTION(Client, Reliable)
	void Client_NotifyItemUsed(FDaInventoryEntry Entry);

	UFUNCTION(Client, Reliable)
	void Client_NotifyItemDropped(FDaInventoryEntry Entry);

	// ----- Internal helpers -----

	/** Server-only logic: loads definition, finds or creates slot, adds or stacks entry. */
	bool Internal_AddItem(FPrimaryAssetId ItemDefinitionID, int32 StackCount, int32 SlotHint);

	/** Server-only logic: validate entry, fire gameplay event, consume stack, notify client. */
	bool Internal_UseItem(int32 SlotIndex);

	/** Server-only logic: remove from inventory and spawn a world pickup actor. */
	bool Internal_DropItem(int32 SlotIndex, int32 Count);

	/** Server-only: find entry, mutate stat, mark dirty, broadcast changed. */
	bool Internal_SetItemStat(const FGuid& ItemID, FGameplayTag StatTag, int32 Count);

	/** Stamp Grade + full Condition directly onto an entry that has NOT been published yet, so it is
	 *  coherent the first time any listener sees it. This is the acquisition path's form. */
	static void InitializeConditionStats(FDaInventoryEntry& Entry, const class UDaItemDefinition& Def);

	/** Server-only: same, for an entry already in the list — the writes go through the stat path so
	 *  each marks the entry dirty and broadcasts. Used by the LoadInventory migration; NOT by
	 *  RestoreEntry, whose snapshot carries its own stats (re-initialising would repair for free). */
	void InitializeConditionStats(const FGuid& ItemID, const class UDaItemDefinition& Def);

	/** May a CLIENT write this Item.Stat leaf through Server_SetItemStat / Server_AddItemStat?
	 *  False for the protected leaves (Grade — permanent provenance that fixes both the condition
	 *  cap and the repair price). Authority-side callers bypass the RPCs and are unaffected. */
	static bool IsClientWritableStat(FGameplayTag StatTag);

	/** Server-only: validate, charge the owner's credits, then raise Condition through the stat
	 *  path (so the equipment manager's threshold watcher clears penalty bands on its own). */
	bool Internal_RepairItem(const FGuid& ItemID, int32 Points);

	/** Definition of the item with this ItemID, but only when the item is in this inventory AND
	 *  its definition opts into condition; nullptr otherwise. The one gate every condition
	 *  query and the repair path share, so they can never disagree about what is repairable. */
	const class UDaItemDefinition* ResolveConditionDefinition(const FGuid& ItemID) const;

	/** The ADaPlayerState whose credits pay for repairs: our owner when the inventory lives on a
	 *  PlayerState (the normal case), else the owning pawn's PlayerState. */
	class ADaPlayerState* ResolveOwnerPlayerState() const;

	/** Server-only: assign, reassign or clear one loadout slot. */
	bool Internal_SetLoadoutSlot(FGameplayTag SlotTag, const FGuid& ItemID);

	/** Server-only: drop every loadout assignment naming ItemID. Called when the item leaves
	 *  the inventory, so an assignment can never outlive its item. */
	void ClearLoadoutForItem(const FGuid& ItemID);

	/** Resolve and (if needed) synchronously load the item definition for an ID. */
	class UDaItemDefinition* ResolveItemDefinition(FPrimaryAssetId ItemDefinitionID) const;

	/** Resolve the pawn that represents this inventory's owner in the world (owner itself or PlayerState's pawn). */
	AActor* ResolveOwnerAvatar() const;
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
