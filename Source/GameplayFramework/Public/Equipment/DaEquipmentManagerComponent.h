// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "Equipment/DaEquipmentList.h"
#include "DaEquipmentManagerComponent.generated.h"

class UDaAbilitySystemComponent;
class UDaInventoryComponent;
class UDaItemDefinition;
class UGameplayAbility;
struct FDaConditionConfig;
struct FDaInventoryEntry;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEquipmentEntryEvent, const FDaAppliedEquipmentEntry&, Entry);

/** Which penalty an equipped item's Condition currently earns it (see FDaConditionConfig). */
enum class EDaConditionBand : uint8
{
	Normal,
	Worn,
	Critical,
	/** Condition 0: the item is inert. It auto-unequips and cannot be re-equipped until repaired. */
	Broken
};

/**
 * UDaEquipmentManagerComponent
 *
 * Pawn-side, server-authoritative equipment. Equipping an inventory item (by ItemID)
 * grants the item's ability set to the owner's ASC and spawns the definition's
 * ActorsToSpawn attached to the character mesh. Wire state is a FastArray of
 * FDaAppliedEquipmentEntry; GrantedHandles never replicate.
 */
UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GAMEPLAYFRAMEWORK_API UDaEquipmentManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UDaEquipmentManagerComponent();

	/** Equip the inventory item with ItemID. Empty SlotTag = first free slot the
	 *  definition's EquipSlotTags allows. Occupied slot auto-unequips first.
	 *  Routes to server when called on a client.
	 *
	 *  CAUTION on the return value off the authority: a client gets an OPTIMISTIC true the moment
	 *  the RPC is sent, which says nothing about the outcome. The server may refuse the request
	 *  outright (wrong slot, item not held, item inert) and, worse, the RPC can be absorbed before
	 *  it ever leaves — a Server_* body invoked in a context that dispatches locally (all editor
	 *  Python does, via FEditorScriptExecutionGuard) runs on the CALLER and is refused by the
	 *  authority guard in Internal_EquipItem. Treat replicated equipment state, not this bool, as
	 *  the answer. */
	UFUNCTION(BlueprintCallable, Category="Equipment")
	bool EquipItem(FGuid ItemID, FGameplayTag SlotTag);

	/** Empty SlotTag's equipment. Routes to server when called on a client — with the same
	 *  optimistic-return caveat as EquipItem above. */
	UFUNCTION(BlueprintCallable, Category="Equipment")
	bool UnequipSlot(FGameplayTag SlotTag);

	/** Authority-only: drain every occupied slot (revoke grants, destroy spawned actors).
	 *  Used when the pawn is unpossessed and again as an EndPlay backstop. */
	UFUNCTION(BlueprintCallable, Category="Equipment")
	void UnequipAll();

	/** Authority-only: give up the bindings this component holds on PlayerState-hosted objects (the
	 *  ASC's ability-activated callback). Called from ADaCharacter::UnPossessed, because the next
	 *  possession can bring a different PlayerState — and so a different ASC — and a stale binding
	 *  would wear down the outgoing player's items. ApplyLoadout re-binds on the next possess. */
	UFUNCTION(BlueprintCallable, Category="Equipment")
	void ReleaseOwnerBindings();

	/** Authority-only: equip every loadout entry whose definition is equippable
	 *  (EquipSlotTags non-empty). Consumable hotbar assignments are skipped.
	 *  Called on possess so a fresh pawn inherits the player's persistent loadout. */
	UFUNCTION(BlueprintCallable, Category="Equipment")
	void ApplyLoadout();

	UFUNCTION(BlueprintPure, Category="Equipment")
	FGuid GetEquippedItemID(FGameplayTag SlotTag) const;

	UFUNCTION(BlueprintPure, Category="Equipment")
	bool IsItemEquipped(FGuid ItemID) const;

	/** Server-only: which inventory item granted this ability spec (invalid Guid on clients/miss). */
	FGuid GetItemIDForAbility(FGameplayAbilitySpecHandle Handle) const;

	/** Which equipped item spawned this actor (invalid Guid when none did). Reads the REPLICATED
	 *  entry list, so a spawned cosmetic can identify the item it represents on clients too —
	 *  which is how UDaConditionComponent finds its item without being told. */
	UFUNCTION(BlueprintPure, Category="Equipment")
	FGuid FindItemIDForSpawnedActor(const AActor* SpawnedActor) const;

	UFUNCTION(BlueprintCallable, Category="Equipment", meta=(DefaultToSelf="Actor"))
	static UDaEquipmentManagerComponent* GetEquipmentFromActor(AActor* Actor);

	/** Condition (absolute) against the config's percentage thresholds of the grade-derived cap.
	 *  Public and static because it is pure arithmetic over replicated numbers: a client-side
	 *  reader (the item debug overlay, a shop or repair UI) can band an item it does not wear
	 *  without the authority's penalty bookkeeping, and shares this function so its answer can
	 *  never drift from the one the penalties are applied from. */
	static EDaConditionBand ComputeConditionBand(const FDaConditionConfig& Config, int32 Condition, int32 Grade);

	// Called by FastArray callbacks on clients AND inline on authority.
	void HandleEquipped(const FDaAppliedEquipmentEntry& Entry);
	void HandleUnequipped(const FDaAppliedEquipmentEntry& Entry);

	/** Called from FDaAppliedEquipmentEntry::PostReplicatedChange on clients. An entry whose
	 *  SpawnedActors references were unmapped when it first arrived resolves them through this
	 *  path, so listeners that need the actors get a second chance at them. */
	void HandleChanged(const FDaAppliedEquipmentEntry& Entry);

	UPROPERTY(BlueprintAssignable, Category="Equipment")
	FOnEquipmentEntryEvent OnEquipped;

	UPROPERTY(BlueprintAssignable, Category="Equipment")
	FOnEquipmentEntryEvent OnUnequipped;

	/** An already-equipped entry changed on the wire (e.g. its SpawnedActors finally resolved). */
	UPROPERTY(BlueprintAssignable, Category="Equipment")
	FOnEquipmentEntryEvent OnEquipmentChanged;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated)
	FDaEquipmentList EquipmentList;

