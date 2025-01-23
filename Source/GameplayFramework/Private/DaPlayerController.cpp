// Copyright Dream Awake Solutions LLC


#include "DaPlayerController.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "EnhancedInputSubsystems.h"
#include "AbilitySystem/DaAbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "DaInputComponent.h"
#include "DaPlayerState.h"
#include "Inventory/DaInventoryUIWidget.h"
#include "Inventory/DaInventoryWidgetController.h"
#include "Kismet/GameplayStatics.h"
#include "UI/DaHUD.h"
#include "UI/DaUserWidgetBase.h"
#include "UI/DaWidgetController.h"


ADaPlayerController::ADaPlayerController()
{
	bReplicates = true;
	InputType = EGameplayInputType::GameOnly;
	bLoadGameUIMappingContextByDefault = false;
}

void ADaPlayerController::ToggleIMC(bool bUI)
{
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (Subsystem)
	{
		Subsystem->ClearAllMappings();
		if (GameUIInputMappingContext && bUI) 
		{
			checkf(GameUIInputMappingContext, TEXT("No Game UI IMC Set on player controller"));
			Subsystem->AddMappingContext(GameUIInputMappingContext, 0);
		}
		if (GameplayInputMappingContext)
		{
			checkf(GameplayInputMappingContext, TEXT("No Gameplay IMC Set on player controller"));
			Subsystem->AddMappingContext(GameplayInputMappingContext, 1);
		}
	}
}


void ADaPlayerController::BeginPlay()
{
	Super::BeginPlay();
	check(GameplayInputMappingContext);
	
	ToggleIMC(bLoadGameUIMappingContextByDefault);
	SetCurrentInputMode(InputType);
}

void ADaPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	checkf(InputConfig, TEXT("DaPlayerController requires a reference to a DaInputConfig DataAsset mapping input actions to their respective Input GameplayTag."));
	
	UDaInputComponent* IC = CastChecked<UDaInputComponent>(InputComponent);
	IC->BindAbilityActions(InputConfig, this, &ThisClass::AbilityInputTagPressed, &ThisClass::AbilityInputTagReleased, &ThisClass::AbilityInputTagHeld);

}

void ADaPlayerController::AnyKeyInput(FKey PressedKey)
{
	bIsUsingGamepad = PressedKey.IsGamepadKey();
}

void ADaPlayerController::SetCurrentInputMode(EGameplayInputType Type)
{
	if (Type == EGameplayInputType::GameOnly)
	{
		bShowMouseCursor = false;
		SetInputMode(FInputModeGameOnly());
	}
	else if (Type == EGameplayInputType::GameAndCursor)
	{
		bShowMouseCursor = true;
		SetInputMode(FInputModeGameAndUI());
	} else
	{
		bShowMouseCursor = true;
		SetInputMode(FInputModeUIOnly());
	}
}

void ADaPlayerController::AbilityInputTagPressed(const FInputActionValue& Value, FGameplayTag InputTag)
{
	//LogOnScreen(this, FString::Printf(TEXT("Input Pressed: %s"), *InputTag.ToString()), true, FColor::Red, 1.f, 1 );
	if (GetAbilitySystemComponent()) GetAbilitySystemComponent()->AbilityInputTagPressed(Value, InputTag);
}

void ADaPlayerController::AbilityInputTagReleased(FGameplayTag InputTag)
{
	//LogOnScreen(this, FString::Printf(TEXT("Input Released: %s"), *InputTag.ToString()), true, FColor::Blue, 1.f, 2 );
	if (GetAbilitySystemComponent()) GetAbilitySystemComponent()->AbilityInputTagReleased(InputTag);
}

void ADaPlayerController::AbilityInputTagHeld(FGameplayTag InputTag)
{
	//LogOnScreen(this, FString::Printf(TEXT("Input Held: %s"), *InputTag.ToString()), true, FColor::Green, 1.f, 3 );
	if (GetAbilitySystemComponent()) GetAbilitySystemComponent()->AbilityInputTagHeld(InputTag);
}

UDaAbilitySystemComponent* ADaPlayerController::GetAbilitySystemComponent()
{
	if (AbilitySystemComponent == nullptr)
	{
		AbilitySystemComponent = Cast<UDaAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn<APawn>()) );
	}

	return AbilitySystemComponent;
}

void ADaPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);

	OnPawnChanged.Broadcast(InPawn);
}

void ADaPlayerController::BeginPlayingState()
{
	Super::BeginPlayingState();

	BlueprintBeginPlayingState();
}

void ADaPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	OnPlayerStateReceived.Broadcast(PlayerState);
}

void ADaPlayerController::TogglePauseMenu()
{
	if (PauseMenuInstance && PauseMenuInstance->IsInViewport())
	{
		PauseMenuInstance->RemoveFromParent();
		PauseMenuInstance = nullptr;

		SetCurrentInputMode(InputType);
		
		//@TODO: Single-player only. Make work for multiplayer.
		// Example issues to resolve: triggering abilities while the game is paused, or releasing your sprint button after pausing the game 
		if (GetWorld()->IsNetMode(NM_Standalone))
		{
			UGameplayStatics::SetGamePaused(this, false);
		}
		
		return;
	}
	
	PauseMenuInstance = CreateWidget<UUserWidget>(this, PauseMenuClass);
	if (PauseMenuInstance)
	{
		PauseMenuInstance->AddToViewport(100);

		SetCurrentInputMode(EGameplayInputType::CursorOnly);

		//@TODO: Single-player only. Make work for multiplayer
		if (GetWorld()->IsNetMode(NM_Standalone))
		{
			UGameplayStatics::SetGamePaused(this, true);
		}
	}
	
}
