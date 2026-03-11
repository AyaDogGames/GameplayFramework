// Copyright Dream Awake Solutions LLC

#include "Inventory/DaInventoryComponent.h"

#include "AbilitySystem/DaAbilitySet.h"
#include "GameplayFramework.h"
#include "Engine/AssetManager.h"
#include "Inventory/DaInventoryList.h"
#include "Inventory/DaItemDefinition.h"
#include "Net/UnrealNetwork.h"

UDaInventoryComponent::UDaInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDaInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	InventoryList.OwnerComponent = this;
}

void UDaInventoryComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDaInventoryComponent, InventoryList);
	DOREPLIFETIME(UDaInventoryComponent, MaxSlots);
	DOREPLIFETIME(UDaInventoryComponent, InventoryTags);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool UDaInventoryComponent::AddItem(FPrimaryAssetId ItemDefinitionID, int32 StackCount, int32 SlotHint)
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		return Internal_AddItem(ItemDefinitionID, StackCount, SlotHint);
	}

	// Client: route to server RPC (optimistic return)
	Server_AddItem(ItemDefinitionID, StackCount, SlotHint);
	return true;
}

bool UDaInventoryComponent::RemoveItem(int32 SlotIndex, int32 Count)
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		const FDaInventoryEntry* Entry = InventoryList.FindBySlot(SlotIndex);
		if (!Entry)
		{
			return false;
		}

		if (Count == 0 || Count >= Entry->StackCount)
		{
			InventoryList.RemoveEntry(SlotIndex);
		}
		else
		{
			FDaInventoryEntry Updated = *Entry;
			Updated.StackCount -= Count;
			InventoryList.UpdateEntry(SlotIndex, Updated);
		}
		return true;
	}

	// Client: route to server RPC
	Server_RemoveItem(SlotIndex, Count);
	return true;
}

bool UDaInventoryComponent::MoveItem(int32 FromSlot, int32 ToSlot)
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		const FDaInventoryEntry* FromEntry = InventoryList.FindBySlot(FromSlot);
		if (!FromEntry)
		{
			return false;
		}

		const FDaInventoryEntry* ToEntry = InventoryList.FindBySlot(ToSlot);
		if (ToEntry)
		{
			// Swap: update both entries with swapped slot indices
			FDaInventoryEntry UpdatedFrom = *FromEntry;
			FDaInventoryEntry UpdatedTo = *ToEntry;
			UpdatedFrom.SlotIndex = ToSlot;
			UpdatedTo.SlotIndex = FromSlot;
			InventoryList.UpdateEntry(FromSlot, UpdatedTo);
			InventoryList.UpdateEntry(ToSlot, UpdatedFrom);
		}
		else
		{
			// Move: just update the slot index
			FDaInventoryEntry Updated = *FromEntry;
			Updated.SlotIndex = ToSlot;

			// Remove from old slot, add to new
			InventoryList.RemoveEntry(FromSlot);
			InventoryList.AddEntry(Updated);
		}
		return true;
	}

	// Client: route to server RPC
	Server_MoveItem(FromSlot, ToSlot);
	return true;
}

TArray<FDaInventoryEntry> UDaInventoryComponent::GetAllEntries() const
{
	return InventoryList.GetEntries();
}

const FDaInventoryEntry* UDaInventoryComponent::GetEntryAtSlot(int32 SlotIndex) const
{
	return InventoryList.FindBySlot(SlotIndex);
}

bool UDaInventoryComponent::IsSlotEmpty(int32 SlotIndex) const
{
	return InventoryList.FindBySlot(SlotIndex) == nullptr;
}

int32 UDaInventoryComponent::GetFilledSlotCount() const
{
	return InventoryList.GetCount();
}

int32 UDaInventoryComponent::GetMaxSlots() const
{
	return MaxSlots;
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

TArray<FDaInventoryEntry> UDaInventoryComponent::SaveInventory() const
{
	return InventoryList.GetEntries();
}

void UDaInventoryComponent::LoadInventory(const TArray<FDaInventoryEntry>& SavedEntries)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		LOG_WARNING("LoadInventory called on non-authority. Ignoring.");
		return;
	}

	// Clear existing entries
	InventoryList.MarkArrayDirty();
	InventoryList.Entries.Empty();

	// Re-add each saved entry
	for (const FDaInventoryEntry& Entry : SavedEntries)
	{
		InventoryList.AddEntry(Entry);
	}
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

UDaInventoryComponent* UDaInventoryComponent::GetInventoryFromActor(AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}
	return Actor->FindComponentByClass<UDaInventoryComponent>();
}

// ---------------------------------------------------------------------------
// Server RPCs
// ---------------------------------------------------------------------------

void UDaInventoryComponent::Server_AddItem_Implementation(FPrimaryAssetId ItemDefinitionID, int32 StackCount, int32 SlotHint)
{
	if (!Internal_AddItem(ItemDefinitionID, StackCount, SlotHint))
	{
		LOG_WARNING("Server_AddItem failed for asset: %s", *ItemDefinitionID.ToString());
	}
}

void UDaInventoryComponent::Server_RemoveItem_Implementation(int32 SlotIndex, int32 Count)
{
	const FDaInventoryEntry* Entry = InventoryList.FindBySlot(SlotIndex);
	if (!Entry)
	{
		LOG_WARNING("Server_RemoveItem: no entry at slot %d", SlotIndex);
		return;
	}

	if (Count == 0 || Count >= Entry->StackCount)
	{
		InventoryList.RemoveEntry(SlotIndex);
	}
	else
	{
		FDaInventoryEntry Updated = *Entry;
		Updated.StackCount -= Count;
		InventoryList.UpdateEntry(SlotIndex, Updated);
	}
}

