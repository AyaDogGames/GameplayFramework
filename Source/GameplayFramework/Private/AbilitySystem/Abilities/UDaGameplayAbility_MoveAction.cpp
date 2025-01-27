// Copyright Dream Awake Solutions LLC


#include "AbilitySystem/Abilities/UDaGameplayAbility_MoveAction.h"

#include "DaCharacter.h"
#include "CoreGameplayTags.h"
#include "InputActionValue.h"

void UUDaGameplayAbility_MoveAction::OnTriggeredInputAction_Implementation(const FInputActionValue& Value)
{
	if (ADaCharacter* PlayerCharacter = Cast<ADaCharacter>(GetAvatarActorFromActorInfo()))
	{
		FRotator ControlRot = PlayerCharacter->GetControlRotation();

		// only need yaw
		ControlRot.Pitch = 0.0f;
		ControlRot.Roll = 0.0f;

		// Combined Input VaLue from WASD or Gamestick -> convert to vector
		const FVector2d AxisValue = Value.Get<FVector2d>();

		// Forward/Back
		PlayerCharacter->AddMovementInput(ControlRot.Vector(), AxisValue.Y);

		// Strafe
		const FVector RightVector = FRotationMatrix(ControlRot).GetScaledAxis(EAxis::Y);
		PlayerCharacter->AddMovementInput(RightVector, AxisValue.X);
		
	}
}

UUDaGameplayAbility_MoveAction::UUDaGameplayAbility_MoveAction(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	StartupActivationTags.AddTag(CoreGameplayTags::TAG_Input_Move);
}
