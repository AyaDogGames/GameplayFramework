// Copyright Dream Awake Solutions LLC


#include "AbilitySystem/Abilities/DaGameplayAbility_BaseTriggeredInputAction.h"

#include "EnhancedInputComponent.h"
#include "DaCharacter.h"
#include "DaPlayerController.h"
#include "DaInputComponent.h"

UDaGameplayAbility_BaseTriggeredInputAction::UDaGameplayAbility_BaseTriggeredInputAction(
	const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
		, bCancelAbilityOnInputReleased(true)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

void UDaGameplayAbility_BaseTriggeredInputAction::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bool bSuccess = false;

	//LogOnScreen(this, FString::Printf(TEXT("BaseTriggeredInputAction::ActivateAbility: %s"), *StartupActivationTag.ToString()), true, FColor::Red, 1.f, 4 );

	if (ADaPlayerController* PlayerController = Cast<ADaPlayerController>(ActorInfo->PlayerController))
	{
		check(PlayerController);
		
		InputComponent = Cast<UDaInputComponent>(PlayerController->InputComponent);
		if (InputComponent)
		{
			if (const UDaInputConfig* InputConfig = PlayerController->GetInputConfig())
			{
				for (const auto& It : InputConfig->AbilityInputActions)
				{
					if (It.InputAction && StartupActivationTags.HasTagExact(It.InputTag))
					{
						const FEnhancedInputActionEventBinding& TriggeredEventBinding = InputComponent->BindAction(It.InputAction, ETriggerEvent::Triggered, this, &UDaGameplayAbility_BaseTriggeredInputAction::OnTriggeredInputAction);
						const uint32 TriggeredEventHandle = TriggeredEventBinding.GetHandle();
	
						TriggeredEventHandles.AddUnique(TriggeredEventHandle);
      
						bSuccess = true;
					}
				}
			}
		}
	}

	if (bSuccess)
	{
		if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
		{			
			constexpr bool bReplicateEndAbility = true;
			constexpr bool bWasCancelled = true;
			EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
		}
	}
	else
	{
		constexpr bool bReplicateCancelAbility = true;
		CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
	}
}

void UDaGameplayAbility_BaseTriggeredInputAction::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	
	if (InputComponent)
	{
		for (const uint32 EventHandle : TriggeredEventHandles)
		{
			InputComponent->RemoveBindingByHandle(EventHandle);
		}
  
		InputComponent = nullptr;
	}
	
	TriggeredEventHandles.Reset();
}

void UDaGameplayAbility_BaseTriggeredInputAction::InputPressed(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);
}

void UDaGameplayAbility_BaseTriggeredInputAction::InputReleased(const FGameplayAbilitySpecHandle Handle,
                                                                const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	if (bCancelAbilityOnInputReleased)
	{
		CancelAbility(Handle, ActorInfo, ActivationInfo, true);

	}
}

void UDaGameplayAbility_BaseTriggeredInputAction::OnTriggeredInputAction_Implementation(const FInputActionValue& Value)
{
	// Subclasses should override this
}

