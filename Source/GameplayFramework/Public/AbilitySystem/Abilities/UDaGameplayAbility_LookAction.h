// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "DaGameplayAbility_BaseTriggeredInputAction.h"
#include "UDaGameplayAbility_LookAction.generated.h"

/**
 * 
 */
UCLASS()
class GAMEPLAYFRAMEWORK_API UUDaGameplayAbility_LookAction : public UDaGameplayAbility_BaseTriggeredInputAction
{
	GENERATED_BODY()

	virtual void OnTriggeredInputAction_Implementation(const FInputActionValue& Value) override;

public:
	UUDaGameplayAbility_LookAction(const FObjectInitializer& ObjectInitializer);

protected:

	void LookMouse(const FInputActionValue& InputValue);
	void LookStick(const FInputActionValue& InputValue);
};
