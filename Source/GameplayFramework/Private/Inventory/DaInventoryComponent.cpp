// Copyright Dream Awake Solutions LLC

#include "Inventory/DaInventoryComponent.h"

#include "AbilitySystem/DaAbilitySet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "CoreGameplayTags.h"
#include "DaItemActor.h"
#include "GameplayFramework.h"
#include "Engine/AssetManager.h"
#include "GameFramework/PlayerState.h"
#include "Inventory/DaInventoryItemBase.h"
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
	if (Count < 0)
	{
		return false;
	}

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
	if (FromSlot < 0 || FromSlot >= MaxSlots || ToSlot < 0 || ToSlot >= MaxSlots || FromSlot == ToSlot)
	{
		return false;
	}

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
			// Move: update slot index in-place to preserve FastArray identity (change delta, not remove/add)
			FDaInventoryEntry Updated = *FromEntry;
			Updated.SlotIndex = ToSlot;
			InventoryList.UpdateEntry(FromSlot, Updated);
		}
		return true;
	}

	// Client: route to server RPC
	Server_MoveItem(FromSlot, ToSlot);
	return true;
}

bool UDaInventoryComponent::UseItem(int32 SlotIndex)
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		return Internal_UseItem(SlotIndex);
	}

	// Client: route to server RPC (optimistic return)
	Server_UseItem(SlotIndex);
	return true;
}

bool UDaInventoryComponent::DropItem(int32 SlotIndex, int32 Count)
{
	if (Count < 0)
	{
		return false;
	}

	if (GetOwnerRole() == ROLE_Authority)
	{
		return Internal_DropItem(SlotIndex, Count);
	}

	// Client: route to server RPC (optimistic return)
	Server_DropItem(SlotIndex, Count);
	return true;
}

TArray<FDaInventoryEntry> UDaInventoryComponent::GetAllEntries() const
{
	return InventoryList.GetEntries();
}

TArray<UDaInventoryItemBase*> UDaInventoryComponent::GetItems()
{
	TArray<UDaInventoryItemBase*> Result;
	const TArray<FDaInventoryEntry>& Entries = InventoryList.GetEntries();
	Result.Reserve(Entries.Num());
	for (const FDaInventoryEntry& Entry : Entries)
	{
		if (UDaInventoryItemBase* Item = UDaInventoryItemBase::CreateFromEntry(Entry, this))
		{
			Result.Add(Item);
		}
	}
	return Result;
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
// Per-instance stats
// ---------------------------------------------------------------------------

const FDaInventoryEntry* UDaInventoryComponent::FindEntryByItemID(const FGuid& ItemID) const
{
	for (const FDaInventoryEntry& Entry : InventoryList.Entries)
	{
		if (Entry.ItemID == ItemID)
		{
			return &Entry;
		}
	}

	return nullptr;
}

bool UDaInventoryComponent::SetItemStat(FGuid ItemID, FGameplayTag StatTag, int32 Count)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		// Client: route to server RPC (optimistic return; server validates)
		Server_SetItemStat(ItemID, StatTag, Count);
		return true;
	}

	return Internal_SetItemStat(ItemID, StatTag, Count);
}

bool UDaInventoryComponent::AddItemStat(FGuid ItemID, FGameplayTag StatTag, int32 Delta)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		// Client cannot read-modify-write safely; send the absolute result to the server.
		Server_SetItemStat(ItemID, StatTag, GetItemStat(ItemID, StatTag) + Delta);
		return true;
	}

	return Internal_SetItemStat(ItemID, StatTag, GetItemStat(ItemID, StatTag) + Delta);
}

int32 UDaInventoryComponent::GetItemStat(FGuid ItemID, FGameplayTag StatTag) const
{
	const FDaInventoryEntry* Entry = FindEntryByItemID(ItemID);
	return Entry ? Entry->GetStatCount(StatTag) : 0;
}

