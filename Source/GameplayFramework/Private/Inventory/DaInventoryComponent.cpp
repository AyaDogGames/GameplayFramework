// Copyright Dream Awake Solutions LLC

#include "Inventory/DaInventoryComponent.h"

#include "AbilitySystem/DaAbilitySet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "CoreGameplayTags.h"
#include "DaItemActor.h"
#include "DaPlayerState.h"
#include "GameplayFramework.h"
#include "Engine/AssetManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Inventory/DaInventoryItemBase.h"
#include "Inventory/DaInventoryList.h"
#include "Inventory/DaItemDefinition.h"
#include "Net/UnrealNetwork.h"

namespace
{
	// Bounds on client-driven growth: a compromised client must not be able to grow an
	// entry's stat array or the loadout without limit.
	constexpr int32 MaxStatTagsPerEntry = 32;
	constexpr int32 MaxLoadoutEntries = 16;

	/**
	 * Slack on the repair cost before rounding up. The cost curve is authored as floats, so a
	 * price whose exact arithmetic is a whole number of credits usually is not: grade 7 at the
	 * shipped defaults gives 2 * (1 + 7 * 0.1f) = 3.40000002086..., and 20 points of that ceils
	 * to 69 credits instead of the 68 the content author priced. A thousandth of a credit is not
	 * a real cost, so it is charged as zero.
	 */
	constexpr double RepairCostEpsilon = 1.e-3;

	/** Credits for Points condition points on an item of this grade. Single source of truth for
	 *  the price: quoting (GetRepairCost) and charging (Internal_RepairItem) both come here. */
	int32 RepairCostFor(const FDaConditionConfig& Config, int32 Grade, int32 Points)
	{
		if (Points <= 0)
		{
			return 0;
		}

		const double PerPoint = static_cast<double>(Config.RepairCreditsPerPoint)
			* (1.0 + static_cast<double>(Grade) * static_cast<double>(Config.RepairGradeCostScale));

		return FMath::Max(0, FMath::CeilToInt(static_cast<double>(Points) * PerPoint - RepairCostEpsilon));
	}
}

UDaInventoryComponent::UDaInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// The first replication bunch can be applied before BeginPlay runs, and the FastArray
	// callbacks need the owner to broadcast through (Lyra sets this in the constructor too).
	InventoryList.OwnerComponent = this;
}

void UDaInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	InventoryList.OwnerComponent = this; // belt and braces; the constructor already set it
}

void UDaInventoryComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDaInventoryComponent, InventoryList);
	DOREPLIFETIME(UDaInventoryComponent, MaxSlots);
	DOREPLIFETIME(UDaInventoryComponent, InventoryTags);
	DOREPLIFETIME(UDaInventoryComponent, Loadout);
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
		// Send the DELTA, not a client-computed absolute: the client's replicated copy can be a
		// frame or more stale, and the server owns the arithmetic.
		Server_AddItemStat(ItemID, StatTag, Delta);
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
// Condition repair
// ---------------------------------------------------------------------------

bool UDaInventoryComponent::RepairItem(FGuid ItemID, int32 Points)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		// Client: route to server RPC (optimistic return; the server prices and validates).
		Server_RepairItem(ItemID, Points);
		return true;
	}

	return Internal_RepairItem(ItemID, Points);
}

int32 UDaInventoryComponent::GetRepairCost(FGuid ItemID, int32 Points) const
{
	const UDaItemDefinition* Def = ResolveConditionDefinition(ItemID);
	if (!Def)
	{
		return 0;
	}

	const int32 Grade = FMath::Clamp(GetItemStat(ItemID, CoreGameplayTags::TAG_Item_Stat_Grade), 0, 10);
	// Quote exactly what the till charges. Internal_RepairItem clamps an explicit order down to what
	// is actually missing, so pricing the full Points here would quote a shop customer for points the
	// item cannot hold — the quote and the charge have to be the same number.
	const int32 Billable = FMath::Min(Points, GetMissingCondition(ItemID));
	return RepairCostFor(Def->ConditionConfig, Grade, Billable);
}

