// Copyright Dream Awake Solutions LLC

#include "AbilitySystem/Abilities/DaGameplayAbility_QuickSlot.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Equipment/DaEquipmentManagerComponent.h"
#include "Inventory/DaInventoryComponent.h"
#include "Inventory/DaInventoryEntry.h"
#include "Inventory/DaItemDefinition.h"
#include "Engine/AssetManager.h"

UDaGameplayAbility_QuickSlot::UDaGameplayAbility_QuickSlot()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
}

void UDaGameplayAbility_QuickSlot::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	APawn* Pawn = Cast<APawn>(Avatar);
	UDaInventoryComponent* Inventory = Pawn && Pawn->GetPlayerState()
		? UDaInventoryComponent::GetInventoryFromActor(Pawn->GetPlayerState())
		: UDaInventoryComponent::GetInventoryFromActor(Avatar);
	if (!Inventory || !QuickSlotTag.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	const FGuid ItemID = Inventory->GetLoadoutItemID(QuickSlotTag);
	const FDaInventoryEntry* Entry = ItemID.IsValid() ? Inventory->FindEntryByItemID(ItemID) : nullptr;
	if (!Entry)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// Resolve the definition to decide use-vs-equip.
	const FPrimaryAssetId DefinitionID = Entry->ItemDefinitionID;
	const int32 SlotIndex = Entry->SlotIndex;
	UDaItemDefinition* Def = nullptr;
	if (UObject* Loaded = UAssetManager::Get().GetPrimaryAssetObject(DefinitionID))
	{
		Def = Cast<UDaItemDefinition>(Loaded);
	}
	if (!Def)
	{
		Def = Cast<UDaItemDefinition>(UAssetManager::Get().GetPrimaryAssetPath(DefinitionID).TryLoad());
	}

	if (Def && !Def->EquipSlotTags.IsEmpty())
	{
		if (UDaEquipmentManagerComponent* Equipment = UDaEquipmentManagerComponent::GetEquipmentFromActor(Pawn))
		{
			if (Equipment->GetEquippedItemID(QuickSlotTag) == ItemID)
			{
				Equipment->UnequipSlot(QuickSlotTag);
			}
			else
			{
				Equipment->EquipItem(ItemID, QuickSlotTag);
			}
		}
	}
	else
	{
		// UseItem can destroy the entry this pointer came from, so read the slot index first.
		Inventory->UseItem(SlotIndex);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
