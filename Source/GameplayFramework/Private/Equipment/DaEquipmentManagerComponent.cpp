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
#include "AbilitySystem/DaAbilitySet.h"
#include "AbilitySystem/DaAbilitySystemComponent.h"
#include "Inventory/DaInventoryComponent.h"
#include "Inventory/DaInventoryEntry.h"
#include "Inventory/DaItemDefinition.h"

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
}

void UDaEquipmentManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Backstop for the UnPossessed drain: a pawn destroyed while still possessed
	// (or one that never had a controller) still has to give its grants back.
	UnequipAll();

	if (UDaInventoryComponent* Inventory = BoundInventory.Get())
	{
		Inventory->OnEntryRemoved.RemoveDynamic(this, &UDaEquipmentManagerComponent::OnInventoryEntryRemoved);
	}
	BoundInventory.Reset();

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
	if (GetOwnerRole() != ROLE_Authority || BoundInventory.IsValid())
	{
		return;
	}

	UDaInventoryComponent* Inventory = ResolveInventory();
	if (!Inventory)
	{
		// No PlayerState yet — ApplyLoadout (which runs on possess) tries again.
		return;
	}

	Inventory->OnEntryRemoved.AddDynamic(this, &UDaEquipmentManagerComponent::OnInventoryEntryRemoved);
	BoundInventory = Inventory;
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

	// Runs on possess, by which point the PlayerState (and its inventory) exists.
	EnsureInventoryBinding();

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
	check(GetOwnerRole() == ROLE_Authority);

	// Client-supplied slot tags reach here through Server_EquipItem, so validate the tag tree.
	if (SlotTag.IsValid() && !SlotTag.MatchesTag(CoreGameplayTags::TAG_Equip_Slot))
	{
		LOG_WARNING("[%s] EquipItem: rejected slot tag %s (not under %s)", *GetNameSafe(GetOwner()),
			*SlotTag.ToString(), *CoreGameplayTags::TAG_Equip_Slot.GetTag().ToString());
		return false;
	}

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
	// Authority-side broadcast (clients get it via PostReplicatedAdd).
	HandleEquipped(EquipmentList.Entries.Last());
	return true;
}

bool UDaEquipmentManagerComponent::Internal_UnequipSlot(FGameplayTag SlotTag)
{
	check(GetOwnerRole() == ROLE_Authority);

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