private:

	UFUNCTION(Server, Reliable)
	void Server_EquipItem(FGuid ItemID, FGameplayTag SlotTag);

	UFUNCTION(Server, Reliable)
	void Server_UnequipSlot(FGameplayTag SlotTag);

	bool Internal_EquipItem(const FGuid& ItemID, FGameplayTag SlotTag);
	bool Internal_UnequipSlot(FGameplayTag SlotTag);

	/** Which slot holds ItemID, or an invalid tag. */
	FGameplayTag FindSlotForItem(const FGuid& ItemID) const;

	/** Authority-only, idempotent: subscribe to the resolved inventory's removal and change
	 *  delegates so an item that leaves the inventory cannot stay equipped and a Condition write
	 *  re-evaluates the wearer's penalties. The inventory lives on the PlayerState, which is not
	 *  always assigned by BeginPlay, so ApplyLoadout retries. */
	void EnsureInventoryBinding();

	/** Bound to UDaInventoryComponent::OnEntryRemoved on the authority. */
	UFUNCTION()
	void OnInventoryEntryRemoved(const FDaInventoryEntry& Entry, int32 SlotIndex);

	/** Bound to UDaInventoryComponent::OnEntryChanged on the authority: a stat write on an
	 *  equipped condition-user may have moved it into another Condition band. */
	UFUNCTION()
	void OnInventoryEntryChanged(const FDaInventoryEntry& Entry, int32 SlotIndex);

	/** Authority-only, idempotent: subscribe to the owner ASC's ability-activated callback so
	 *  using an item's ability wears that item down. The ASC lives on the PlayerState, which
	 *  BeginPlay can run without, so the equip path retries. */
	void EnsureAbilityDecayBinding();

	/** Bound to UAbilitySystemComponent::AbilityActivatedCallbacks on the authority: one use of
	 *  an item-granted ability costs that item DecayPerUse Condition. */
	void OnAbilityActivated(UGameplayAbility* Ability);

	/** Authority-only: re-evaluate the Condition band of the item in SlotTag and make the ASC
	 *  match it — swap in the band's penalty effect, or unequip the slot outright when the item
	 *  has broken (Condition 0). Cheap and idempotent, so the equip path and every entry-changed
	 *  broadcast can both call it. */
	void RefreshConditionPenalty(FGameplayTag SlotTag);

	/** Authority-only: drop the penalty effect tracked for SlotTag, if any. Keeps the handle (and
	 *  warns) when no ASC can be resolved, so a penalty can never be stranded un-removable. */
	void ClearConditionPenalty(FGameplayTag SlotTag);

	/** Next-tick half of the Broken path: RefreshConditionPenalty schedules this instead of
	 *  unequipping inline, because the decay that broke the item runs inside the activating
	 *  ability's PreActivate and the weapon actor is that ability's SourceObject. Re-checks that the
	 *  slot still holds the same broken item before tearing anything down (a repair may have landed
	 *  in the intervening tick). */
	void HandleDeferredBreak(FGameplayTag SlotTag);

	/** PlayerState (preferred) or owner inventory component. */
	UDaInventoryComponent* ResolveInventory() const;
	UDaAbilitySystemComponent* ResolveASC() const;
	UDaItemDefinition* ResolveItemDefinition(FPrimaryAssetId ItemDefinitionID) const;

	/** Server-only: ability spec handle -> granting item. */
	TMap<FGameplayAbilitySpecHandle, FGuid> AbilityToItemMap;

	/** Server-only: the penalty effect currently applied for each occupied slot, and the band it
	 *  represents. Keyed by slot rather than by item so a swap into the same slot cannot leak the
	 *  previous item's penalty. Normal-band slots hold no entry in either map. */
	TMap<FGameplayTag, FActiveGameplayEffectHandle> ConditionPenaltyHandles;
	TMap<FGameplayTag, EDaConditionBand> ConditionBands;

	/** Slots with a break teardown already scheduled for the next tick, so a burst of decay writes
	 *  in one frame queues one teardown rather than one per write. */
	TSet<FGameplayTag> PendingBreakSlots;

	/** Inventory whose OnEntryRemoved this component is currently bound to (authority only). */
	TWeakObjectPtr<UDaInventoryComponent> BoundInventory;

	/** ASC whose AbilityActivatedCallbacks this component is currently bound to (authority only). */
	TWeakObjectPtr<UDaAbilitySystemComponent> BoundASC;
	FDelegateHandle AbilityActivatedHandle;
};
