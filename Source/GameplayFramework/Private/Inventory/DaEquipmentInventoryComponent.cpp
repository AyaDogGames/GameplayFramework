// Copyright Dream Awake Solutions LLC


#include "Inventory/DaEquipmentInventoryComponent.h"

#include "CoreGameplayTags.h"
#include "GameplayFramework.h"
#include "Inventory/DaInventoryItemBase.h"


UDaEquipmentInventoryComponent::UDaEquipmentInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDaEquipmentInventoryComponent::EquipItem(UDaInventoryItemBase* Item, FName EquipSlotName)
{
	if (!Item || !EquipSlotName.IsNone())
	{
		LOG_WARNING("EquipItem: Item or slot is null");
		return;
	}

	// check to see if the item can be equipped in this slot by seeing if it has the same slot tag in its tags
	FGameplayTag EquipTag = FGameplayTag::RequestGameplayTag(EquipSlotName);
	if (Item->GetTags().HasTag(EquipTag))
	{
		int32 SlotIndex = SlotNames.Contains(EquipSlotName) ? SlotNames.IndexOfByKey(EquipSlotName) : INDEX_NONE;
		if (SlotIndex == INDEX_NONE)
		{
			LOG_WARNING("EquipItem: SlotIndex does not exist, this must be created in advance");
			return;
		}

		if (!AddItem(Item, SlotIndex))
		{
			LOG_WARNING("EquipItem: AddItem failed");
		}
	}
}

bool UDaEquipmentInventoryComponent::AddItem(const UObject* SourceObject, int32 SlotIndex)
{
	if (AddItem(SourceObject, SlotIndex)) // Use base AddItem for replication/prediction
	{
		if (GetOwnerRole() == ROLE_Authority)
		{
			UDaInventoryItemBase* Item = Items[SlotIndex];
			Item->ActivateEquipAbility(); // Activate ability on successful server equip
		}
		return true;
	}
	return false;
}


void UDaEquipmentInventoryComponent::UnequipItem(FName EquipSlotName)
{
	if (!EquipSlotName.IsNone())
	{
		LOG_WARNING("UnequipItem slot is null");
		return;
	}

	int32 SlotIndex = SlotNames.Contains(EquipSlotName) ? SlotNames.IndexOfByKey(EquipSlotName) : INDEX_NONE;
	if (SlotIndex == INDEX_NONE)
	{
		LOG_WARNING("UnequipItem: SlotIndex does not exist, this must be created in advance");
		return;
	}
	
	if (UDaInventoryItemBase* Item = Items[SlotIndex])
	{
		if (!RemoveItem(SlotIndex))
		{
			LOG_WARNING("UnequipItem: RemoveItem failed");
		}
	}
}

bool UDaEquipmentInventoryComponent::RemoveItem(int32 SlotIndex)
{
	UDaInventoryItemBase* Item = Items[SlotIndex];
	if (RemoveItem(SlotIndex)) // Use base RemoveItem for replication/prediction
	{
		if (GetOwnerRole() == ROLE_Authority)
		{
			Item->EndEquipAbility();
		}
		return true;
	}

	// Item not found in the expected slot
	return false;
}



