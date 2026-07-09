// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "UI/DaWidgetController.h"
#include "DaInventoryWidgetController.generated.h"

class UDaInventoryComponent;
class UDaInventoryItemBase;
struct FDaInventoryEntry;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryItemsChanged, const TArray<UDaInventoryItemBase*>&, Items);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInventoryItemAtIndexChanged, const TArray<UDaInventoryItemBase*>&, Items, int32, SlotIndex);

/**
 * UDaInventoryWidgetController
 *
 * Adapts the FastArray inventory (UDaInventoryComponent) to the UMG layer. It listens
 * to the component's per-entry add/remove/change delegates, maintains an array of
 * UDaInventoryItemBase view-models mirroring the current entries, and re-broadcasts
 * changes through the Blueprint-facing delegates the inventory widgets bind to.
 */
UCLASS(Blueprintable)
class GAMEPLAYFRAMEWORK_API UDaInventoryWidgetController : public UDaWidgetController
{
	GENERATED_BODY()

public:

	/** Bind to the inventory component on Actor and broadcast the initial contents. */
	UFUNCTION(BlueprintCallable, Category = "DaInventoryWidgetController")
	void InitializeInventory(AActor* Actor);

	/** Current view-models, one per occupied slot. */
	UFUNCTION(BlueprintCallable, Category = "DaInventoryWidgetController")
	TArray<UDaInventoryItemBase*> GetItems() const;

	// Delegate to notify listeners when the whole inventory changes
	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FOnInventoryItemsChanged OnInventoryChanged;

	// Delegate to notify listeners when a single slot changes
	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FOnInventoryItemAtIndexChanged FOnInventoryItemChanged;

protected:

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "DaInventoryWidgetController")
	TObjectPtr<UDaInventoryComponent> InventoryComponent;

	// View-models mirroring the component's current entries
	UPROPERTY(Transient, BlueprintReadOnly, Category = "DaInventoryWidgetController")
	TArray<TObjectPtr<UDaInventoryItemBase>> Items;

	/** Rebuild the entire Items array from the component's current entries. */
	void RebuildItems();

	UFUNCTION()
	void HandleEntryAdded(const FDaInventoryEntry& Entry, int32 SlotIndex);

	UFUNCTION()
	void HandleEntryRemoved(const FDaInventoryEntry& Entry, int32 SlotIndex);

	UFUNCTION()
	void HandleEntryChanged(const FDaInventoryEntry& Entry, int32 SlotIndex);
};