int32 UDaInventoryComponent::GetItemSeed(FGuid ItemID)
{
	return static_cast<int32>(GetTypeHash(ItemID));
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
	if (Count < 0)
	{
		LOG_WARNING("Server_RemoveItem: invalid Count %d", Count);
		return;
	}

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
	if (FromSlot < 0 || FromSlot >= MaxSlots || ToSlot < 0 || ToSlot >= MaxSlots || FromSlot == ToSlot)
	{
		LOG_WARNING("Server_MoveItem: invalid slot range From=%d To=%d (MaxSlots=%d)", FromSlot, ToSlot, MaxSlots);
		return;
	}

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
		// Move in-place to preserve FastArray identity
		FDaInventoryEntry Updated = *FromEntry;
		Updated.SlotIndex = ToSlot;
		InventoryList.UpdateEntry(FromSlot, Updated);
	}
}

void UDaInventoryComponent::Server_UseItem_Implementation(int32 SlotIndex)
{
	if (!Internal_UseItem(SlotIndex))
	{
		LOG_WARNING("Server_UseItem failed for slot %d", SlotIndex);
	}
}

void UDaInventoryComponent::Server_DropItem_Implementation(int32 SlotIndex, int32 Count)
{
	if (Count < 0)
	{
		LOG_WARNING("Server_DropItem: invalid Count %d", Count);
		return;
	}

	if (!Internal_DropItem(SlotIndex, Count))
	{
		LOG_WARNING("Server_DropItem failed for slot %d", SlotIndex);
	}
}

void UDaInventoryComponent::Server_SetItemStat_Implementation(FGuid ItemID, FGameplayTag StatTag, int32 Count)
{
	if (!Internal_SetItemStat(ItemID, StatTag, Count))
	{
		LOG_WARNING("Server_SetItemStat failed for item %s stat %s", *ItemID.ToString(), *StatTag.ToString());
	}
}

// ---------------------------------------------------------------------------
// Client notifications
// ---------------------------------------------------------------------------

void UDaInventoryComponent::Client_NotifyItemUsed_Implementation(FDaInventoryEntry Entry)
{
	OnItemUsed.Broadcast(Entry, Entry.SlotIndex);
}

void UDaInventoryComponent::Client_NotifyItemDropped_Implementation(FDaInventoryEntry Entry)
{
	OnItemDropped.Broadcast(Entry, Entry.SlotIndex);
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

	if (!ItemDefinitionID.IsValid())
	{
		LOG_WARNING("Internal_AddItem: empty/invalid ItemDefinitionID passed by %s — the item must implement GetItemDefinitionID / have a UDaItemDefinition assigned",
			*GetNameSafe(GetOwner()));
		return false;
	}

	if (StackCount <= 0)
	{
		LOG_WARNING("Internal_AddItem: invalid StackCount %d for item %s", StackCount, *ItemDefinitionID.ToString());
		return false;
	}

	UDaItemDefinition* Def = ResolveItemDefinition(ItemDefinitionID);
	if (!Def)
	{
		LOG_WARNING("Internal_AddItem: failed to load item definition %s", *ItemDefinitionID.ToString());
		return false;
	}

	int32 Remaining = StackCount;

	// Check if stackable and try to stack with existing entries
	if (Def->MaxStackCount > 1)
	{
		// Build a temporary entry to test stacking compatibility
		FDaInventoryEntry StackProbe;
		StackProbe.ItemDefinitionID = ItemDefinitionID;
		StackProbe.MaxStackCount = Def->MaxStackCount;
		StackProbe.StackCount = 1;

		// Keep stacking into existing slots until we run out of items or stackable slots
		while (Remaining > 0)
		{
			int32 StackSlot = InventoryList.FindStackableSlot(StackProbe);
			if (StackSlot == INDEX_NONE)
			{
				break;
			}

			FDaInventoryEntry* Existing = InventoryList.FindBySlotMutable(StackSlot);
			if (!Existing)
			{
				break;
			}

			const int32 AvailableSpace = Existing->MaxStackCount - Existing->StackCount;
			const int32 ToStack = FMath::Min(AvailableSpace, Remaining);

			FDaInventoryEntry Updated = *Existing;
			Updated.StackCount += ToStack;
			InventoryList.UpdateEntry(StackSlot, Updated);

			Remaining -= ToStack;
		}

		if (Remaining <= 0)
		{
			return true;
		}
	}

	// Create new entries for remaining items (may need multiple slots for large stack counts)
	while (Remaining > 0)
	{
		// Find an empty slot
		int32 TargetSlot = INDEX_NONE;

		// Prefer SlotHint if valid and empty (only for first new entry)
		if (SlotHint >= 0 && SlotHint < MaxSlots)
		{
			if (InventoryList.FindBySlot(SlotHint) == nullptr)
			{
				TargetSlot = SlotHint;
			}
			SlotHint = INDEX_NONE; // Only use hint once
		}

		// Otherwise find first empty slot
		if (TargetSlot == INDEX_NONE)
		{
			TargetSlot = InventoryList.FindFirstEmptySlot(MaxSlots);
		}

		if (TargetSlot == INDEX_NONE)
		{
			LOG_WARNING("Internal_AddItem: inventory full, %d items could not be added (MaxSlots=%d)", Remaining, MaxSlots);
			return Remaining < StackCount; // Partial success if we added some
		}

		// Create new entry, clamped to max stack size
		const int32 EntryCount = FMath::Min(Remaining, Def->MaxStackCount);

		FDaInventoryEntry NewEntry;
		NewEntry.ItemID = FGuid::NewGuid();
		NewEntry.ItemDefinitionID = ItemDefinitionID;
		NewEntry.SlotIndex = TargetSlot;
		NewEntry.StackCount = EntryCount;
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
		Remaining -= EntryCount;
	}

	return true;
}

