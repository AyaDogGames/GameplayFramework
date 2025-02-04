// Copyright Dream Awake Solutions LLC


#include "Inventory/DaInventoryUIWidget.h"

void UDaInventoryUIWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
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
