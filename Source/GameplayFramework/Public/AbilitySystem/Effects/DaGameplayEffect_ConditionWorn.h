// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "DaGameplayEffect_ConditionWorn.generated.h"

/**
 * UDaGameplayEffect_ConditionWorn
 *
 * Applied by UDaEquipmentManagerComponent while an equipped item's Condition sits in the Worn
 * band (below FDaConditionConfig::WornThresholdPct of its grade-derived cap), and removed again
 * when the band changes or the item leaves the slot.
 *
 * It grants Item.Condition.Worn and carries no modifiers on purpose: the TAG is the framework's
 * contract, and each game decides what a worn weapon actually costs — either by keying its own
 * content off the tag, or by pointing FDaConditionConfig::WornEffect at a subclass of this that
 * adds real modifiers.
 */
UCLASS()
class GAMEPLAYFRAMEWORK_API UDaGameplayEffect_ConditionWorn : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UDaGameplayEffect_ConditionWorn();

	/** Where the granted tag is configured — see the .cpp for why it cannot be the constructor. */
	virtual void PostInitProperties() override;
};
