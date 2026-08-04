// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DaGameplayAbilityBase.h"
#include "GameplayTagContainer.h"
#include "DaGameplayAbility_QuickSlot.generated.h"

/**
 * UDaGameplayAbility_QuickSlot
 *
 * Generic hotbar activation. Grant one instance per hotbar button through an
 * ability set entry whose InputTag is Input.Item1..4 and set QuickSlotTag to the
 * matching Equip.Slot.Item1..4. On activation: looks up the loadout item in that
 * slot; consumables are Used, equippables toggle equip/unequip.
 */
UCLASS()
class GAMEPLAYFRAMEWORK_API UDaGameplayAbility_QuickSlot : public UDaGameplayAbilityBase
{
	GENERATED_BODY()

public:

	UDaGameplayAbility_QuickSlot();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/**
	 * Which Equip.Slot.* this instance drives (set per-entry in the granting BP subclass/asset).
	 * Public so editor scripts and the hotbar UI can read/author it — a protected UPROPERTY is
	 * invisible to the Python editor API.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="QuickSlot", meta=(Categories="Equip.Slot"))
	FGameplayTag QuickSlotTag;
};