int32 UDaInventoryComponent::GetMissingCondition(FGuid ItemID) const
{
	const UDaItemDefinition* Def = ResolveConditionDefinition(ItemID);
	if (!Def)
	{
		return 0;
	}

	const int32 Grade = FMath::Clamp(GetItemStat(ItemID, CoreGameplayTags::TAG_Item_Stat_Grade), 0, 10);
	const int32 Cap = Def->ConditionConfig.GetConditionCap(Grade);
	return FMath::Max(0, Cap - GetItemStat(ItemID, CoreGameplayTags::TAG_Item_Stat_Condition));
}

// ---------------------------------------------------------------------------
// Loadout
// ---------------------------------------------------------------------------

bool UDaInventoryComponent::SetLoadoutSlot(FGameplayTag SlotTag, FGuid ItemID)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		// Client: route to server RPC (optimistic return; server validates)
		Server_SetLoadoutSlot(SlotTag, ItemID);
		return true;
	}

	return Internal_SetLoadoutSlot(SlotTag, ItemID);
}

FGuid UDaInventoryComponent::GetLoadoutItemID(FGameplayTag SlotTag) const
{
	for (const FDaLoadoutEntry& Entry : Loadout)
	{
		if (Entry.SlotTag == SlotTag)
		{
			return Entry.ItemID;
		}
	}

	return FGuid();
}

TMap<FGameplayTag, FGuid> UDaInventoryComponent::GetLoadout() const
{
	TMap<FGameplayTag, FGuid> Result;
	for (const FDaLoadoutEntry& Entry : Loadout)
	{
		Result.Add(Entry.SlotTag, Entry.ItemID);
	}

	return Result;
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

	// Tell listeners the old contents are going away before they do. Clients get exactly this
	// notification from PreReplicatedRemove when the wipe replicates; without it, authority-side
	// listeners (equipment, UI) would keep state for entries that no longer exist.
	// Copied first: the broadcast can re-enter and mutate Entries.
	const TArray<FDaInventoryEntry> Existing = InventoryList.Entries;
	for (const FDaInventoryEntry& Entry : Existing)
	{
		OnEntryRemovedInternal(Entry);
	}

	// Clear existing entries
	InventoryList.MarkArrayDirty();
	InventoryList.Entries.Empty();

	// Re-add each saved entry
	for (const FDaInventoryEntry& Entry : SavedEntries)
	{
		InventoryList.AddEntry(Entry);
	}

	// Inert-lock migration. A save written before an item type opted into condition carries entries
	// with no Item.Stat.Condition at all — and a stat count of 0 is how "absent" is stored, so those
	// entries read as Broken and Internal_EquipItem refuses them forever. Fill them once, here, on
	// the way in. Deliberately NOT in RestoreEntry: a drop snapshot carries its own stats by design,
	// and re-initialising one would be a free repair.
	TArray<FGuid> NeedsConditionInit;
	for (const FDaInventoryEntry& Entry : InventoryList.Entries)
	{
		if (Entry.GetStatCount(CoreGameplayTags::TAG_Item_Stat_Condition) > 0)
		{
			continue;
		}
		const UDaItemDefinition* Def = ResolveItemDefinition(Entry.ItemDefinitionID);
		if (Def && Def->ConditionConfig.bUsesCondition)
		{
			NeedsConditionInit.Add(Entry.ItemID);
		}
	}
	// Second pass by identity: the stat writes below mark entries dirty and broadcast, and a
	// listener is free to reshape the array we would otherwise still be walking.
	for (const FGuid& ItemID : NeedsConditionInit)
	{
		const FDaInventoryEntry* Entry = FindEntryByItemID(ItemID);
		const UDaItemDefinition* Def = Entry ? ResolveItemDefinition(Entry->ItemDefinitionID) : nullptr;
		if (!Def)
		{
			continue;
		}
		LOG("LoadInventory: item %s (%s) loaded without Item.Stat.Condition — initialising it to its "
			"grade cap (pre-condition save)", *ItemID.ToString(), *Def->GetName());
		InitializeConditionStats(ItemID, *Def);
	}
}

