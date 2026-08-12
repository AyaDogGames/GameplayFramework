// Copyright Dream Awake Solutions LLC

#include "Equipment/DaEquipmentManagerComponent.h"

#include "AbilitySystemGlobals.h"
#include "CoreGameplayTags.h"
#include "Engine/AssetManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameplayFramework.h"
#include "Net/UnrealNetwork.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/DaAbilitySet.h"
#include "AbilitySystem/DaAbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Engine/World.h"
#include "Inventory/DaInventoryComponent.h"
#include "Inventory/DaInventoryEntry.h"
#include "Inventory/DaItemDefinition.h"
#include "TimerManager.h"

UDaEquipmentManagerComponent::UDaEquipmentManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// The first replication bunch can be applied before BeginPlay runs, and the FastArray
	// callbacks need the owner to broadcast through (Lyra sets this in the constructor too).
	EquipmentList.OwnerComponent = this;
}

void UDaEquipmentManagerComponent::BeginPlay()
{
	Super::BeginPlay();
	EquipmentList.OwnerComponent = this; // belt and braces; the constructor already set it

	EnsureInventoryBinding();
	EnsureAbilityDecayBinding();
}

void UDaEquipmentManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Backstop for the UnPossessed drain: a pawn destroyed while still possessed
	// (or one that never had a controller) still has to give its grants back.
	UnequipAll();

	if (UDaInventoryComponent* Inventory = BoundInventory.Get())
	{
		Inventory->OnEntryRemoved.RemoveDynamic(this, &UDaEquipmentManagerComponent::OnInventoryEntryRemoved);
		Inventory->OnEntryChanged.RemoveDynamic(this, &UDaEquipmentManagerComponent::OnInventoryEntryChanged);
	}
	BoundInventory.Reset();

	// The ASC outlives this pawn (it sits on the PlayerState), so the callback has to come off.
	// Deliberately spelled out rather than calling ReleaseOwnerBindings(): this is the EndPlay
	// backstop for a pawn that was never unpossessed, and keeping it independent means a change to
	// the UnPossessed path cannot silently change teardown.
	if (UDaAbilitySystemComponent* ASC = BoundASC.Get())
	{
		ASC->AbilityActivatedCallbacks.Remove(AbilityActivatedHandle);
	}
	BoundASC.Reset();
	AbilityActivatedHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

void UDaEquipmentManagerComponent::UnequipAll()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	// Copy the slots first: unequipping mutates the entry array while we walk it.
	TArray<FGameplayTag> Slots;
	Slots.Reserve(EquipmentList.Entries.Num());
	for (const FDaAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		Slots.Add(Entry.SlotTag);
	}
	for (const FGameplayTag& Slot : Slots)
	{
		Internal_UnequipSlot(Slot);
	}
}

void UDaEquipmentManagerComponent::EnsureInventoryBinding()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	UDaInventoryComponent* Inventory = ResolveInventory();
	if (!Inventory)
	{
		// No PlayerState yet — ApplyLoadout (which runs on possess) tries again.
		return;
	}

	// Compare-and-rebind, not "bound once and done": a pawn can be re-possessed by a DIFFERENT
	// controller, and the inventory lives on the PlayerState, so what this component resolves after
	// a repossession may not be what it subscribed to. Still bound to the same one -> nothing to do.
	if (BoundInventory.Get() == Inventory)
	{
		return;
	}
	if (UDaInventoryComponent* Previous = BoundInventory.Get())
	{
		Previous->OnEntryRemoved.RemoveDynamic(this, &UDaEquipmentManagerComponent::OnInventoryEntryRemoved);
		Previous->OnEntryChanged.RemoveDynamic(this, &UDaEquipmentManagerComponent::OnInventoryEntryChanged);
	}

	Inventory->OnEntryRemoved.AddDynamic(this, &UDaEquipmentManagerComponent::OnInventoryEntryRemoved);
	Inventory->OnEntryChanged.AddDynamic(this, &UDaEquipmentManagerComponent::OnInventoryEntryChanged);
	BoundInventory = Inventory;
}

void UDaEquipmentManagerComponent::ReleaseOwnerBindings()
{
	// The ASC belongs to the PlayerState, not to this pawn, so a possession change replaces it.
	// Dropping the callback here (rather than leaving a binding pointing at the outgoing player's
	// ASC) is what keeps a re-possessed pawn from decaying the previous player's items.
	if (UDaAbilitySystemComponent* ASC = BoundASC.Get())
	{
		ASC->AbilityActivatedCallbacks.Remove(AbilityActivatedHandle);
	}
	BoundASC.Reset();
	AbilityActivatedHandle.Reset();
}

