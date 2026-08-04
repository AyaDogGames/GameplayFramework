// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DaGameplayAbilityBase.h"
#include "GameplayTagContainer.h"
#include "DaGameplayAbility_QuickSlot.generated.h"

/**
 * UDaGameplayAbility_QuickSlot
 *
 * Generic hotbar activation. Grant one instance per hotbar button through an ability set
 * entry, whose InputTag is any Input.* tag the consuming project routes to the ASC (the
 * Input.Item1..4 leaves GlitchShaper uses are game-side tags, not plugin ones), and set
 * QuickSlotTag to the Equip.Slot.* that button drives. On activation: looks up the loadout
 * item in that slot; consumables are Used, equippables toggle equip/unequip.
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

protected:

	/** Which Equip.Slot.* this instance drives (set per-entry in the granting BP subclass/asset). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="QuickSlot", meta=(Categories="Equip.Slot"))
	FGameplayTag QuickSlotTag;
};
