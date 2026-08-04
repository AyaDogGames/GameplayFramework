// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
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