void UDaEquipmentManagerComponent::EnsureAbilityDecayBinding()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	UDaAbilitySystemComponent* ASC = ResolveASC();
	if (!ASC)
	{
		// No PlayerState / ASC yet — ApplyLoadout and the equip path try again.
		return;
	}

	// Same compare-and-rebind discipline as the inventory binding above: the ASC this component
	// resolves after a repossession can be a different object from the one it subscribed to.
	if (BoundASC.Get() == ASC)
	{
		return;
	}
	if (UDaAbilitySystemComponent* Previous = BoundASC.Get())
	{
		Previous->AbilityActivatedCallbacks.Remove(AbilityActivatedHandle);
		AbilityActivatedHandle.Reset();
	}

	// Ability ACTIVATION, not commit: the spec asked for the commit callback, but committing is
	// per-ability opt-in (UGameplayAbility::CommitAbility, which Blueprint abilities authored
	// without a Commit node never call), so a commit binding would silently never decay for
	// most content. Activation fires exactly once per successful use, on the authority for
	// client-initiated uses too, and only after CanActivateAbility has already checked
	// cost/cooldown. Binding both would double-charge the framework's C++ abilities, which do commit.
	AbilityActivatedHandle = ASC->AbilityActivatedCallbacks.AddUObject(
		this, &UDaEquipmentManagerComponent::OnAbilityActivated);
	BoundASC = ASC;
}

void UDaEquipmentManagerComponent::OnAbilityActivated(UGameplayAbility* Ability)
{
	if (!Ability || GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	// Only abilities this component granted map to an item; everything the player owns
	// natively (and every non-instanced ability, whose spec handle is not per-activation)
	// falls out here.
	const FGuid ItemID = GetItemIDForAbility(Ability->GetCurrentAbilitySpecHandle());
	if (!ItemID.IsValid())
	{
		return;
	}

	UDaInventoryComponent* Inventory = ResolveInventory();
	if (!Inventory)
	{
		return;
	}
	const FDaInventoryEntry* Entry = Inventory->FindEntryByItemID(ItemID);
	if (!Entry)
	{
		return;
	}
	// Copy before the stat write below reallocates the entry array.
	const FPrimaryAssetId EntryDefinitionID = Entry->ItemDefinitionID;
	Entry = nullptr;

	const UDaItemDefinition* Def = ResolveItemDefinition(EntryDefinitionID);
	if (!Def || !Def->ConditionConfig.bUsesCondition || Def->ConditionConfig.DecayPerUse <= 0)
	{
		return;
	}

	// The floor at 0 comes from the stat path's clamp.
	Inventory->AddItemStat(ItemID, CoreGameplayTags::TAG_Item_Stat_Condition,
		-Def->ConditionConfig.DecayPerUse);
}

void UDaEquipmentManagerComponent::OnInventoryEntryRemoved(const FDaInventoryEntry& Entry, int32 SlotIndex)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	// The item is gone from the inventory (dropped, consumed, wiped by a load), so the
	// equipment state that references it must go with it.
	const FGameplayTag Slot = FindSlotForItem(Entry.ItemID);
	if (Slot.IsValid())
	{
		Internal_UnequipSlot(Slot);
	}
}

void UDaEquipmentManagerComponent::OnInventoryEntryChanged(const FDaInventoryEntry& Entry, int32 SlotIndex)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	// Entries change for all sorts of reasons (stack counts, any stat, a slot move) and most
	// items are not equipped; only the equipped instance can earn a penalty, so the slot lookup
	// is the cheap gate in front of the real work.
	const FGameplayTag Slot = FindSlotForItem(Entry.ItemID);
	if (Slot.IsValid())
	{
		RefreshConditionPenalty(Slot);
	}
}

EDaConditionBand UDaEquipmentManagerComponent::ComputeConditionBand(const FDaConditionConfig& Config, int32 Condition, int32 Grade)
{
	// A stat count of 0 is also how "no such stat" is stored, so an item that never had its
	// Condition filled in reads as broken. That is the intended reading: the acquisition path
	// fills Condition for every condition-using item it creates.
	if (Condition <= 0)
	{
		return EDaConditionBand::Broken;
	}

	const int32 Cap = Config.GetConditionCap(Grade);
	if (Cap <= 0)
	{
		// Nothing to be a percentage of — a misconfigured cap must not invent penalties.
		return EDaConditionBand::Normal;
	}

	// Integer percent of the cap, floored: thresholds are "below this percent", so 25% of an
	// 88-point cap (22) is Worn and 21 is Critical.
	const int32 Percent = (Condition * 100) / Cap;
	if (Percent < Config.CriticalThresholdPct)
	{
		return EDaConditionBand::Critical;
	}
	if (Percent < Config.WornThresholdPct)
	{
		return EDaConditionBand::Worn;
	}
	return EDaConditionBand::Normal;
}

