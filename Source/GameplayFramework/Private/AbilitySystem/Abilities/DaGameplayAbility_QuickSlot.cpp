// Copyright Dream Awake Solutions LLC

#include "AbilitySystem/Abilities/DaGameplayAbility_QuickSlot.h"

#include "GameFramework/Pawn.h"
#include "Equipment/DaEquipmentManagerComponent.h"

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

	// Use-or-toggle lives on the equipment manager so the hotbar WIDGET's click runs exactly this
	// decision rather than a second copy of it (UDaHotbarWidget::ActivateSlot).
	APawn* Pawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	UDaEquipmentManagerComponent::ActivateItemSlotForPawn(Pawn, QuickSlotTag);

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
