// Copyright Dream Awake Solutions LLC


#include "Inventory/DaInventoryComponent.h"
#include "Inventory/DaInventoryItemBase.h"

#include "CoreGameplayTags.h"
#include "GameplayFramework.h"
#include "GameplayTagContainer.h"
#include "Inventory/DaInventoryBlueprintLibrary.h"
#include "Inventory/DaStackableInventoryItem.h"
#include "Inventory/DaMcpInventoryServerSubsystem.h"
#include "Net/UnrealNetwork.h"


UDaInventoryComponent::UDaInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDaInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	InitializeEmptySlots();
}

void UDaInventoryComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDaInventoryComponent, Items);
	DOREPLIFETIME(UDaInventoryComponent, MaxSize);
	DOREPLIFETIME(UDaInventoryComponent, FilledSlots);
	DOREPLIFETIME(UDaInventoryComponent, InventoryTags);
	
}

void UDaInventoryComponent::InitializeEmptySlots()
{
	Items.SetNum(MaxSize); // Ensure array is of correct size

	for (int32 i = 0; i < MaxSize; ++i)
	{
		if (!Items[i])
		{
			UDaInventoryItemBase* EmptySlot = NewObject<UDaInventoryItemBase>(this);
			EmptySlot->bIsEmptySlot = true;
			Items[i] = EmptySlot;
		}
	}

	if (GetOwnerRole() == ROLE_Authority)
	{
		OnRep_Items(); // Notify clients
	}
}

TArray<int32> UDaInventoryComponent::GetSlotsWithDuplicates(FGameplayTagContainer Tags) const
{
	FGameplayTag TagToCheck = GetSpecificTag(Tags, CoreGameplayTags::InventoryItem_Type);

	TArray<int32> DuplicateSlotIndexes;
	if (TagToCheck.IsValid())
	{
		int32 CurrentSlotIndex = 0;
		for (UDaInventoryItemBase* ExistingItem : Items)
		{
			if (ExistingItem->GetTags().HasTagExact(TagToCheck))
			{
				DuplicateSlotIndexes.Add(CurrentSlotIndex);
			}
			CurrentSlotIndex++;
		}
	}
	return DuplicateSlotIndexes;
}


bool UDaInventoryComponent::IsItemValid(FGameplayTagContainer Tags) const
{
	// check for duplicates
	bool bDuplicateExists = !GetSlotsWithDuplicates(Tags).IsEmpty();
	bool bAllowedDuplicate = DuplicationPolicy == EInventoryItemDuplicationPolicy::AllowDuplicates;
	
	// Add if item meets insertion and duplication policies
	if (InsertionPolicy == EInventoryItemInsertionPolicy::AddAlways)
	{
		if (bAllowedDuplicate || !bDuplicateExists)
		{
			return true;
		}
	}
	else if (InsertionPolicy == EInventoryItemInsertionPolicy::AddOnlyIfTypeTagMatches)
	{
		// Check if this inventory supports this type of item
		FGameplayTag InventoryTypeTag = GetSpecificTag(InventoryTags, CoreGameplayTags::InventoryItem_Type);
		if (InventoryTypeTag.IsValid() && Tags.HasTag(InventoryTypeTag))
		{
			if (bAllowedDuplicate || !bDuplicateExists)
			{
				return true;
			}
		}
	}

	LOG_WARNING("Item failed validation due when adding");
	
	return false;
}

int32 UDaInventoryComponent::FindSlot(FGameplayTagContainer Tags) const
{
	int32 SlotIndex = INDEX_NONE;

	// Find the first slot where we can stack a duplicate
	TArray<int32> DuplicateSlotIndexes = GetSlotsWithDuplicates(Tags);
	for (int32 DuplicateSlotIndex: DuplicateSlotIndexes)
	{
		UDaInventoryItemBase* DuplicateItem = Items[DuplicateSlotIndex];
		if (UDaStackableInventoryItem* StackableItem = Cast<UDaStackableInventoryItem>(DuplicateItem))
		{
			// Check if this Stackable Item can support more on its stack
			if (StackableItem->Quantity < StackableItem->MaxStackSize)
			{
				// We found the first stack that meets our criteria
				SlotIndex = DuplicateSlotIndex;
				break;
			}
		}
	}

	// We didnt find a duplicate to stack with, check and see if there is any room in this inventory still
	if (SlotIndex == INDEX_NONE && !IsComplete())
	{
		SlotIndex = GetSize();
	}
	
	return SlotIndex;
}