FGameplayTag UDaEquipmentManagerComponent::GetItemSlotTag(int32 SlotNumber)
{
	switch (SlotNumber)
	{
	case 1: return CoreGameplayTags::TAG_Equip_Slot_Item1;
	case 2: return CoreGameplayTags::TAG_Equip_Slot_Item2;
	case 3: return CoreGameplayTags::TAG_Equip_Slot_Item3;
	case 4: return CoreGameplayTags::TAG_Equip_Slot_Item4;
	default: return FGameplayTag();
	}
}

bool UDaEquipmentManagerComponent::ActivateItemSlotForPawn(APawn* Pawn, FGameplayTag SlotTag)
{
	if (!Pawn || !SlotTag.IsValid())
	{
		return false;
	}

	// The loadout lives on the PlayerState's inventory; fall back to a pawn-hosted one so a
	// pawn that owns its own inventory (AI, a vehicle) still works.
	UDaInventoryComponent* Inventory = Pawn->GetPlayerState()
		? UDaInventoryComponent::GetInventoryFromActor(Pawn->GetPlayerState())
		: nullptr;
	if (!Inventory)
	{
		Inventory = UDaInventoryComponent::GetInventoryFromActor(Pawn);
	}
	if (!Inventory)
	{
		return false;
	}

	const FGuid ItemID = Inventory->GetLoadoutItemID(SlotTag);
	const FDaInventoryEntry* Entry = ItemID.IsValid() ? Inventory->FindEntryByItemID(ItemID) : nullptr;
	if (!Entry)
	{
		return false;
	}

	// Read what we need off the entry before anything can invalidate the pointer.
	const FPrimaryAssetId DefinitionID = Entry->ItemDefinitionID;
	const int32 SlotIndex = Entry->SlotIndex;

	UDaItemDefinition* Def = Cast<UDaItemDefinition>(UAssetManager::Get().GetPrimaryAssetObject(DefinitionID));
	if (!Def)
	{
		Def = Cast<UDaItemDefinition>(UAssetManager::Get().GetPrimaryAssetPath(DefinitionID).TryLoad());
	}
	if (!Def)
	{
		return false;
	}

	if (!Def->EquipSlotTags.IsEmpty())
	{
		UDaEquipmentManagerComponent* Equipment = GetEquipmentFromActor(Pawn);
		if (!Equipment)
		{
			return false;
		}
		if (Equipment->GetEquippedItemID(SlotTag) == ItemID)
		{
			return Equipment->UnequipSlot(SlotTag);
		}
		return Equipment->EquipItem(ItemID, SlotTag);
	}

	return Inventory->UseItem(SlotIndex);
}

void UDaEquipmentManagerComponent::ClearConditionPenalty(FGameplayTag SlotTag)
{
	const FActiveGameplayEffectHandle* Tracked = ConditionPenaltyHandles.Find(SlotTag);
	if (Tracked && Tracked->IsValid())
	{
		// Resolve the ASC BEFORE the handle leaves the map. Removing it first and then failing to
		// find an ASC would strand an infinite-duration penalty on a component nothing has a handle
		// to any more; keeping the handle means the next call (or EndPlay) can still lift it.
		const FActiveGameplayEffectHandle Handle = *Tracked;
		UDaAbilitySystemComponent* ASC = ResolveASC();
		if (!ASC)
		{
			LOG_WARNING("[%s] condition penalty for %s: no ASC resolved, keeping the handle rather "
				"than stranding the effect", *GetNameSafe(GetOwner()), *SlotTag.ToString());
			return;
		}
		ASC->RemoveActiveGameplayEffect(Handle);
	}
	ConditionPenaltyHandles.Remove(SlotTag);
	ConditionBands.Remove(SlotTag);
}

