// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "DaInteractableInterface.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "Inventory/DaInventoryEntry.h"
#include "Inventory/DaInventoryItemInterface.h"
#include "DaItemActor.generated.h"

class UDaAbilitySystemComponent;
class UDaAbilitySet;
class USphereComponent;

UCLASS(BlueprintType, Blueprintable)
class GAMEPLAYFRAMEWORK_API ADaItemActor : public AActor, public IDaInteractableInterface, public IAbilitySystemInterface, public IDaInventoryItemInterface
{
	GENERATED_BODY()

public:
	ADaItemActor();

	// IDaInteractableInterface
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual void SecondaryInteract_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractText_Implementation(APawn* InstigatorPawn) override;
	virtual void HighlightActor_Implementation() override;
	virtual void UnHighlightActor_Implementation() override;

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// IDaInventoryItemInterface
	virtual FPrimaryAssetId GetItemDefinitionID_Implementation() const override { return ItemDefinitionID; }
	virtual int32 GetStackCount_Implementation() const override { return 1; }
	virtual void AddToInventory_Implementation(APawn* InstigatorPawn, bool bDestroyActor = true) override;

	void GrantSetToActor(UDaAbilitySystemComponent* ReceivingASC);

	/** Set up an actor spawned for a dropped inventory item (server, before FinishSpawning). */
	void InitializeDroppedItem(const FPrimaryAssetId& InItemDefinitionID, UStaticMesh* DisplayMesh);

	/**
	 * Same, for a drop that must survive as the SAME item instance: keeps a snapshot of the entry
	 * that left the inventory, so picking this actor back up restores its ItemID and stats instead
	 * of minting a new instance. Also derives the wear the snapshot describes and pushes it into the
	 * pickup's visual — the entry is gone from the inventory by now, so this is the only moment those
	 * numbers are available, and a worn item has to hit the ground looking worn.
	 * The snapshot is spent on the first successful restore (see bHasDroppedSnapshot).
	 */
	void InitializeDroppedItem(const FDaInventoryEntry& SourceEntry, UStaticMesh* DisplayMesh);

protected:

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Components")
	TObjectPtr<USphereComponent> SphereComp;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Components")
	TObjectPtr<UDaAbilitySystemComponent> AbilitySystemComponent;

	// The item definition this actor represents in the inventory system
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory")
	FPrimaryAssetId ItemDefinitionID;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "InventoryItems")
	FName Name;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "InventoryItems")
	FName Description;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "InventoryItems")
	FGameplayTagContainer TypeTags;

	UPROPERTY(EditDefaultsOnly, Category="InventoryItems")
	TObjectPtr<UDaAbilitySet> OwnedAbilitySet;

	UPROPERTY(BlueprintReadOnly, Category="InventoryItems")
	bool bHighlighted = false;

	/**
	 * The inventory entry this actor was dropped from, kept verbatim so a re-pickup restores the
	 * same instance. Server-only by design: NOT a UPROPERTY, so it neither replicates nor
	 * serializes. A client's cosmetic copy of the actor therefore has no snapshot, and a
	 * hand-placed pickup never has one either — both take the definition-based AddItem path.
	 */
	FDaInventoryEntry DroppedEntrySnapshot;

	/**
	 * True while the snapshot above is still unspent. Cleared the instant RestoreEntry succeeds, so
	 * one dropped actor can only ever restore its instance ONCE — the window between a successful
	 * restore and the end-of-frame Destroy is otherwise long enough for a second interact (the same
	 * player double-tapping, or a second player racing the first) to reach the AddItem fallback and
	 * mint a pristine duplicate.
	 */
	bool bHasDroppedSnapshot = false;

	/** Set once this actor has handed its item over and is waiting to be destroyed. */
	bool bPickedUp = false;

	/** Wear contract values derived from the drop snapshot, pushed into whatever drives this
	 *  pickup's materials. See UDaConditionComponent and docs/ConditionMaterialContract.md. */
	float DroppedWearIntensity = 0.f;
	float DroppedWearSeed = 0.f;
	float DroppedWearGrade = 0.f;
	bool bHasDroppedWear = false;

	/** Push the derived wear into a UDaConditionComponent if this pickup class has one, else
	 *  straight into the display mesh's contract-implementing material slots. Idempotent: called
	 *  from InitializeDroppedItem (where components may not be registered yet) and again from
	 *  BeginPlay. */
	void ApplyDroppedWear();

	/** Item definition for an id, via the asset manager (loaded first, synchronous load second). */
	static const class UDaItemDefinition* ResolveItemDefinition(const FPrimaryAssetId& InItemDefinitionID);

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
};