bool UDaInventoryComponent::RestoreEntry(const FDaInventoryEntry& SavedEntry)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		LOG_WARNING("RestoreEntry called on non-authority. Ignoring.");
		return false;
	}

	if (!SavedEntry.IsValid())
	{
		LOG_WARNING("RestoreEntry: entry carries no ItemID — nothing to restore");
		return false;
	}

	// One instance, one place: restoring an ItemID this inventory already holds would give the
	// same item two entries, and every lookup by identity would then be ambiguous.
	if (FindEntryByItemID(SavedEntry.ItemID))
	{
		LOG_WARNING("RestoreEntry: item %s is already in this inventory", *SavedEntry.ItemID.ToString());
		return false;
	}

	int32 TargetSlot = SavedEntry.SlotIndex;
	if (TargetSlot < 0 || TargetSlot >= MaxSlots || InventoryList.FindBySlot(TargetSlot) != nullptr)
	{
		TargetSlot = InventoryList.FindFirstEmptySlot(MaxSlots);
	}

	if (TargetSlot == INDEX_NONE)
	{
		LOG_WARNING("RestoreEntry: inventory full (MaxSlots=%d), cannot restore item %s", MaxSlots, *SavedEntry.ItemID.ToString());
		return false;
	}

	// Verbatim copy apart from the slot: AddEntry rebuilds the stat accelerator and fires the
	// added-broadcast, exactly as it does for save-loaded entries.
	FDaInventoryEntry Restored = SavedEntry;
	Restored.SlotIndex = TargetSlot;
	InventoryList.AddEntry(Restored);
	return true;
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
	if (!IsClientWritableStat(StatTag))
	{
		LOG_WARNING("Server_SetItemStat: %s is not client-writable — rejected for item %s",
			*StatTag.ToString(), *ItemID.ToString());
		return;
	}

	if (!Internal_SetItemStat(ItemID, StatTag, Count))
	{
		LOG_WARNING("Server_SetItemStat failed for item %s stat %s", *ItemID.ToString(), *StatTag.ToString());
	}
}

void UDaInventoryComponent::Server_AddItemStat_Implementation(FGuid ItemID, FGameplayTag StatTag, int32 Delta)
{
	if (!IsClientWritableStat(StatTag))
	{
		LOG_WARNING("Server_AddItemStat: %s is not client-writable — rejected for item %s",
			*StatTag.ToString(), *ItemID.ToString());
		return;
	}

	// Read-modify-write against the server's own value, not a value the client computed.
	if (!Internal_SetItemStat(ItemID, StatTag, GetItemStat(ItemID, StatTag) + Delta))
	{
		LOG_WARNING("Server_AddItemStat failed for item %s stat %s delta %d", *ItemID.ToString(), *StatTag.ToString(), Delta);
	}
}

bool UDaInventoryComponent::IsClientWritableStat(FGameplayTag StatTag)
{
	// The protected leaves. These are provenance or economy inputs the authority alone decides:
	// Grade is permanent (it fixes the condition cap AND prices every repair), so a client that
	// could write it would mint itself a better item and a cheaper repair bill in one move.
	// Server-side callers — spawners, loot tables, the acquisition path — do not come through here:
	// they use SetItemStat/AddItemStat directly, which reach Internal_SetItemStat on the authority.
	// Extend this list as M3 adds leaves with the same shape.
	const FGameplayTag ProtectedLeaves[] = { CoreGameplayTags::TAG_Item_Stat_Grade };
	for (const FGameplayTag& Protected : ProtectedLeaves)
	{
		if (StatTag.MatchesTagExact(Protected))
		{
			return false;
		}
	}
	return true;
}

void UDaInventoryComponent::Server_SetLoadoutSlot_Implementation(FGameplayTag SlotTag, FGuid ItemID)
{
	if (!Internal_SetLoadoutSlot(SlotTag, ItemID))
	{
		LOG_WARNING("Server_SetLoadoutSlot failed for slot %s item %s", *SlotTag.ToString(), *ItemID.ToString());
	}
}

