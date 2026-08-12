// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Inventory/DaConditionBand.h"
#include "DaInventoryItemBase.generated.h"

class UDaAbilitySet;
class UDaInventoryComponent;
class UDaItemDefinition;
class UTexture2D;
class USlateBrushAsset;
struct FDaInventoryEntry;

/**
 * FDaInventoryItemData
 *
 * Plain snapshot of a UI item view-model. Retained for Blueprint compatibility
 * (ToData / PopulateWithData). Runtime, authoritative inventory state lives in the
 * FastArray (FDaInventoryEntry); this struct is a convenience copy for UI/BP code.
 */
USTRUCT(BlueprintType)
struct FDaInventoryItemData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName ItemName = FName();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName ItemDescription = FName();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGuid ItemID = FGuid();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<class UDaInventoryItemBase> ItemClass = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTagContainer Tags = FGameplayTagContainer();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Icon = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UDaAbilitySet> AbilitySetToGrant = nullptr;

	UPROPERTY(BlueprintReadOnly)
	int32 ItemCount = 1;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryItemDataRemoved, const FDaInventoryItemData&, itemData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryItemUpdated, UDaInventoryItemBase*, item);

/**
 * UDaInventoryItemBase
 *
 * Transient, client-side UI view-model for a single inventory slot. It wraps one
 * FDaInventoryEntry (the replicated FastArray record) together with its resolved
 * UDaItemDefinition, and exposes the fields UMG widgets (UDaInventoryUIWidget list
 * entries) bind to. These objects are created on demand by
 * UDaInventoryWidgetController — they are NOT replicated and hold no authoritative
 * gameplay state; the FastArray inventory (UDaInventoryComponent) is the source of
 * truth.
 */
UCLASS(Blueprintable, BlueprintType)
class GAMEPLAYFRAMEWORK_API UDaInventoryItemBase : public UObject
{
	GENERATED_BODY()

public:

	UDaInventoryItemBase();

	// ----- Factories -----

	/** Build a view-model from a FastArray entry (resolving its item definition). */
	static UDaInventoryItemBase* CreateFromEntry(const FDaInventoryEntry& Entry, UObject* Outer);

	/** Legacy convenience factory kept for Blueprint compatibility. */
	static UDaInventoryItemBase* CreateFromData(const FDaInventoryItemData& Data);

	// ----- View-model fields (bound by UMG / Blueprints) -----

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	bool bIsEmptySlot = true;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FName Name;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FName Description;

	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FGuid ItemID;

	/** Which inventory slot this view-model represents. */
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	int32 SlotIndex = INDEX_NONE;

	/** Stack count copied from the backing entry. */
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	int32 StackCount = 1;

	/** Primary asset id of the backing UDaItemDefinition. */
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	FPrimaryAssetId ItemDefinitionID;

	/** Icon for the slot, resolved from the item definition. */
	UPROPERTY(BlueprintReadOnly, Category="Inventory|UI")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Legacy Slate brush thumbnail, kept for Blueprint image-binding compatibility. */
	UPROPERTY(BlueprintReadOnly, Category="Inventory|UI")
	TObjectPtr<USlateBrushAsset> ThumbnailBrush;

	// ----- Per-instance state (FDaInventoryEntry::StatTags, mirrored for UI) -----
	// A view-model that showed only the definition's static data could not draw a worn sword
	// differently from a mint one, which is the whole point of the condition system. These are
	// copies taken at CreateFromEntry time: the FastArray entry stays the source of truth, and a
	// changed entry produces a fresh view-model (UDaInventoryWidgetController rebuilds on every
	// OnEntryChanged), so nothing here goes stale in place.

	/** Every Item.Stat.* leaf on the backing entry, as tag -> count. */
	UPROPERTY(BlueprintReadOnly, Category="Inventory|Stats")
	TMap<FGameplayTag, int32> StatCounts;

	/** True when the item's definition opts into the condition/wear model. When false, Condition,
	 *  ConditionCap and ConditionBand are all meaningless and a UI should draw no wear at all. */
	UPROPERTY(BlueprintReadOnly, Category="Inventory|Condition")
	bool bUsesCondition = false;

	/** Permanent provenance (Item.Stat.Grade). 0 when the item carries no grade. */
	UPROPERTY(BlueprintReadOnly, Category="Inventory|Condition")
	int32 Grade = 0;