void UDaEquipmentManagerComponent::RefreshConditionPenalty(FGameplayTag SlotTag)
{
	if (GetOwnerRole() != ROLE_Authority || !SlotTag.IsValid())
	{
		return;
	}

	const FGuid ItemID = GetEquippedItemID(SlotTag);
	if (!ItemID.IsValid())
	{
		ClearConditionPenalty(SlotTag);
		return;
	}

	UDaInventoryComponent* Inventory = ResolveInventory();
	const FDaInventoryEntry* Entry = Inventory ? Inventory->FindEntryByItemID(ItemID) : nullptr;
	if (!Entry)
	{
		// The item left the inventory; OnInventoryEntryRemoved unequips the slot and clears up.
		return;
	}
	// Copy before anything below can reallocate the entry array.
	const FPrimaryAssetId EntryDefinitionID = Entry->ItemDefinitionID;
	Entry = nullptr;

	const UDaItemDefinition* Def = ResolveItemDefinition(EntryDefinitionID);
	if (!Def || !Def->ConditionConfig.bUsesCondition)
	{
		return;
	}

	const int32 Condition = Inventory->GetItemStat(ItemID, CoreGameplayTags::TAG_Item_Stat_Condition);
	const int32 Grade = Inventory->GetItemStat(ItemID, CoreGameplayTags::TAG_Item_Stat_Grade);
	const EDaConditionBand Band = ComputeConditionBand(Def->ConditionConfig, Condition, Grade);

	const EDaConditionBand* Tracked = ConditionBands.Find(SlotTag);
	if (Tracked && *Tracked == Band)
	{
		// Every point of decay lands here; only a band crossing costs an effect swap.
		return;
	}

	if (Band == EDaConditionBand::Broken)
	{
		LOG_WARNING("[%s] %s broke (Condition 0) and is being unequipped from %s — repair required",
			*GetNameSafe(GetOwner()), *Def->GetName(), *SlotTag.ToString());
		// NOT here, and not synchronously. The decay that broke the item lands from
		// OnAbilityActivated, which runs inside the ability's PreActivate — and the actor
		// Internal_UnequipSlot would destroy is that still-activating ability's SourceObject.
		// Tearing it down mid-activation is the crash. One tick later the activation has finished,
		// and the deferred handler re-checks the band because a repair may have landed meanwhile.
		// The loadout ASSIGNMENT deliberately survives (M1 semantics), so a repaired item is one
		// hotbar press from being back in this slot.
		if (UWorld* World = GetWorld(); World && !PendingBreakSlots.Contains(SlotTag))
		{
			PendingBreakSlots.Add(SlotTag);
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(
				this, &UDaEquipmentManagerComponent::HandleDeferredBreak, SlotTag));
		}
		return;
	}

	ClearConditionPenalty(SlotTag);

	TSubclassOf<UGameplayEffect> EffectClass;
	if (Band == EDaConditionBand::Critical)
	{
		EffectClass = Def->ConditionConfig.CriticalEffect;
	}
	else if (Band == EDaConditionBand::Worn)
	{
		EffectClass = Def->ConditionConfig.WornEffect;
	}
	if (!EffectClass)
	{
		// Normal (no effect by definition), or a penalty band whose effect the content author
		// deliberately cleared. Normal is recorded unconditionally — the early-out above is what
		// keeps every single point of decay from redoing this work, and a Normal slot holds no
		// effect that the record could get out of step with.
		if (Band == EDaConditionBand::Normal)
		{
			ConditionBands.Add(SlotTag, Band);
		}
		return;
	}

	UDaAbilitySystemComponent* ASC = ResolveASC();
	if (!ASC)
	{
		LOG_WARNING("[%s] condition penalty for %s: no ASC to apply %s to",
			*GetNameSafe(GetOwner()), *Def->GetName(), *EffectClass->GetName());
		return;
	}
	const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectToSelf(
		EffectClass->GetDefaultObject<UGameplayEffect>(), 1.f, ASC->MakeEffectContext());
	if (Handle.IsValid())
	{
		// Record the band only now that there is an effect standing behind it: a band remembered
		// after a failed apply would make the early-out above skip the retry that could fix it.
		ConditionPenaltyHandles.Add(SlotTag, Handle);
		ConditionBands.Add(SlotTag, Band);
	}
}