void UDaInventoryComponent::Server_RepairItem_Implementation(FGuid ItemID, int32 Points)
{
	if (!Internal_RepairItem(ItemID, Points))
	{
		LOG_WARNING("Server_RepairItem failed for item %s points %d", *ItemID.ToString(), Points);
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
		if (Def->ConditionConfig.bUsesCondition)
		{
			// Condition is per-INSTANCE state on one entry, and a stack is one entry standing in for
			// many items, so there is no honest answer to "what condition is this stack in". The
			// definition is misconfigured; IsDataValid flags it in the editor, and this is the same
			// finding at runtime for content that shipped before the check existed.
			LOG_WARNING("Internal_AddItem: %s sets bUsesCondition AND MaxStackCount=%d — stacked items "
				"get no condition stats. Set MaxStackCount=1 on condition-using definitions.",
				*Def->GetName(), Def->MaxStackCount);
		}

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

		// Stamp condition onto the LOCAL entry, before it is published. Filling it afterwards
		// through the stat path would publish a condition-user at 0 for an instant — one added
		// broadcast that reads as Broken, then two more changed broadcasts correcting it. Stamped
		// here, the entry is coherent the first time anyone sees it: one broadcast, one dirty mark.
		InitializeConditionStats(NewEntry, *Def);

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

	// Snapshot before any consume so listeners see the entry as it was used. The identity comes
	// along because the event below can invalidate both the pointer and the slot index.
	const FDaInventoryEntry UsedEntry = *Entry;
	const FGuid UsedItemID = Entry->ItemID;
	Entry = nullptr;

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
		// The ability that just ran could have consumed, moved or dropped this very item, so
		// find it again by identity rather than trusting SlotIndex to still mean the same thing.
		const FDaInventoryEntry* Current = FindEntryByItemID(UsedItemID);
		if (!Current)
		{
			LOG("Internal_UseItem: item %s no longer in the inventory after the use event — nothing to consume", *UsedItemID.ToString());
			Client_NotifyItemUsed(UsedEntry);
			return true;
		}
		RemoveItem(Current->SlotIndex, 1);
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
	const FGuid DroppedItemID = Entry->ItemID;
	Entry = nullptr;

	// Resolve pickup class before mutating inventory so a bad class fails the whole drop
	UClass* PickupClass = Def->PickupActorClass.IsNull() ? ADaItemActor::StaticClass() : Def->PickupActorClass.LoadSynchronous();
	if (!PickupClass)
	{
		LOG_WARNING("Internal_DropItem: failed to load PickupActorClass for %s", *DroppedEntry.ItemDefinitionID.ToString());
		return false;
	}

	// The synchronous loads above can tick the loading path and run arbitrary code, so re-locate
	// by identity: SlotIndex is only a name for where the item used to be.
	const FDaInventoryEntry* Current = FindEntryByItemID(DroppedItemID);
	if (!Current)
	{
		LOG_WARNING("Internal_DropItem: item %s left the inventory before it could be dropped", *DroppedItemID.ToString());
		return false;
	}

	if (!RemoveItem(Current->SlotIndex, DropCount >= DroppedEntry.StackCount ? 0 : DropCount))
	{
		return false;
	}

	// Spawn the world pickup in front of the avatar
	const FVector SpawnLocation = Avatar->GetActorLocation() + Avatar->GetActorForwardVector() * 150.f;
	const FTransform SpawnTransform(Avatar->GetActorRotation(), SpawnLocation);

	// A single pickup actor represents the whole dropped stack
	if (ADaItemActor* Pickup = GetWorld()->SpawnActorDeferred<ADaItemActor>(PickupClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
	{
		if (DropCount >= DroppedEntry.StackCount)
		{
			// The instance left the inventory whole, so hand the actor its entry: picking it back
			// up restores this same item, per-instance stats and all.
			Pickup->InitializeDroppedItem(DroppedEntry, Def->DisplayMesh.LoadSynchronous());
		}
		else
		{
			// A partial stack leaves the original entry (and its ItemID) behind in the inventory,
			// so what hits the ground is a new instance and gets a fresh ID on pickup.
			Pickup->InitializeDroppedItem(DroppedEntry.ItemDefinitionID, Def->DisplayMesh.LoadSynchronous());
		}
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

	// Clients reach this through Server_SetItemStat / Server_AddItemStat, so the tag is
	// untrusted input: only the Item.Stat tree may be written through the stat API.
	if (!StatTag.MatchesTag(CoreGameplayTags::TAG_Item_Stat))
	{
		LOG_WARNING("Internal_SetItemStat: rejected stat tag %s (not under %s)",
			*StatTag.ToString(), *CoreGameplayTags::TAG_Item_Stat.GetTag().ToString());
		return false;
	}

	// Counts are magnitudes; a negative one has no meaning and 0 is the "remove" encoding.
	Count = FMath::Max(0, Count);

	for (FDaInventoryEntry& Entry : InventoryList.Entries)
	{
		if (Entry.ItemID == ItemID)
		{
			if (Entry.MaxStackCount > 1)
			{
				LOG_WARNING("Internal_SetItemStat on stackable item %s (MaxStackCount=%d) — per-instance stats require MaxStackCount=1",
					*Entry.ItemDefinitionID.ToString(), Entry.MaxStackCount);
			}

			// Cap the array a client can grow. Existing tags (and removals) always go through.
			if (Count > 0 && Entry.GetStatCount(StatTag) == 0 && Entry.StatTags.Num() >= MaxStatTagsPerEntry)
			{
				LOG_WARNING("Internal_SetItemStat: item %s already holds %d stat tags (max %d) — rejected %s",
					*ItemID.ToString(), Entry.StatTags.Num(), MaxStatTagsPerEntry, *StatTag.ToString());
				return false;
			}

			Entry.SetStatCount(StatTag, Count);
			InventoryList.MarkItemDirty(Entry);

			// Snapshot, then broadcast — the discipline the other five guards in this file follow.
			// `Entry` is a reference into InventoryList.Entries, and the listeners this reaches
			// (equipment penalties, wear visuals) are free to add or remove entries underneath it.
			const FDaInventoryEntry Changed = Entry;
			// Mirrors FDaInventoryList::UpdateEntry's authority broadcast discipline.
			OnEntryChangedInternal(Changed);
			return true;
		}
	}

	LOG_WARNING("Internal_SetItemStat: item %s not in inventory", *ItemID.ToString());
	return false;
}

void UDaInventoryComponent::InitializeConditionStats(FDaInventoryEntry& Entry, const UDaItemDefinition& Def)
{
	const FDaConditionConfig& Config = Def.ConditionConfig;
	if (!Config.bUsesCondition)
	{
		return;
	}

	// Already worn in: this instance came from somewhere that owns its own stats.
	if (Entry.GetStatCount(CoreGameplayTags::TAG_Item_Stat_Condition) > 0)
	{
		return;
	}

	// A stat count of 0 is how "absent" is stored (SetStatCount(0) removes the tag), so a
	// grade of 0 and no grade at all read the same here — both take the definition's default.
	int32 Grade = Entry.GetStatCount(CoreGameplayTags::TAG_Item_Stat_Grade);
	if (Grade <= 0)
	{
		Grade = FMath::Clamp(Config.DefaultGrade, 0, 10);
		Entry.SetStatCount(CoreGameplayTags::TAG_Item_Stat_Grade, Grade);
	}

	// Items enter play at full condition.
	Entry.SetStatCount(CoreGameplayTags::TAG_Item_Stat_Condition, Config.GetConditionCap(Grade));
}

void UDaInventoryComponent::InitializeConditionStats(const FGuid& ItemID, const UDaItemDefinition& Def)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	const FDaConditionConfig& Config = Def.ConditionConfig;
	if (!Config.bUsesCondition || GetItemStat(ItemID, CoreGameplayTags::TAG_Item_Stat_Condition) > 0)
	{
		return;
	}

	int32 Grade = GetItemStat(ItemID, CoreGameplayTags::TAG_Item_Stat_Grade);
	if (Grade <= 0)
	{
		Grade = FMath::Clamp(Config.DefaultGrade, 0, 10);
		Internal_SetItemStat(ItemID, CoreGameplayTags::TAG_Item_Stat_Grade, Grade);
	}

	// Both writes go through the stat path: the entry is already published, so each one has to mark
	// it dirty and broadcast changed for the UI and the equipment manager's band watcher.
	Internal_SetItemStat(ItemID, CoreGameplayTags::TAG_Item_Stat_Condition, Config.GetConditionCap(Grade));
}

bool UDaInventoryComponent::Internal_RepairItem(const FGuid& ItemID, int32 Points)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	// Clients reach this through Server_RepairItem, so Points is untrusted: a negative order
	// would otherwise pay the player credits for wearing the item down.
	if (Points < 0)
	{
		LOG_WARNING("Internal_RepairItem: negative Points %d for item %s", Points, *ItemID.ToString());
		return false;
	}

	const UDaItemDefinition* Def = ResolveConditionDefinition(ItemID);
	if (!Def)
	{
		LOG_WARNING("Internal_RepairItem: item %s is not in this inventory, or does not use condition", *ItemID.ToString());
		return false;
	}

	const FDaConditionConfig& Config = Def->ConditionConfig;
	const int32 Grade = FMath::Clamp(GetItemStat(ItemID, CoreGameplayTags::TAG_Item_Stat_Grade), 0, 10);
	const int32 Cap = Config.GetConditionCap(Grade);
	const int32 Current = GetItemStat(ItemID, CoreGameplayTags::TAG_Item_Stat_Condition);
	const int32 Missing = Cap - Current;
	if (Missing <= 0)
	{
		LOG_WARNING("Internal_RepairItem: item %s is already at its grade-%d cap (%d)", *ItemID.ToString(), Grade, Cap);
		return false;
	}

	ADaPlayerState* PlayerState = ResolveOwnerPlayerState();
	if (!PlayerState)
	{
		LOG_WARNING("Internal_RepairItem: no ADaPlayerState owns %s — nothing to charge the repair to", *GetNameSafe(GetOwner()));
		return false;
	}

	const int32 Credits = PlayerState->GetCredits();
	int32 Buying = 0;
	if (Points > 0)
	{
		// Explicit order: clamp to what is actually missing (never overshoot the cap, never charge
		// for points the item cannot hold), then take it or leave it.
		Buying = FMath::Min(Points, Missing);
		if (RepairCostFor(Config, Grade, Buying) > Credits)
		{
			LOG_WARNING("Internal_RepairItem: item %s repair of %d point(s) costs %d credits, player has %d — rejected",
				*ItemID.ToString(), Buying, RepairCostFor(Config, Grade, Buying), Credits);
			return false;
		}
	}
	else
	{
		// Points == 0: buy as much as the purse allows. The division only seeds the search —
		// RepairCostFor is the authority on the price, so the two can never disagree by a credit.
		const double PerPoint = static_cast<double>(Config.RepairCreditsPerPoint)
			* (1.0 + static_cast<double>(Grade) * static_cast<double>(Config.RepairGradeCostScale));
		Buying = PerPoint > 0.0
			? FMath::Clamp(FMath::FloorToInt(static_cast<double>(Credits) / PerPoint), 0, Missing)
			: Missing;
		while (Buying < Missing && RepairCostFor(Config, Grade, Buying + 1) <= Credits)
		{
			++Buying;
		}
		while (Buying > 0 && RepairCostFor(Config, Grade, Buying) > Credits)
		{
			--Buying;
		}

		if (Buying <= 0)
		{
			LOG_WARNING("Internal_RepairItem: item %s needs %d credits for a single point, player has %d",
				*ItemID.ToString(), RepairCostFor(Config, Grade, 1), Credits);
			return false;
		}
	}

	const int32 Cost = RepairCostFor(Config, Grade, Buying);
	PlayerState->AdjustCredits(-Cost);

	// The stat path marks the entry dirty and broadcasts changed, which is what makes the
	// equipment manager's threshold watcher drop the penalty band the repair just lifted.
	if (!Internal_SetItemStat(ItemID, CoreGameplayTags::TAG_Item_Stat_Condition, Current + Buying))
	{
		// Nothing above should let this happen; if it does, the player keeps their credits.
		PlayerState->AdjustCredits(Cost);
		LOG_WARNING("Internal_RepairItem: item %s stat write failed — refunded %d credits", *ItemID.ToString(), Cost);
		return false;
	}

	LOG("Repaired item %s by %d point(s) to %d/%d for %d credits (grade %d)",
		*ItemID.ToString(), Buying, Current + Buying, Cap, Cost, Grade);
	return true;
}

// ---------------------------------------------------------------------------
// Internal loadout logic (server-only)
// ---------------------------------------------------------------------------

bool UDaInventoryComponent::Internal_SetLoadoutSlot(FGameplayTag SlotTag, const FGuid& ItemID)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	if (!SlotTag.IsValid())
	{
		LOG_WARNING("Internal_SetLoadoutSlot: invalid slot tag");
		return false;
	}

	// Clients reach this through Server_SetLoadoutSlot, so the tag is untrusted input.
	if (!SlotTag.MatchesTag(CoreGameplayTags::TAG_Equip_Slot))
	{
		LOG_WARNING("Internal_SetLoadoutSlot: rejected slot tag %s (not under %s)",
			*SlotTag.ToString(), *CoreGameplayTags::TAG_Equip_Slot.GetTag().ToString());
		return false;
	}

	// An invalid ItemID clears the slot; anything else must name an item we actually hold.
	if (ItemID.IsValid() && !FindEntryByItemID(ItemID))
	{
		LOG_WARNING("Internal_SetLoadoutSlot: item %s not in inventory", *ItemID.ToString());
		return false;
	}

	for (int32 Index = 0; Index < Loadout.Num(); ++Index)
	{
		if (Loadout[Index].SlotTag == SlotTag)
		{
			if (ItemID.IsValid())
			{
				const bool bChanged = Loadout[Index].ItemID != ItemID;
				Loadout[Index].ItemID = ItemID;
				if (bChanged)
				{
					OnLoadoutChanged.Broadcast();
				}
			}
			else
			{
				Loadout.RemoveAt(Index);
				OnLoadoutChanged.Broadcast();
			}
			return true;
		}
	}

	if (ItemID.IsValid())
	{
		if (Loadout.Num() >= MaxLoadoutEntries)
		{
			LOG_WARNING("Internal_SetLoadoutSlot: loadout already holds %d assignments (max %d) — rejected %s",
				Loadout.Num(), MaxLoadoutEntries, *SlotTag.ToString());
			return false;
		}

		FDaLoadoutEntry& NewEntry = Loadout.AddDefaulted_GetRef();
		NewEntry.SlotTag = SlotTag;
		NewEntry.ItemID = ItemID;
		OnLoadoutChanged.Broadcast();
	}

	// Clearing a slot that was already empty is a no-op, not a failure — and broadcasts nothing.
	return true;
}

void UDaInventoryComponent::ClearLoadoutForItem(const FGuid& ItemID)
{
	if (GetOwnerRole() != ROLE_Authority || !ItemID.IsValid())
	{
		return;
	}

	bool bChanged = false;
	for (int32 Index = Loadout.Num() - 1; Index >= 0; --Index)
	{
		if (Loadout[Index].ItemID == ItemID)
		{
			Loadout.RemoveAt(Index);
			bChanged = true;
		}
	}

	if (bChanged)
	{
		OnLoadoutChanged.Broadcast();
	}
}

void UDaInventoryComponent::OnRep_Loadout()
{
	OnLoadoutChanged.Broadcast();
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

const UDaItemDefinition* UDaInventoryComponent::ResolveConditionDefinition(const FGuid& ItemID) const
{
	const FDaInventoryEntry* Entry = ItemID.IsValid() ? FindEntryByItemID(ItemID) : nullptr;
	if (!Entry)
	{
		return nullptr;
	}

	const UDaItemDefinition* Def = ResolveItemDefinition(Entry->ItemDefinitionID);
	return (Def && Def->ConditionConfig.bUsesCondition) ? Def : nullptr;
}

ADaPlayerState* UDaInventoryComponent::ResolveOwnerPlayerState() const
{
	// Player inventories live on the PlayerState (ADaPlayerState::InventoryComp), which is also
	// where the credits are, so this is the normal case.
	if (ADaPlayerState* OwnerState = Cast<ADaPlayerState>(GetOwner()))
	{
		return OwnerState;
	}

	// An inventory placed directly on a pawn still charges that pawn's player, when it has one.
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		return Cast<ADaPlayerState>(Pawn->GetPlayerState());
	}

	return nullptr;
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
	// Every full-removal path (RemoveItem, use-consume, drop, load-wipe) lands here, so this is
	// the one place that has to keep the loadout from naming an item the inventory no longer
	// holds. Done BEFORE the broadcast so listeners — the equipment manager above all — see a
	// coherent loadout while they react.
	if (GetOwnerRole() == ROLE_Authority)
	{
		ClearLoadoutForItem(Entry.ItemID);
	}

	OnEntryRemoved.Broadcast(Entry, Entry.SlotIndex);
}

void UDaInventoryComponent::OnEntryChangedInternal(const FDaInventoryEntry& Entry)
{
	OnEntryChanged.Broadcast(Entry, Entry.SlotIndex);
}
