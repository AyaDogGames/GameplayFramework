// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "Equipment/DaEquipmentList.h"
#include "DaEquipmentManagerComponent.generated.h"

class UDaAbilitySystemComponent;
class UDaInventoryComponent;
class UDaItemDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEquipmentEntryEvent, const FDaAppliedEquipmentEntry&, Entry);

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
	 *  Routes to server when called on a client. */
	UFUNCTION(BlueprintCallable, Category="Equipment")
	bool EquipItem(FGuid ItemID, FGameplayTag SlotTag);

	UFUNCTION(BlueprintCallable, Category="Equipment")
	bool UnequipSlot(FGameplayTag SlotTag);

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

	UFUNCTION(BlueprintCallable, Category="Equipment", meta=(DefaultToSelf="Actor"))
	static UDaEquipmentManagerComponent* GetEquipmentFromActor(AActor* Actor);

	// Called by FastArray callbacks on clients AND inline on authority.
	void HandleEquipped(const FDaAppliedEquipmentEntry& Entry);
	void HandleUnequipped(const FDaAppliedEquipmentEntry& Entry);

	UPROPERTY(BlueprintAssignable, Category="Equipment")
	FOnEquipmentEntryEvent OnEquipped;

	UPROPERTY(BlueprintAssignable, Category="Equipment")
	FOnEquipmentEntryEvent OnUnequipped;

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

	/** PlayerState (preferred) or owner inventory component. */
	UDaInventoryComponent* ResolveInventory() const;
	UDaAbilitySystemComponent* ResolveASC() const;
	UDaItemDefinition* ResolveItemDefinition(FPrimaryAssetId ItemDefinitionID) const;

	/** Server-only: ability spec handle -> granting item. */
	TMap<FGameplayAbilitySpecHandle, FGuid> AbilityToItemMap;
};
