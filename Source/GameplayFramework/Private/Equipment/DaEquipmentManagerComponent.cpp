// Copyright Dream Awake Solutions LLC

#include "Equipment/DaEquipmentManagerComponent.h"

#include "AbilitySystemGlobals.h"
#include "Engine/AssetManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
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
}

void UDaEquipmentManagerComponent::BeginPlay()
{
	Super::BeginPlay();
	EquipmentList.OwnerComponent = this;
}

void UDaEquipmentManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Copy the slots first: unequipping mutates the entry array while we walk it.
	if (GetOwnerRole() == ROLE_Authority)
	{
		TArray<FGameplayTag> Slots;
		for (const FDaAppliedEquipmentEntry& Entry : EquipmentList.Entries)
		{
			Slots.Add(Entry.SlotTag);
		}
		for (const FGameplayTag& Slot : Slots)
		{
			Internal_UnequipSlot(Slot);
		}
	}
	Super::EndPlay(EndPlayReason);
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

	UDaInventoryComponent* Inventory = ResolveInventory();
	if (!Inventory)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] EquipItem: no inventory component found"), *GetNameSafe(GetOwner()));
		return false;
	}
	const FDaInventoryEntry* Entry = Inventory->FindEntryByItemID(ItemID);
	if (!Entry)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] EquipItem: item %s not in inventory"), *GetNameSafe(GetOwner()), *ItemID.ToString());
		return false;
	}

	UDaItemDefinition* Def = ResolveItemDefinition(Entry->ItemDefinitionID);
	if (!Def)
	{
		return false;
	}
	if (Def->EquipSlotTags.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] EquipItem: %s has no EquipSlotTags"), *GetNameSafe(GetOwner()), *Def->GetName());
		return false;
	}

	// Resolve slot: explicit tag must be allowed; empty tag = first allowed free slot.
	if (SlotTag.IsValid())
	{
		if (!Def->EquipSlotTags.HasTag(SlotTag))
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] EquipItem: %s not allowed in slot %s"),
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
	// Item equipped elsewhere -> move it (unequip old slot first).
	if (IsItemEquipped(ItemID))
	{
		for (const FDaAppliedEquipmentEntry& Existing : EquipmentList.Entries)
		{
			if (Existing.ItemID == ItemID)
			{
				Internal_UnequipSlot(Existing.SlotTag);
				break;
			}
		}
	}

	FDaAppliedEquipmentEntry& NewEntry = EquipmentList.Entries.AddDefaulted_GetRef();
	NewEntry.ItemID = ItemID;
	NewEntry.ItemDefinitionID = Entry->ItemDefinitionID;
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
	if (Entry->AbilitySetID.IsValid())
	{
		// Per-instance override, resolved through the asset manager like definitions are.
		SetToGrant = Cast<UDaAbilitySet>(UAssetManager::Get().GetPrimaryAssetObject(Entry->AbilitySetID));
		if (!SetToGrant)
		{
			const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(Entry->AbilitySetID);
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

	EquipmentList.MarkItemDirty(NewEntry);
	HandleEquipped(NewEntry); // authority-side broadcast (clients get it via PostReplicatedAdd)
	return true;
}

bool UDaEquipmentManagerComponent::Internal_UnequipSlot(FGameplayTag SlotTag)
{
	check(GetOwnerRole() == ROLE_Authority);

	for (int32 Index = 0; Index < EquipmentList.Entries.Num(); ++Index)
	{
		FDaAppliedEquipmentEntry& Entry = EquipmentList.Entries[Index];
		if (Entry.SlotTag != SlotTag)
		{
			continue;
		}
		for (const FGameplayAbilitySpecHandle& Handle : Entry.GrantedHandles.GetAbilitySpecHandles())
		{
			AbilityToItemMap.Remove(Handle);
		}
		if (UDaAbilitySystemComponent* ASC = ResolveASC())
		{
			Entry.GrantedHandles.TakeFromAbilitySystem(ASC);
		}
		for (AActor* Spawned : Entry.SpawnedActors)
		{
			if (IsValid(Spawned))
			{
				Spawned->Destroy();
			}
		}
		const FDaAppliedEquipmentEntry Removed = Entry; // copy for the broadcast
		EquipmentList.Entries.RemoveAt(Index);
		EquipmentList.MarkArrayDirty();
		HandleUnequipped(Removed);
		return true;
	}
	return false;
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
