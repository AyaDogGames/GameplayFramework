// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/PlayerController.h"
#include "DaPlayerController.generated.h"

struct FInputActionValue;
class UDaAbilitySystemComponent;
class UDaInputConfig;
class UInputMappingContext;

UENUM(BlueprintType)
enum EGameplayInputType
{
	GameOnly,
	CursorOnly,
	GameAndCursor
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerStateChanged, APlayerState*, NewPlayerState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPawnChanged, APawn*, NewPawn);

/**
 * 
 */
UCLASS()
class GAMEPLAYFRAMEWORK_API ADaPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADaPlayerController();

	UFUNCTION(BlueprintCallable)
	void ToggleIMC(bool bUI);
	
	UFUNCTION(BlueprintCallable)
	void TogglePauseMenu();

	FORCEINLINE UDaInputConfig* GetInputConfig() { return InputConfig; }

	UFUNCTION(BlueprintPure, Category= "DA|Input")
	bool IsUsingGamepad() const
	{
		return bIsUsingGamepad;
	}
	
protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	void AnyKeyInput(FKey PressedKey);

	void SetCurrentInputMode(EGameplayInputType Type);
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	bool bLoadGameUIMappingContextByDefault;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> GameUIInputMappingContext;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> GameplayInputMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta=(AllowPrivateAccess = "true"))
	TObjectPtr<UDaInputConfig> InputConfig;

	virtual void AbilityInputTagPressed(const FInputActionValue& Value, FGameplayTag InputTag);
	virtual void AbilityInputTagReleased(FGameplayTag InputTag);
	virtual void AbilityInputTagHeld(FGameplayTag InputTag);

	UPROPERTY()
	TObjectPtr<UDaAbilitySystemComponent> AbilitySystemComponent;
	UDaAbilitySystemComponent* GetAbilitySystemComponent();

	UPROPERTY(EditAnywhere, Category=Input)
	TEnumAsByte<EGameplayInputType> InputType;
	
	UPROPERTY(EditDefaultsOnly, Category=Input)
	TSubclassOf<UUserWidget> PauseMenuClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> PauseMenuInstance;

	// Listen for incoming player state (for clients this may be nullptr when initially joining a game, 
	// afterwards player state will not change again as PlayerControllers maintain the same player state throughout the level)
	UPROPERTY(BlueprintAssignable)
	FOnPlayerStateChanged OnPlayerStateReceived;

	UPROPERTY(BlueprintAssignable)
	FOnPawnChanged OnPawnChanged;

	virtual void SetPawn(APawn* InPawn) override;
	
	/* Called when player controller is ready to begin playing, good moment to initialize things like UI which might be too early in BeginPlay 
		(esp. in multiplayer clients where not all data such as PlayerState may have been received yet) */
	virtual void BeginPlayingState() override;

	UFUNCTION(BlueprintImplementableEvent)
	void BlueprintBeginPlayingState();

	virtual void OnRep_PlayerState() override;

private:

	/* Was any input recently using GamePad */
	bool bIsUsingGamepad;
};