// ---------------------------------------------------------------------------
// Internal use/drop logic (server-only)
// ---------------------------------------------------------------------------

bool UDaInventoryComponent::Internal_UseItem(int32 SlotIndex)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	const FDaInventoryEntry* Entry = InventoryList.FindBySlot(SlotIndex);
	if (!Entry)
	{
		LOG_WARNING("Internal_UseItem: no entry at slot %d", SlotIndex);
		return false;
	}

	UDaItemDefinition* Def = ResolveItemDefinition(Entry->ItemDefinitionID);
	if (!Def)
	{
		LOG_WARNING("Internal_UseItem: failed to load item definition %s", *Entry->ItemDefinitionID.ToString());
		return false;
	}

	// Snapshot before any consume so listeners see the entry as it was used
	const FDaInventoryEntry UsedEntry = *Entry;

	// Let GAS abilities respond (e.g. an ability triggered by Action.UseItem applying the item's effect)
	if (AActor* Avatar = ResolveOwnerAvatar())
	{
		FGameplayEventData Payload;
		Payload.EventTag = CoreGameplayTags::TAG_Action_UseItem;
		Payload.Instigator = Avatar;
		Payload.OptionalObject = Def;
		Payload.EventMagnitude = SlotIndex;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Avatar, CoreGameplayTags::TAG_Action_UseItem, Payload);
	}

	if (Def->bConsumeOnUse)
	{
		RemoveItem(SlotIndex, 1);
	}

	Client_NotifyItemUsed(UsedEntry);
	return true;
}

