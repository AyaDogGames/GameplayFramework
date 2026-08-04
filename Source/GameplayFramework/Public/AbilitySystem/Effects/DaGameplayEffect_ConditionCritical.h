// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "DaGameplayEffect_ConditionCritical.generated.h"

/**
 * UDaGameplayEffect_ConditionCritical
 *
 * The Worn band's harsher sibling: applied while Condition is below
 * FDaConditionConfig::CriticalThresholdPct of the cap, and it REPLACES the Worn effect rather
 * than stacking with it, so exactly one band tag is ever owned per equipped item.
 *
 * Grants Item.Condition.Critical and nothing else — see UDaGameplayEffect_ConditionWorn for why
 * the modifiers are left to the game.
 */
UCLASS()
class GAMEPLAYFRAMEWORK_API UDaGameplayEffect_ConditionCritical : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UDaGameplayEffect_ConditionCritical();

	/** Where the granted tag is configured — see UDaGameplayEffect_ConditionWorn's .cpp for why. */
	virtual void PostInitProperties() override;
};