void UDaEquipmentManagerComponent::HandleDeferredBreak(FGameplayTag SlotTag)
{
	PendingBreakSlots.Remove(SlotTag);

	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	// Everything that mattered a tick ago may have changed: the slot could have been unequipped or
	// swapped, the item could have left the inventory, or a repair could have brought it back above
	// zero. Only an item that is STILL equipped here and STILL broken gets torn down.
	const FGuid ItemID = GetEquippedItemID(SlotTag);
	if (!ItemID.IsValid())
	{
		return;
	}

	UDaInventoryComponent* Inventory = ResolveInventory();
	const FDaInventoryEntry* Entry = Inventory ? Inventory->FindEntryByItemID(ItemID) : nullptr;
	if (!Entry)
	{
		// The item left the inventory; OnInventoryEntryRemoved has already unequipped the slot.
		return;
	}
	const FPrimaryAssetId EntryDefinitionID = Entry->ItemDefinitionID;
	Entry = nullptr;

	const UDaItemDefinition* Def = ResolveItemDefinition(EntryDefinitionID);
	if (!Def || !Def->ConditionConfig.bUsesCondition)
	{
		return;
	}

	const int32 Condition = Inventory->GetItemStat(ItemID, CoreGameplayTags::TAG_Item_Stat_Condition);
	const int32 Grade = Inventory->GetItemStat(ItemID, CoreGameplayTags::TAG_Item_Stat_Grade);
	if (ComputeConditionBand(Def->ConditionConfig, Condition, Grade) != EDaConditionBand::Broken)
	{
		// Repaired inside the grace tick: give the slot the band it actually deserves instead.
		RefreshConditionPenalty(SlotTag);
		return;
	}

	Internal_UnequipSlot(SlotTag);
}

void UDaEquipmentManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDaEquipmentManagerComponent, EquipmentList);
}

bool UDaEquipmentManagerComponent::EquipItem(FGuid ItemID, FGameplayTag SlotTag)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		Server_EquipItem(ItemID, SlotTag);
		return true;
	}
	return Internal_EquipItem(ItemID, SlotTag);
}

bool UDaEquipmentManagerComponent::UnequipSlot(FGameplayTag SlotTag)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		Server_UnequipSlot(SlotTag);
		return true;
	}
	return Internal_UnequipSlot(SlotTag);
}

void UDaEquipmentManagerComponent::ApplyLoadout()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	// Runs on possess, by which point the PlayerState (and its inventory + ASC) exists.
	EnsureInventoryBinding();
	EnsureAbilityDecayBinding();

	UDaInventoryComponent* Inventory = ResolveInventory();
	if (!Inventory)
	{
		return;
	}

	for (const TPair<FGameplayTag, FGuid>& Pair : Inventory->GetLoadout())
	{
		const FDaInventoryEntry* Entry = Inventory->FindEntryByItemID(Pair.Value);
		if (!Entry)
		{
			// Assignment outlived the item (dropped, consumed, loaded from a stale save).
			continue;
		}

		// Assignment-only slots (consumables on a hotbar) have nothing to equip.
		UDaItemDefinition* Def = ResolveItemDefinition(Entry->ItemDefinitionID);
		if (Def && !Def->EquipSlotTags.IsEmpty())
		{
			Internal_EquipItem(Pair.Value, Pair.Key);
		}
	}
}

void UDaEquipmentManagerComponent::Server_EquipItem_Implementation(FGuid ItemID, FGameplayTag SlotTag)
{
	Internal_EquipItem(ItemID, SlotTag);
}

void UDaEquipmentManagerComponent::Server_UnequipSlot_Implementation(FGameplayTag SlotTag)
{
	Internal_UnequipSlot(SlotTag);
}