bool UDaInventoryComponent::AddItem(const UObject* SourceObject, int32 SlotIndex)
{
	if (!IsValid(SourceObject)) return false;

	// Determine the required subclass
	TSubclassOf<UDaInventoryItemBase> RequiredClass = UDaInventoryBlueprintLibrary::GetInventoryItemClass(SourceObject);
	if (RequiredClass)
	{
		UDaInventoryItemBase* NewItem = UDaInventoryBlueprintLibrary::CreateInventoryItem(SourceObject);
		if (NewItem)
		{
			if (SlotIndex == INDEX_NONE)
			{
				// We're being asked to find a place to add this.
				SlotIndex = FindSlot(NewItem->GetTags());
			}

			if (IsItemValid(NewItem->GetTags()))
			{
				if (SlotIndex > INDEX_NONE && Items.IsValidIndex(SlotIndex))
				{
					UDaInventoryItemBase* CurrentItem = Items[SlotIndex];
					if (CurrentItem)
					{
						// Check if the current item is not empty
						if (!CurrentItem->bIsEmptySlot)
						{
							// Broadcast the old item's data before replacing it
							FDaInventoryItemData OldData = CurrentItem->ToData();
				
							// Store old data to ensure proper rollback
							if (GetOwnerRole() != ROLE_Authority)
							{
								PredictedItems.Add(SlotIndex, OldData);
							}

							// Stackableitems merge
							if (CurrentItem->CanMergeWith(NewItem))
							{
								CurrentItem->MergeWith(NewItem);
							}
							else
							{
								// Removing data, possibly a swap
							
								// Inform listeners we're replacing the item if we didnt just merge
								CurrentItem->OnInventoryItemRemoved.Broadcast(OldData);
							}
						}
						else 
						{
							// Empty Slot
						
							// Determine the required subclass
							if (RequiredClass && CurrentItem->GetClass() != RequiredClass)
							{
								// Replace the current item with a new instance of the correct subclass (if for example its a stackableItem or something else)
								CurrentItem = NewItem;
								Items[SlotIndex] = NewItem;
							}
						}
						
						// Now fill the current Item with slot data
						CurrentItem->PopulateWithData(NewItem->ToData());
						CurrentItem->bIsEmptySlot = false;
						
						// Sync changes for server authority
						if (GetOwnerRole() != ROLE_Authority)
						{
							// Log item for rollback in case prediction fails
							Server_AddItem(SourceObject, SlotIndex);
						} else
						{
							FilledSlots++;
						}

                                                // broadcast our changes to the specific slot
                                                NotifyInventoryChanged(SlotIndex);

                                                if (GetOwnerRole() == ROLE_Authority)
                                                {
                                                        if (UGameInstance* GI = GetWorld()->GetGameInstance())
                                                        {
                                                                if (UDaMcpInventoryServerSubsystem* MCP = GI->GetSubsystem<UDaMcpInventoryServerSubsystem>())
                                                                {
                                                                        MCP->AddItemToServer(GetOwner(), CurrentItem->ToData());
                                                                }
                                                        }
                                                }

                                                return true;
					}
				}
			}
		}
	}
	return false;
}

void UDaInventoryComponent::Server_AddItem_Implementation(const UObject* SourceObject, int32 SlotIndex)
{
	if (AddItem(SourceObject, SlotIndex))
	{
		// Remove from pending if valid
		// Remove the rollback data
		PredictedItems.Remove(SlotIndex);
	}
	else
	{
		// Rollback prediction on client if invalid
		if (GetOwnerRole() == ROLE_SimulatedProxy)
		{
			RollbackPredictedItem(SlotIndex);
		}
	}
}