void UDaInventoryComponent::Server_MoveItem_Implementation(int32 FromSlot, int32 ToSlot)
{
	const FDaInventoryEntry* FromEntry = InventoryList.FindBySlot(FromSlot);
	if (!FromEntry)
	{
		LOG_WARNING("Server_MoveItem: no entry at from-slot %d", FromSlot);
		return;
	}

	const FDaInventoryEntry* ToEntry = InventoryList.FindBySlot(ToSlot);
	if (ToEntry)
	{
		// Swap
		FDaInventoryEntry UpdatedFrom = *FromEntry;
		FDaInventoryEntry UpdatedTo = *ToEntry;
		UpdatedFrom.SlotIndex = ToSlot;
		UpdatedTo.SlotIndex = FromSlot;
		InventoryList.UpdateEntry(FromSlot, UpdatedTo);
		InventoryList.UpdateEntry(ToSlot, UpdatedFrom);
	}
	else
	{
		// Move
		FDaInventoryEntry Updated = *FromEntry;
		Updated.SlotIndex = ToSlot;
		InventoryList.RemoveEntry(FromSlot);
		InventoryList.AddEntry(Updated);
	}
}

// ---------------------------------------------------------------------------
// Internal add logic (server-only)
// ---------------------------------------------------------------------------

bool UDaInventoryComponent::Internal_AddItem(FPrimaryAssetId ItemDefinitionID, int32 StackCount, int32 SlotHint)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	// Load the item definition
	UDaItemDefinition* Def = Cast<UDaItemDefinition>(UAssetManager::Get().GetPrimaryAssetObject(ItemDefinitionID));
	if (!Def)
	{
		// Try synchronous load
		UAssetManager::Get().LoadPrimaryAsset(ItemDefinitionID);
		Def = Cast<UDaItemDefinition>(UAssetManager::Get().GetPrimaryAssetObject(ItemDefinitionID));

		if (!Def)
		{
			LOG_WARNING("Internal_AddItem: failed to load item definition %s", *ItemDefinitionID.ToString());
			return false;
		}
	}

	// Check if stackable and try to stack with existing entry
	if (Def->MaxStackCount > 1)
	{
		// Build a temporary entry to test stacking compatibility
		FDaInventoryEntry StackProbe;
		StackProbe.ItemDefinitionID = ItemDefinitionID;
		StackProbe.MaxStackCount = Def->MaxStackCount;
		StackProbe.StackCount = 1; // Needs at least 1 for CanStackWith

		int32 StackSlot = InventoryList.FindStackableSlot(StackProbe);
		if (StackSlot != INDEX_NONE)
		{
			FDaInventoryEntry* Existing = InventoryList.FindBySlotMutable(StackSlot);
			if (Existing)
			{
				FDaInventoryEntry Updated = *Existing;
				Updated.StackCount = FMath::Min(Updated.StackCount + StackCount, Updated.MaxStackCount);
				InventoryList.UpdateEntry(StackSlot, Updated);
				return true;
			}
		}
	}

	// Find an empty slot
	int32 TargetSlot = INDEX_NONE;

	// Prefer SlotHint if valid and empty
	if (SlotHint != INDEX_NONE && SlotHint >= 0 && SlotHint < MaxSlots)
	{
		if (InventoryList.FindBySlot(SlotHint) == nullptr)
		{
			TargetSlot = SlotHint;
		}
	}

	// Otherwise find first empty slot
	if (TargetSlot == INDEX_NONE)
	{
		TargetSlot = InventoryList.FindFirstEmptySlot(MaxSlots);
	}

	if (TargetSlot == INDEX_NONE)
	{
		LOG_WARNING("Internal_AddItem: inventory full (MaxSlots=%d)", MaxSlots);
		return false;
	}

	// Create new entry
	FDaInventoryEntry NewEntry;
	NewEntry.ItemID = FGuid::NewGuid();
	NewEntry.ItemDefinitionID = ItemDefinitionID;
	NewEntry.SlotIndex = TargetSlot;
	NewEntry.StackCount = StackCount;
	NewEntry.MaxStackCount = Def->MaxStackCount;
	NewEntry.Tags = Def->ItemTags;

	// Convert ability set soft pointer to FPrimaryAssetId if valid
	if (!Def->AbilitySetToGrant.IsNull())
	{
		const FSoftObjectPath& AssetPath = Def->AbilitySetToGrant.ToSoftObjectPath();
		FString AssetName = FPackageName::ObjectPathToObjectName(AssetPath.GetAssetName());
		NewEntry.AbilitySetID = FPrimaryAssetId(FPrimaryAssetType(TEXT("AbilitySetData")), FName(*AssetName));
	}

	InventoryList.AddEntry(NewEntry);
	return true;
}

// ---------------------------------------------------------------------------
// Internal delegate broadcasts (called by FDaInventoryEntry FastArray callbacks)
// ---------------------------------------------------------------------------

void UDaInventoryComponent::OnEntryAddedInternal(const FDaInventoryEntry& Entry)
{
	OnEntryAdded.Broadcast(Entry, Entry.SlotIndex);
}

void UDaInventoryComponent::OnEntryRemovedInternal(const FDaInventoryEntry& Entry)
{
	OnEntryRemoved.Broadcast(Entry, Entry.SlotIndex);
}

void UDaInventoryComponent::OnEntryChangedInternal(const FDaInventoryEntry& Entry)
{
	OnEntryChanged.Broadcast(Entry, Entry.SlotIndex);
}
