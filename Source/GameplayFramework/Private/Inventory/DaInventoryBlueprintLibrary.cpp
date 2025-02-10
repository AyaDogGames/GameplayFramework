// Copyright Dream Awake Solutions LLC


#include "Inventory/DaInventoryBlueprintLibrary.h"

#include "Inventory/DaInventoryItemBase.h"
#include "Inventory/DaInventoryItemFactory.h"

TSubclassOf<UDaInventoryItemBase> UDaInventoryBlueprintLibrary::GetInventoryItemClassFromTags(const FGameplayTagContainer& Tags)
{
	if (Tags.IsValid())
	{
		for (const TScriptInterface<IDaInventoryItemFactory>& Factory : UDaInventoryItemBase::Factories)
		{
			if (TSubclassOf<UDaInventoryItemBase> ItemClass = Factory->DetermineInventoryItemClassFromTags(Tags))
			{
				return ItemClass;
			}
		}
	}
	return nullptr;
}

TSubclassOf<UDaInventoryItemBase> UDaInventoryBlueprintLibrary::GetInventoryItemClass(const UObject* SourceObject)
{
	for (const TScriptInterface<IDaInventoryItemFactory>& Factory : UDaInventoryItemBase::Factories)
	{
		if (TSubclassOf<UDaInventoryItemBase> ItemClass = Factory->DetermineInventoryItemClass(SourceObject))
		{
			return ItemClass;
		}
	}
	return nullptr;
}

UDaInventoryItemBase* UDaInventoryBlueprintLibrary::CreateInventoryItem(const UObject* SourceObject)
{
	for (const TScriptInterface<IDaInventoryItemFactory>& Factory : UDaInventoryItemBase::Factories)
	{
		if (UDaInventoryItemBase* InventoryItem = Factory->CreateInventoryItem(SourceObject))
		{
			return InventoryItem;
		}
	}
	return nullptr;
}

