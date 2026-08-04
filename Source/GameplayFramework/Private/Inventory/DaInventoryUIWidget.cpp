// Copyright Dream Awake Solutions LLC


#include "Inventory/DaInventoryUIWidget.h"

#include "Inventory/DaInventoryItemBase.h"

void UDaInventoryUIWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	// Cache the view-model so Blueprint bindings can read it, then let BP react.
	InventoryItem = Cast<UDaInventoryItemBase>(ListItemObject);

	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
}

void UDaInventoryUIWidget::NativeOnItemSelectionChanged(bool bIsSelected)
{
	IUserObjectListEntry::NativeOnItemSelectionChanged(bIsSelected);
}

void UDaInventoryUIWidget::NativeOnItemExpansionChanged(bool bIsExpanded)
{
	IUserObjectListEntry::NativeOnItemExpansionChanged(bIsExpanded);
}

void UDaInventoryUIWidget::NativeOnEntryReleased()
{
	IUserObjectListEntry::NativeOnEntryReleased();
}