bool UDaEquipmentManagerComponent::Internal_EquipItem(const FGuid& ItemID, FGameplayTag SlotTag)
{
	// A guarded refusal, not an assert. A Server_* RPC body can execute locally on the caller —
	// editor Python does it on every call (FEditorScriptExecutionGuard forces local dispatch), and
	// a mis-owned component would too — and killing the process over a request we can simply refuse
	// is the wrong trade. Same discipline as every Internal_* in UDaInventoryComponent.
	if (GetOwnerRole() != ROLE_Authority)
	{
		LOG_WARNING("[%s] EquipItem: rejected on non-authority (role %d) — equipment is server-authoritative",
			*GetNameSafe(GetOwner()), static_cast<int32>(GetOwnerRole()));
		return false;
	}

	// Client-supplied slot tags reach here through Server_EquipItem, so validate the tag tree.
	if (SlotTag.IsValid() && !SlotTag.MatchesTag(CoreGameplayTags::TAG_Equip_Slot))
	{
		LOG_WARNING("[%s] EquipItem: rejected slot tag %s (not under %s)", *GetNameSafe(GetOwner()),
			*SlotTag.ToString(), *CoreGameplayTags::TAG_Equip_Slot.GetTag().ToString());
		return false;
	}

	// An item can be equipped without ApplyLoadout ever running (a pickup equipped mid-game),
	// and the decay hook has to be live before the first swing.
	EnsureAbilityDecayBinding();

	UDaInventoryComponent* Inventory = ResolveInventory();
	if (!Inventory)
	{
		LOG_WARNING("[%s] EquipItem: no inventory component found", *GetNameSafe(GetOwner()));
		return false;
	}
	const FDaInventoryEntry* Entry = Inventory->FindEntryByItemID(ItemID);
	if (!Entry)
	{
		LOG_WARNING("[%s] EquipItem: item %s not in inventory", *GetNameSafe(GetOwner()), *ItemID.ToString());
		return false;
	}

	// Copy everything needed off the inventory entry NOW: unequipping below can remove
	// inventory-side entries (and reallocate the array), leaving this pointer dangling.
	const FPrimaryAssetId EntryDefinitionID = Entry->ItemDefinitionID;
	const FPrimaryAssetId EntryAbilitySetID = Entry->AbilitySetID;
	Entry = nullptr;

	UDaItemDefinition* Def = ResolveItemDefinition(EntryDefinitionID);
	if (!Def)
	{
		return false;
	}
	if (Def->EquipSlotTags.IsEmpty())
	{
		LOG_WARNING("[%s] EquipItem: %s has no EquipSlotTags", *GetNameSafe(GetOwner()), *Def->GetName());
		return false;
	}

	// Inert at zero. Checked here, before any slot is resolved or vacated, so a refused equip
	// leaves the equipment state exactly as it was. Note a stat count cannot distinguish "0" from
	// "absent", so a condition-using item that never went through the acquisition fill also reads
	// as inert.
	if (Def->ConditionConfig.bUsesCondition
		&& Inventory->GetItemStat(ItemID, CoreGameplayTags::TAG_Item_Stat_Condition) <= 0)
	{
		LOG_WARNING("[%s] EquipItem: %s is inert — repair required (Condition 0)",
			*GetNameSafe(GetOwner()), *Def->GetName());
		return false;
	}

	// Resolve slot: explicit tag must be allowed; empty tag = first allowed free slot.
	if (SlotTag.IsValid())
	{
		if (!Def->EquipSlotTags.HasTag(SlotTag))
		{
			LOG_WARNING("[%s] EquipItem: %s not allowed in slot %s",
				*GetNameSafe(GetOwner()), *Def->GetName(), *SlotTag.ToString());
			return false;
		}
	}
	else
	{
		for (const FGameplayTag& Candidate : Def->EquipSlotTags)
		{
			if (!GetEquippedItemID(Candidate).IsValid())
			{
				SlotTag = Candidate;
				break;
			}
		}
		if (!SlotTag.IsValid())
		{
			SlotTag = Def->EquipSlotTags.First(); // all full: replace in the first allowed slot
		}
	}

	// Same item already in this slot -> done; occupied by another item -> swap out.
	const FGuid Occupant = GetEquippedItemID(SlotTag);
	if (Occupant == ItemID)
	{
		return true;
	}
	if (Occupant.IsValid())
	{
		Internal_UnequipSlot(SlotTag);
	}
	// Item equipped elsewhere -> move it. Read the slot tag, leave the loop, THEN unequip:
	// Internal_UnequipSlot removes from the array we would still be iterating.
	const FGameplayTag PreviousSlot = FindSlotForItem(ItemID);
	if (PreviousSlot.IsValid())
	{
		Internal_UnequipSlot(PreviousSlot);
	}

	// Build the entry as a LOCAL and only publish it once it is fully populated: spawning
	// actors and granting abilities can add/remove entries, which invalidates any reference
	// into EquipmentList.Entries held across those calls.
	FDaAppliedEquipmentEntry NewEntry;
	NewEntry.ItemID = ItemID;
	NewEntry.ItemDefinitionID = EntryDefinitionID;
	NewEntry.SlotTag = SlotTag;

	// Spawn equipment actors first so the primary actor can be the abilities' SourceObject.
	APawn* Pawn = Cast<APawn>(GetOwner());
	USceneComponent* AttachTarget = nullptr;
	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		AttachTarget = Character->GetMesh();
	}
	else if (Pawn)
	{
		AttachTarget = Pawn->GetRootComponent();
	}
	for (const FDaEquipmentActorToSpawn& SpawnInfo : Def->ActorsToSpawn)
	{
		UClass* ActorClass = SpawnInfo.ActorToSpawn.LoadSynchronous();
		if (!ActorClass || !AttachTarget)
		{
			continue;
		}
		AActor* NewActor = GetWorld()->SpawnActorDeferred<AActor>(ActorClass, FTransform::Identity, GetOwner());
		if (!NewActor)
		{
			continue;
		}
		NewActor->FinishSpawning(FTransform::Identity, /*bIsDefaultTransform=*/true);
		NewActor->SetActorRelativeTransform(SpawnInfo.AttachTransform);
		NewActor->AttachToComponent(AttachTarget, FAttachmentTransformRules::KeepRelativeTransform, SpawnInfo.AttachSocket);
		NewEntry.SpawnedActors.Add(NewActor);
	}

	// Grant the ability set: per-entry override first, else the definition's.
	UDaAbilitySet* SetToGrant = nullptr;
	if (EntryAbilitySetID.IsValid())
	{
		// Per-instance override, resolved through the asset manager like definitions are.
		SetToGrant = Cast<UDaAbilitySet>(UAssetManager::Get().GetPrimaryAssetObject(EntryAbilitySetID));
		if (!SetToGrant)
		{
			const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(EntryAbilitySetID);
			SetToGrant = Cast<UDaAbilitySet>(Path.TryLoad());
		}
	}
	if (!SetToGrant && !Def->AbilitySetToGrant.IsNull())
	{
		SetToGrant = Def->AbilitySetToGrant.LoadSynchronous();
	}
	if (SetToGrant)
	{
		if (UDaAbilitySystemComponent* ASC = ResolveASC())
		{
			UObject* SourceObject = NewEntry.SpawnedActors.Num() > 0
				? static_cast<UObject*>(NewEntry.SpawnedActors[0])
				: static_cast<UObject*>(Def);
			SetToGrant->GiveToAbilitySystem(ASC, &NewEntry.GrantedHandles, SourceObject);

			for (const FGameplayAbilitySpecHandle& Handle : NewEntry.GrantedHandles.GetAbilitySpecHandles())
			{
				AbilityToItemMap.Add(Handle, ItemID);
			}
		}
	}

	EquipmentList.Entries.Add(MoveTemp(NewEntry));
	EquipmentList.MarkItemDirty(EquipmentList.Entries.Last());

	// An item can be equipped already worn (picked up damaged, stowed while Critical), so the band
	// it is in has to be applied on the way in, not only when the next stat write lands — and
	// BEFORE the equip broadcast, so listeners read the final Worn/Critical tags rather than a
	// pristine-looking state that corrects itself a moment later.
	RefreshConditionPenalty(SlotTag);

	// Re-locate before broadcasting: applying a penalty effect runs listener code that can add or
	// remove entries, so the index from a moment ago is not a promise.
	const int32 EquippedIndex = EquipmentList.Entries.IndexOfByPredicate(
		[&ItemID, SlotTag](const FDaAppliedEquipmentEntry& Candidate)
		{ return Candidate.ItemID == ItemID && Candidate.SlotTag == SlotTag; });
	if (EquippedIndex != INDEX_NONE)
	{
		// Authority-side broadcast (clients get it via PostReplicatedAdd).
		HandleEquipped(EquipmentList.Entries[EquippedIndex]);
	}
	return true;
}