	/** Item.Stat.Condition; 0 for a broken item AND for one that has no condition at all — read
	 *  bUsesCondition before believing it. */
	UPROPERTY(BlueprintReadOnly, Category="Inventory|Condition")
	int32 Condition = 0;

	/** Highest Condition this instance can hold, derived from its Grade by the definition's config. */
	UPROPERTY(BlueprintReadOnly, Category="Inventory|Condition")
	int32 ConditionCap = 0;

	/** Which band Condition falls into, from the one banding function
	 *  (UDaEquipmentManagerComponent::ComputeConditionBand). Normal for non-condition items. */
	UPROPERTY(BlueprintReadOnly, Category="Inventory|Condition")
	EDaConditionBand ConditionBand = EDaConditionBand::Normal;

	/** True when the definition lists any Equip.Slot.* tag, i.e. a hotbar press equips it rather
	 *  than using it. Lets a slot widget pick its affordance without re-resolving the definition. */
	UPROPERTY(BlueprintReadOnly, Category="Inventory|Equipment")
	bool bIsEquippable = false;

	/** Slots this item may occupy (the definition's EquipSlotTags). */
	UPROPERTY(BlueprintReadOnly, Category="Inventory|Equipment")
	FGameplayTagContainer EquipSlotTags;

	// ----- Delegates -----

	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FOnInventoryItemDataRemoved OnInventoryItemRemoved;

	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FOnInventoryItemUpdated OnInventoryItemUpdated;

	// ----- Population -----

	/** Populate this view-model from a resolved FastArray entry + definition. */
	void PopulateFromEntry(const FDaInventoryEntry& Entry, UDaItemDefinition* Definition);

	virtual void PopulateWithData(const FDaInventoryItemData& Data);
	virtual FDaInventoryItemData ToData() const;
	virtual void ClearData();

	// ----- Queries -----

	FGameplayTagContainer GetTags() const { return InventoryItemTags; }

	UFUNCTION(BlueprintCallable, Category="Inventory")
	FGameplayTag GetType() const;

	/** One Item.Stat.* count; 0 when this instance does not carry that stat. */
	UFUNCTION(BlueprintPure, Category="Inventory|Stats")
	int32 GetStatCount(FGameplayTag StatTag) const;

	UFUNCTION(BlueprintPure, Category="Inventory|Condition")
	bool UsesCondition() const { return bUsesCondition; }

	UFUNCTION(BlueprintPure, Category="Inventory|Condition")
	int32 GetGrade() const { return Grade; }

	UFUNCTION(BlueprintPure, Category="Inventory|Condition")
	int32 GetCondition() const { return Condition; }

	UFUNCTION(BlueprintPure, Category="Inventory|Condition")
	int32 GetConditionCap() const { return ConditionCap; }

	UFUNCTION(BlueprintPure, Category="Inventory|Condition")
	EDaConditionBand GetConditionBand() const { return ConditionBand; }

	/** Condition as 0..1 of the cap, for a progress bar. 0 when the item has no condition model,
	 *  so a bar bound to this reads empty rather than full for items wear does not apply to —
	 *  gate the bar's visibility on UsesCondition. */
	UFUNCTION(BlueprintPure, Category="Inventory|Condition")
	float GetConditionFraction() const;

	UFUNCTION(BlueprintPure, Category="Inventory|Equipment")
	bool IsEquippable() const { return bIsEquippable; }

	UFUNCTION(BlueprintCallable, Category="Inventory")
	virtual bool CanMergeWith(const UDaInventoryItemBase* OtherItem) const;

	UFUNCTION(BlueprintCallable, Category="Inventory")
	virtual void MergeWith(UDaInventoryItemBase* OtherItem);

	/** Nested inventories are not supported in the FastArray model; returns null. */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	UDaInventoryComponent* GetNestedInventory() const { return nullptr; }

	/** The backing UObject, if any (unused in FastArray model; kept for BP compat). */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	UObject* GetBaseObject() const { return BaseObject.Get(); }

	void SetBaseObject(const UObject* Object) { BaseObject = const_cast<UObject*>(Object); }

protected:

	// Tags of the item instance (from the item definition / entry)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	FGameplayTagContainer InventoryItemTags;

	// Tags describing which slot this can occupy
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory")
	FGameplayTagContainer SlotTags;

	UPROPERTY(EditDefaultsOnly, Category="Inventory")
	TObjectPtr<UDaAbilitySet> AbilitySetToGrant;

	UPROPERTY(Transient)
	TWeakObjectPtr<UObject> BaseObject;
};
