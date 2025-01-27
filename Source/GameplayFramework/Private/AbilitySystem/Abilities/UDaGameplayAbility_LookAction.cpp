// Copyright Dream Awake Solutions LLC


#include "AbilitySystem/Abilities/UDaGameplayAbility_LookAction.h"

#include "CommonInputTypeEnum.h"
#include "DaCharacter.h"
#include "CoreGameplayTags.h"
#include "GameplayFramework.h"
#include "InputActionValue.h"
#include "UI/DaCommonUIExtensions.h"

void UUDaGameplayAbility_LookAction::OnTriggeredInputAction_Implementation(const FInputActionValue& Value)
{
	if (ADaCharacter* PlayerCharacter = Cast<ADaCharacter>(GetAvatarActorFromActorInfo()))
	{
		APlayerController* PlayerController = Cast<APlayerController>(PlayerCharacter->Controller);
		if (PlayerController)
		{
			ECommonInputType inputType = UDaCommonUIExtensions::GetInputTypeFromController(PlayerController);
			if (inputType == ECommonInputType::MouseAndKeyboard)
			{
				LookMouse(Value);
			}
			else if (inputType == ECommonInputType::Gamepad)
			{
				LookStick(Value);
			}
			else
			{
				LOG_ERROR("Unhandled Input type in LookAction");
			}
		}
	}
}

void UUDaGameplayAbility_LookAction::LookMouse(const FInputActionValue& InputValue)
{
	if (ADaCharacter* PlayerCharacter = Cast<ADaCharacter>(GetAvatarActorFromActorInfo()))
	{
		const FVector2d MouseVec = InputValue.Get<FVector2d>();

		PlayerCharacter->AddControllerYawInput(MouseVec.X);
		PlayerCharacter->AddControllerPitchInput(MouseVec.Y);
	}
}

void UUDaGameplayAbility_LookAction::LookStick(const FInputActionValue& InputValue)
{
	if (ADaCharacter* PlayerCharacter = Cast<ADaCharacter>(GetAvatarActorFromActorInfo()))
	{
		FVector2d Value = InputValue.Get<FVector2d>();

		// track if negative
		bool XNegative = Value.X < 0.0f;
		bool YNegative = Value.Y < 0.0f;

		// @TODO: futher modify with sensativity settings
		static const float LookYawRate = 100.0f;
		static const float LookPitchRate = 50.0f;
		
		// nonlinear makes aim easier
		Value = Value*Value;

		if (XNegative)
		{
			Value.X *= -1.f;
		}
		if (YNegative)
		{
			Value.Y *= -1.f;
		}

		// Aim assist
		// todo: may need to ease this out and/or change strength based on distance to target
		float RateMultiplier = 0.5f;
		
		// @todo: increase accelerate longer input is > 0 on axis
		PlayerCharacter->AddControllerYawInput(Value.X * (LookYawRate * RateMultiplier) * GetWorld()->GetDeltaSeconds());
		PlayerCharacter->AddControllerPitchInput(Value.Y * (LookPitchRate * RateMultiplier) * GetWorld()->GetDeltaSeconds());
	}
}

UUDaGameplayAbility_LookAction::UUDaGameplayAbility_LookAction(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	StartupActivationTags.AddTag(CoreGameplayTags::TAG_Input_LookMouse);
	StartupActivationTags.AddTag(CoreGameplayTags::TAG_Input_LookStick);
}
