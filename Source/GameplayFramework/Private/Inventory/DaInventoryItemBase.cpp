// Copyright Dream Awake Solutions LLC


#include "Inventory/DaInventoryItemBase.h"

#include "CoreGameplayTags.h"
#include "Engine/AssetManager.h"
#include "Inventory/DaInventoryEntry.h"
#include "Inventory/DaItemDefinition.h"

UDaInventoryItemBase::UDaInventoryItemBase()
{
}

UDaInventoryItemBase* UDaInventoryItemBase::CreateFromEntry(const FDaInventoryEntry& Entry, UObject* Outer)
{
	UObject* OuterToUse = Outer ? Outer : (UObject*)GetTransientPackage();
	UDaInventoryItemBase* NewItem = NewObject<UDaInventoryItemBase>(OuterToUse);

	// Resolve the item definition from its primary asset id (loaded copy if present,
	// otherwise synchronously load the asset path) — mirrors UDaInventoryComponent.
	UDaItemDefinition* Def = Cast<UDaItemDefinition>(UAssetManager::Get().GetPrimaryAssetObject(Entry.ItemDefinitionID));
	if (!Def)
	{
		const FSoftObjectPath AssetPath = UAssetManager::Get().GetPrimaryAssetPath(Entry.ItemDefinitionID);
		if (AssetPath.IsValid())
		{
			Def = Cast<UDaItemDefinition>(AssetPath.TryLoad());
		}
	}

	NewItem->PopulateFromEntry(Entry, Def);
	return NewItem;
}

UDaInventoryItemBase* UDaInventoryItemBase::CreateFromData(const FDaInventoryItemData& Data)
{
	TSubclassOf<UDaInventoryItemBase> ItemClass = Data.ItemClass;
	if (!ItemClass)
	{
		ItemClass = UDaInventoryItemBase::StaticClass();
	}
	UDaInventoryItemBase* NewItem = NewObject<UDaInventoryItemBase>(GetTransientPackage(), ItemClass);
	if (NewItem)
	{
		NewItem->bIsEmptySlot = false;
		NewItem->PopulateWithData(Data);
	}
	return NewItem;
}

void UDaInventoryItemBase::PopulateFromEntry(const FDaInventoryEntry& Entry, UDaItemDefinition* Definition)
{
	bIsEmptySlot = !Entry.IsValid();
	ItemID = Entry.ItemID;
	ItemDefinitionID = Entry.ItemDefinitionID;
	SlotIndex = Entry.SlotIndex;
	StackCount = Entry.StackCount;
	InventoryItemTags = Entry.Tags;

	if (Definition)
	{
		Name = FName(*Definition->DisplayName.ToString());
		Description = FName(*Definition->Description.ToString());
		Icon = Definition->Icon;
		AbilitySetToGrant = nullptr; // resolved on demand from the definition's soft ptr when needed
		// Merge definition categorisation tags with any per-instance tags.
		InventoryItemTags.AppendTags(Definition->ItemTags);
	}
}

void UDaInventoryItemBase::PopulateWithData(const FDaInventoryItemData& Data)
{
	InventoryItemTags = Data.Tags;
	Name = Data.ItemName;
	Description = Data.ItemDescription;
	ItemID = Data.ItemID;
	Icon = Data.Icon;
	StackCount = Data.ItemCount;

	if (Data.AbilitySetToGrant)
	{
		AbilitySetToGrant = Data.AbilitySetToGrant;
	}
}

FDaInventoryItemData UDaInventoryItemBase::ToData() const
{
	FDaInventoryItemData Data;
	Data.Tags = InventoryItemTags;
	Data.ItemName = Name;
	Data.ItemDescription = Description;
	Data.ItemID = ItemID;
	Data.ItemClass = GetClass();
	Data.Icon = Icon;
	Data.AbilitySetToGrant = AbilitySetToGrant;
	Data.ItemCount = StackCount;
	return Data;
}

void UDaInventoryItemBase::ClearData()
{
	bIsEmptySlot = true;
	InventoryItemTags = FGameplayTagContainer();
	AbilitySetToGrant = nullptr;
	Icon = nullptr;
	ThumbnailBrush = nullptr;
	Name = FName();
	Description = FName();
	ItemID = FGuid();
	ItemDefinitionID = FPrimaryAssetId();
	SlotIndex = INDEX_NONE;
	StackCount = 1;
}

FGameplayTag UDaInventoryItemBase::GetType() const
{
	return GetSpecificTag(InventoryItemTags, CoreGameplayTags::InventoryItem_Type);
}

bool UDaInventoryItemBase::CanMergeWith(const UDaInventoryItemBase* OtherItem) const
{
	// subclasses to implement if desired merging behavior
	return false;
}

void UDaInventoryItemBase::MergeWith(UDaInventoryItemBase* OtherItem)
{
	// subclasses to implement if desired merging behavior
}
