// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "DaItemDefinition.generated.h"

class UDaAbilitySet;
class UTexture2D;
class UStaticMesh;

/**
 * UDaItemDefinition
 *
 * Static data asset that describes an item type. Designers create one of these
 * per item archetype (e.g. "Iron Sword", "Health Potion"). Runtime inventory
 * entries (FDaInventoryEntry) reference these by FPrimaryAssetId rather than
 * holding a hard object pointer, keeping replication lightweight.
 */
UCLASS(BlueprintType, Blueprintable)
class GAMEPLAYFRAMEWORK_API UDaItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	// ----- Item Identity -----

	/** Human-readable display name shown in UI. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	FText DisplayName;

	/** Longer description shown in tooltips / detail panels. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	FText Description;

	// ----- Visuals -----

	/** Icon texture for inventory UI slots. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|UI")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Static mesh used for world / preview rendering. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Visual")
	TSoftObjectPtr<UStaticMesh> DisplayMesh;

	// ----- Categorisation -----

	/** Gameplay tags that categorise this item (stackable, equipable, type, etc.). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	FGameplayTagContainer ItemTags;

	// ----- Stacking -----

	/** Maximum number of items that can share a single inventory slot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Stacking", meta=(ClampMin="1"))
	int32 MaxStackCount = 1;

	// ----- Abilities -----

	/** Ability set granted to the owner while this item is active / equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Abilities")
	TSoftObjectPtr<UDaAbilitySet> AbilitySetToGrant;

	// ----- Equipment -----

	/** Tags describing which equipment slot(s) this item can occupy. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Equipment")
	FGameplayTagContainer EquipSlotTags;

	// ----- Primary Asset Id -----

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
