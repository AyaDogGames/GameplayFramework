// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "Inventory/DaInventoryItemBase.h"
#include "DaStackableInventoryItem.generated.h"

// delegate method to update UI When Quantity changes
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStackableItemQuantitySignature, int32, NewQuantity);

/**
 * UDaStackableInventoryItem
 *
 * Stackable variant of the inventory UI view-model. Stacking is authoritatively
 * handled by the FastArray (FDaInventoryEntry::StackCount); this class mirrors that
 * count for UI purposes and exposes the merge helpers Blueprints referenced.
 */
UCLASS(Blueprintable, BlueprintType)
class GAMEPLAYFRAMEWORK_API UDaStackableInventoryItem : public UDaInventoryItemBase
{
	GENERATED_BODY()

public:

	// Maximum stack size for this item
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Stacking")
	int32 MaxStackSize = 99;

	// Current quantity in the stack (mirrors the backing entry's StackCount)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory|Stacking")
	int32 Quantity = 1;

	UPROPERTY(BlueprintAssignable, Category="Inventory|Stacking")
	FStackableItemQuantitySignature StackQuantityUpdateDelegate;

	virtual bool CanMergeWith(const UDaInventoryItemBase* OtherItem) const override;
	virtual void MergeWith(UDaInventoryItemBase* OtherItem) override;
};
