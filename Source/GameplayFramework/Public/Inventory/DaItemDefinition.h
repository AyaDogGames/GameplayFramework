// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "DaItemDefinition.generated.h"

class AActor;
class ADaItemActor;
class UDaAbilitySet;
class UGameplayEffect;
class UTexture2D;
class UStaticMesh;

/**
 * FDaEquipmentActorToSpawn
 * One actor to spawn+attach while an item is equipped. The spawned actor is THE
 * Blueprint extension point for per-equip behavior (visuals, anim, audio).
 */
USTRUCT(BlueprintType)
struct GAMEPLAYFRAMEWORK_API FDaEquipmentActorToSpawn
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category="Equipment")
	TSoftClassPtr<AActor> ActorToSpawn;

	UPROPERTY(EditDefaultsOnly, Category="Equipment")
	FName AttachSocket;

	UPROPERTY(EditDefaultsOnly, Category="Equipment")
	FTransform AttachTransform;
};

/**
 * FDaConditionConfig
 *
 * Opt-in wear model for one item type. Disabled by default, so existing content is
 * untouched: an item only gains an Item.Stat.Condition (and only ever decays) when its
 * definition sets bUsesCondition.
 *
 * Condition is stored as a StatTags count on the inventory entry, which makes it an int32
 * (the spec's float DecayPerUse ships as an integer for that reason). It runs from 0 to a
 * grade-derived cap; Grade itself is permanent provenance and is never mutated here.
 * All numbers are tunable content defaults, not contract.
 */
USTRUCT(BlueprintType)
struct GAMEPLAYFRAMEWORK_API FDaConditionConfig
{
	GENERATED_BODY()

	/** Master switch. Items with this off never gain, decay or repair Condition. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition")
	bool bUsesCondition = false;

	/** Condition lost per ability use (see UDaEquipmentManagerComponent's decay hook). 0 disables decay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition", meta=(ClampMin="0"))
	int32 DecayPerUse = 1;

	/** Cap = CapBase + CapPerGrade * Grade (grade 10 -> 100 with the shipped defaults). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition")
	int32 CapBase = 60;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition")
	int32 CapPerGrade = 4;

	/** Grade stamped at acquisition when nothing else set Item.Stat.Grade on the instance
	 *  (a spawner or loot table that seeds Grade before the item is added wins over this). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition", meta=(ClampMin="0", ClampMax="10"))
	int32 DefaultGrade = 5;

	/** Percent of the cap below which WornEffect applies. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition", meta=(ClampMin="0", ClampMax="100"))
	int32 WornThresholdPct = 50;

	/** Percent of the cap below which CriticalEffect applies (replacing WornEffect). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition", meta=(ClampMin="0", ClampMax="100"))
	int32 CriticalThresholdPct = 25;

	/** Penalty effects for the two worn bands. Defaults are set with the threshold watcher. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition")
	TSubclassOf<UGameplayEffect> WornEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition")
	TSubclassOf<UGameplayEffect> CriticalEffect;

	/** Repair cost = Points * RepairCreditsPerPoint * (1 + Grade * RepairGradeCostScale). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition", meta=(ClampMin="0"))
	float RepairCreditsPerPoint = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Condition", meta=(ClampMin="0"))
	float RepairGradeCostScale = 0.1f;

	/** Highest Condition this instance can hold, given its permanent Grade. */
	int32 GetConditionCap(int32 Grade) const
	{
		return CapBase + CapPerGrade * FMath::Clamp(Grade, 0, 10);
	}
};

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

	// ----- Usage -----

	/** If true, using this item consumes one from the stack (removing the entry when it hits zero). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Usage")
	bool bConsumeOnUse = false;

	// ----- Dropping -----

	/** Actor class spawned when this item is dropped from an inventory. Falls back to ADaItemActor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Dropping")
	TSoftClassPtr<ADaItemActor> PickupActorClass;

	// ----- Equipment -----

	/** Tags describing which equipment slot(s) this item can occupy. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Equipment")
	FGameplayTagContainer EquipSlotTags;

	// ----- Condition -----

	/** Wear model for this item type (off by default; see FDaConditionConfig). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Condition")
	FDaConditionConfig ConditionConfig;

	// ----- Equipment actors -----

	/** Actor spawned and attached while this item is equipped (weapon mesh, effect rig, ...). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Equipment")
	TArray<FDaEquipmentActorToSpawn> ActorsToSpawn;

	// ----- Primary Asset Id -----

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
