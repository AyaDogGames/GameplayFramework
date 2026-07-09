// Copyright Dream Awake Solutions LLC


#include "Inventory/DaStackableInventoryItem.h"

bool UDaStackableInventoryItem::CanMergeWith(const UDaInventoryItemBase* OtherItem) const
{
	const UDaStackableInventoryItem* StackableItem = Cast<UDaStackableInventoryItem>(OtherItem);
	if (StackableItem && StackableItem->GetClass() == GetClass() && Quantity < MaxStackSize)
	{
		const FGameplayTag ThisType = GetType();
		const FGameplayTag OtherType = OtherItem->GetType();
		if (ThisType.MatchesTagExact(OtherType))
		{
			return true;
		}
	}
	return false;
}

void UDaStackableInventoryItem::MergeWith(UDaInventoryItemBase* OtherItem)
{
	UDaStackableInventoryItem* StackableItem = Cast<UDaStackableInventoryItem>(OtherItem);
	if (StackableItem)
	{
		const int32 TransferAmount = FMath::Min(StackableItem->Quantity, MaxStackSize - Quantity);
		Quantity += TransferAmount;
		StackableItem->Quantity -= TransferAmount;

		StackQuantityUpdateDelegate.Broadcast(Quantity);
	}
}