bool UDaInventoryComponent::Internal_DropItem(int32 SlotIndex, int32 Count)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	const FDaInventoryEntry* Entry = InventoryList.FindBySlot(SlotIndex);
	if (!Entry)
	{
		LOG_WARNING("Internal_DropItem: no entry at slot %d", SlotIndex);
		return false;
	}

	UDaItemDefinition* Def = ResolveItemDefinition(Entry->ItemDefinitionID);
	if (!Def)
	{
		LOG_WARNING("Internal_DropItem: failed to load item definition %s", *Entry->ItemDefinitionID.ToString());
		return false;
	}

	AActor* Avatar = ResolveOwnerAvatar();
	if (!Avatar)
	{
		LOG_WARNING("Internal_DropItem: no world avatar for inventory owner %s — nowhere to drop", *GetNameSafe(GetOwner()));
		return false;
	}

	const int32 DropCount = (Count == 0) ? Entry->StackCount : FMath::Min(Count, Entry->StackCount);
	const FDaInventoryEntry DroppedEntry = *Entry;

	// Resolve pickup class before mutating inventory so a bad class fails the whole drop
	UClass* PickupClass = Def->PickupActorClass.IsNull() ? ADaItemActor::StaticClass() : Def->PickupActorClass.LoadSynchronous();
	if (!PickupClass)
	{
		LOG_WARNING("Internal_DropItem: failed to load PickupActorClass for %s", *Entry->ItemDefinitionID.ToString());
		return false;
	}

	if (!RemoveItem(SlotIndex, DropCount >= DroppedEntry.StackCount ? 0 : DropCount))
	{
		return false;
	}

	// Spawn the world pickup in front of the avatar
	const FVector SpawnLocation = Avatar->GetActorLocation() + Avatar->GetActorForwardVector() * 150.f;
	const FTransform SpawnTransform(Avatar->GetActorRotation(), SpawnLocation);

	// A single pickup actor represents the whole dropped stack
	if (ADaItemActor* Pickup = GetWorld()->SpawnActorDeferred<ADaItemActor>(PickupClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
	{
		Pickup->InitializeDroppedItem(DroppedEntry.ItemDefinitionID, Def->DisplayMesh.LoadSynchronous());
		Pickup->FinishSpawning(SpawnTransform);
	}

	FDaInventoryEntry NotifyEntry = DroppedEntry;
	NotifyEntry.StackCount = DropCount;
	Client_NotifyItemDropped(NotifyEntry);
	return true;
}

// ---------------------------------------------------------------------------
// Internal stat logic (server-only)
// ---------------------------------------------------------------------------

bool UDaInventoryComponent::Internal_SetItemStat(const FGuid& ItemID, FGameplayTag StatTag, int32 Count)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (!ItemID.IsValid() || !StatTag.IsValid())
	{
		LOG_WARNING("Internal_SetItemStat: invalid ItemID (%s) or stat tag (%s)", *ItemID.ToString(), *StatTag.ToString());
		return false;
	}

	for (FDaInventoryEntry& Entry : InventoryList.Entries)
	{
		if (Entry.ItemID == ItemID)
		{
			if (Entry.MaxStackCount > 1)
			{
				LOG_WARNING("Internal_SetItemStat on stackable item %s (MaxStackCount=%d) — per-instance stats require MaxStackCount=1",
					*Entry.ItemDefinitionID.ToString(), Entry.MaxStackCount);
			}

			Entry.SetStatCount(StatTag, Count);
			InventoryList.MarkItemDirty(Entry);

			// Mirrors FDaInventoryList::UpdateEntry's authority broadcast discipline.
			OnEntryChangedInternal(Entry);
			return true;
		}
	}

	LOG_WARNING("Internal_SetItemStat: item %s not in inventory", *ItemID.ToString());
	return false;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

UDaItemDefinition* UDaInventoryComponent::ResolveItemDefinition(FPrimaryAssetId ItemDefinitionID) const
{
	if (!ItemDefinitionID.IsValid())
	{
		return nullptr;
	}

	// Fast path: already loaded via the AssetManager
	UDaItemDefinition* Def = Cast<UDaItemDefinition>(UAssetManager::Get().GetPrimaryAssetObject(ItemDefinitionID));
	if (!Def)
	{
		// Synchronous load via resolved asset path (definitions are small data assets)
		FSoftObjectPath AssetPath = UAssetManager::Get().GetPrimaryAssetPath(ItemDefinitionID);
		if (AssetPath.IsValid())
		{
			Def = Cast<UDaItemDefinition>(AssetPath.TryLoad());
		}
	}
	return Def;
}

AActor* UDaInventoryComponent::ResolveOwnerAvatar() const
{
	AActor* Owner = GetOwner();
	if (const APlayerState* PS = Cast<APlayerState>(Owner))
	{
		return PS->GetPawn();
	}
	return Owner;
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
