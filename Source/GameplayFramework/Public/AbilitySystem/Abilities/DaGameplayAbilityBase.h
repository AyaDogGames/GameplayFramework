// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "DaGameplayAbilityBase.generated.h"

/**
 * 
 */
UCLASS()
class GAMEPLAYFRAMEWORK_API UDaGameplayAbilityBase : public UGameplayAbility
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly, Category="InputTag")
	FGameplayTagContainer StartupActivationTags;
};