bool UDaEquipmentManagerComponent::Internal_UnequipSlot(FGameplayTag SlotTag)
{
	// See Internal_EquipItem: reachable off the authority (a locally dispatched Server_UnequipSlot),
	// and a refusal beats an assert that takes the editor with it.
	if (GetOwnerRole() != ROLE_Authority)
	{
		LOG_WARNING("[%s] UnequipSlot: rejected on non-authority (role %d) — equipment is server-authoritative",
			*GetNameSafe(GetOwner()), static_cast<int32>(GetOwnerRole()));
		return false;
	}

	const int32 Index = EquipmentList.Entries.IndexOfByPredicate(
		[SlotTag](const FDaAppliedEquipmentEntry& Candidate) { return Candidate.SlotTag == SlotTag; });
	if (Index == INDEX_NONE)
	{
		return false;
	}

	// Work from a copy from here on: the broadcast below can re-enter this component, and
	// nothing may hold a reference into Entries across it. The copy carries GrantedHandles
	// and SpawnedActors, so the teardown is identical.
	FDaAppliedEquipmentEntry Removed = EquipmentList.Entries[Index];

	// Broadcast BEFORE tearing anything down so host-side listeners see the same live state a
	// client sees in PreReplicatedRemove: spawned actors still valid, grants still on the ASC.
	HandleUnequipped(Removed);

	// Re-locate: a listener may have changed the array under us.
	const int32 RemoveAtIndex = EquipmentList.Entries.IndexOfByPredicate(
		[&Removed](const FDaAppliedEquipmentEntry& Candidate) { return Candidate.ItemID == Removed.ItemID && Candidate.SlotTag == Removed.SlotTag; });
	if (RemoveAtIndex != INDEX_NONE)
	{
		EquipmentList.Entries.RemoveAt(RemoveAtIndex);
		EquipmentList.MarkArrayDirty();
	}

	for (const FGameplayAbilitySpecHandle& Handle : Removed.GrantedHandles.GetAbilitySpecHandles())
	{
		AbilityToItemMap.Remove(Handle);
	}
	// The penalty belongs to the item, not to the wearer: it comes off with the item, otherwise
	// the next item in this slot would inherit the previous one's Worn/Critical tag.
	ClearConditionPenalty(Removed.SlotTag);
	if (UDaAbilitySystemComponent* ASC = ResolveASC())
	{
		Removed.GrantedHandles.TakeFromAbilitySystem(ASC);
	}
	for (AActor* Spawned : Removed.SpawnedActors)
	{
		if (IsValid(Spawned))
		{
			Spawned->Destroy();
		}
	}
	return true;
}