void UDaInventoryComponent::RollbackPredictedItem(int32 SlotIndex)
{
	if (SlotIndex > INDEX_NONE && Items.IsValidIndex(SlotIndex))
	{
		UDaInventoryItemBase* CurrentItem = Items[SlotIndex];
		FDaInventoryItemData* OriginalData = PredictedItems.Find(SlotIndex);
		if (OriginalData && OriginalData->ItemClass)
		{
			TSubclassOf<UDaInventoryItemBase> RequiredClass = OriginalData->ItemClass;
			if (CurrentItem->GetClass() != RequiredClass)
			{
				// Replace the current item with a new instance of the correct subclass
				CurrentItem = NewObject<UDaInventoryItemBase>(this, RequiredClass);
				Items[SlotIndex] = CurrentItem;
			}

			CurrentItem->PopulateWithData(*OriginalData);

			// Remove the rollback data
			PredictedItems.Remove(SlotIndex);

			// update listeners
			NotifyInventoryChanged(SlotIndex);
		}
	}
}


bool UDaInventoryComponent::RemoveItem(int32 SlotIndex)
{
	if (SlotIndex > INDEX_NONE && Items.IsValidIndex(SlotIndex))
	{
		UDaInventoryItemBase* CurrentItem = Items[SlotIndex];
		if (CurrentItem)
		{
			if (!CurrentItem->bIsEmptySlot)
			{
				// Broadcast the old item's data before replacing it
				FDaInventoryItemData OldData = CurrentItem->ToData();
				CurrentItem->ClearData();
				
				// notify listeners of the item we've removed
				CurrentItem->OnInventoryItemRemoved.Broadcast(OldData);
				
				// Store old data to ensure proper rollback
				if (GetOwnerRole() != ROLE_Authority)
				{
					PredictedItems.Add(SlotIndex, OldData);
					Server_RemoveItem(SlotIndex);
				} else
				{
					FilledSlots--;
				}

                                // broadcast our changes to the specific slot
                                NotifyInventoryChanged(SlotIndex);

                                if (GetOwnerRole() == ROLE_Authority)
                                {
                                        if (UGameInstance* GI = GetWorld()->GetGameInstance())
                                        {
                                                if (UDaMcpInventoryServerSubsystem* MCP = GI->GetSubsystem<UDaMcpInventoryServerSubsystem>())
                                                {
                                                        MCP->RemoveItemFromServer(GetOwner(), OldData.ItemID);
                                                }
                                        }
                                }

                                return true;
			}
		}
	}
	return false;
}

void UDaInventoryComponent::Server_RemoveItem_Implementation(int32 SlotIndex)
{
	if (RemoveItem(SlotIndex))
	{
		// Remove from pending if valid
		PredictedItems.Remove(SlotIndex);
	}
	else
	{
		// Rollback prediction on client if invalid
		if (GetOwnerRole() == ROLE_SimulatedProxy)
		{
			RollbackPredictedItem(SlotIndex);
		}
	}
}

void UDaInventoryComponent::OnRep_Items()
{
	NotifyInventoryChanged(INDEX_NONE);
}

void UDaInventoryComponent::NotifyInventoryChanged(int32 SlotIndex)
{
	if (SlotIndex == INDEX_NONE)
		OnInventoryChanged.Broadcast(Items, SlotIndex);
	else
	{
		UDaInventoryItemBase* CurrentItem = Items[SlotIndex];
		CurrentItem->OnInventoryItemUpdated.Broadcast(CurrentItem);
	}
}

UDaInventoryComponent* UDaInventoryComponent::GetInventoryFromActor(AActor* Actor)
{
	if (!Actor) return nullptr;
	return Actor->FindComponentByClass<UDaInventoryComponent>();
}

TArray<UDaInventoryItemBase*> UDaInventoryComponent::QueryByTag(FGameplayTagQuery Query) const
{
	TArray<UDaInventoryItemBase*> FilteredItems;
	for (UDaInventoryItemBase* Item : Items)
	{
		if (Query.Matches(Item->GetTags()))
		{
			FilteredItems.Add(Item);
		}
	}
	return FilteredItems;
}