FGameplayTag UDaEquipmentManagerComponent::FindSlotForItem(const FGuid& ItemID) const
{
	for (const FDaAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		if (Entry.ItemID == ItemID)
		{
			return Entry.SlotTag;
		}
	}
	return FGameplayTag();
}

FGuid UDaEquipmentManagerComponent::GetEquippedItemID(FGameplayTag SlotTag) const
{
	for (const FDaAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		if (Entry.SlotTag == SlotTag)
		{
			return Entry.ItemID;
		}
	}
	return FGuid();
}

bool UDaEquipmentManagerComponent::IsItemEquipped(FGuid ItemID) const
{
	for (const FDaAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		if (Entry.ItemID == ItemID)
		{
			return true;
		}
	}
	return false;
}

FGuid UDaEquipmentManagerComponent::GetItemIDForAbility(FGameplayAbilitySpecHandle Handle) const
{
	if (const FGuid* Found = AbilityToItemMap.Find(Handle))
	{
		return *Found;
	}
	return FGuid();
}

FGuid UDaEquipmentManagerComponent::FindItemIDForSpawnedActor(const AActor* SpawnedActor) const
{
	if (!SpawnedActor)
	{
		return FGuid();
	}

	for (const FDaAppliedEquipmentEntry& Entry : EquipmentList.Entries)
	{
		for (const TObjectPtr<AActor>& Actor : Entry.SpawnedActors)
		{
			if (Actor.Get() == SpawnedActor)
			{
				return Entry.ItemID;
			}
		}
	}
	return FGuid();
}

UDaEquipmentManagerComponent* UDaEquipmentManagerComponent::GetEquipmentFromActor(AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UDaEquipmentManagerComponent>() : nullptr;
}

void UDaEquipmentManagerComponent::HandleEquipped(const FDaAppliedEquipmentEntry& Entry)
{
	OnEquipped.Broadcast(Entry);
}

void UDaEquipmentManagerComponent::HandleUnequipped(const FDaAppliedEquipmentEntry& Entry)
{
	OnUnequipped.Broadcast(Entry);
}

void UDaEquipmentManagerComponent::HandleChanged(const FDaAppliedEquipmentEntry& Entry)
{
	OnEquipmentChanged.Broadcast(Entry);
}

UDaInventoryComponent* UDaEquipmentManagerComponent::ResolveInventory() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->GetPlayerState())
	{
		if (UDaInventoryComponent* Inv = UDaInventoryComponent::GetInventoryFromActor(Pawn->GetPlayerState()))
		{
			return Inv;
		}
	}
	return UDaInventoryComponent::GetInventoryFromActor(GetOwner());
}

UDaAbilitySystemComponent* UDaEquipmentManagerComponent::ResolveASC() const
{
	return Cast<UDaAbilitySystemComponent>(
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()));
}

UDaItemDefinition* UDaEquipmentManagerComponent::ResolveItemDefinition(FPrimaryAssetId ItemDefinitionID) const
{
	if (!ItemDefinitionID.IsValid())
	{
		return nullptr;
	}
	if (UDaItemDefinition* Loaded = Cast<UDaItemDefinition>(UAssetManager::Get().GetPrimaryAssetObject(ItemDefinitionID)))
	{
		return Loaded;
	}
	const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(ItemDefinitionID);
	return Cast<UDaItemDefinition>(Path.TryLoad());
}
