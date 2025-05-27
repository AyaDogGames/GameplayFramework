// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "DaCharacterInterface.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Character.h"
#include "DaCharacterBase.generated.h"

class UGameplayEffect;
class UDaAbilitySystemComponent;
class UDaAttributeComponent;
class UDaWorldUserWidget;

UCLASS()
class GFCORE_API ADaCharacterBase : public ACharacter, public IAbilitySystemInterface, public IDaCharacterInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ADaCharacterBase();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// Set up Ability system component and attribute component which contains this characters Vital attributes
	virtual void InitAbilitySystem();

	// fire off an event based gameplay ability of attribute tag and amount. Ability needs to have been previously activated and listening for GameplayEvent
	UFUNCTION(BlueprintCallable)
	void UpgradeAttribute(const FGameplayTag& AttributeTag, int32 Amount);

	// Fires off the DefaultPrimaryAttributes GameplayEffect which should be setup to initialize any attributes passed in to the character's pawn data AbilitySet
	UFUNCTION(BlueprintCallable)
	void ApplyEffectToSelf(const TSubclassOf<UGameplayEffect>& GameplayEffectClass, const float Level) const;

	// Resets all default attributes using default gameplay effects for primary, secondary and vital stats (if available)
	UFUNCTION(BlueprintCallable)
	void InitDefaultAttributes() const;
	
	virtual UAnimMontage* GetAttackMontage_Implementation() override;
	
	// Will check the default mesh for ProjectileSocketName and return that. Subclasses can override and use their own mesh like a weapon if they have one.
	virtual FVector GetProjectileSocketLocation_Implementation() override;

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UDaAttributeComponent> AttributeComponent;

	// Player Characters will get this from Player State, NPC subclasses *MUST* create it in their constructors
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UDaAbilitySystemComponent> AbilitySystemComponent;

	// Gameplay effect used to set all Vital attributes in CharacterAttributeSet which are used by the AttributeComponent on a Character.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DA|Attributes")
	TSubclassOf<UGameplayEffect> DefaultVitalAttributes;
	
	// Gameplay effect used to set all (non-vital) attributes (provided in an AbilitySet) to default.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DA|Attributes")
	TSubclassOf<UGameplayEffect> DefaultPrimaryAttributes;

	// Gameplay effect used to set all (non-vital) attributes (provided in an AbilitySet) to default.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DA|Attributes")
	TSubclassOf<UGameplayEffect> DefaultSecondaryAttributes;

	// If using projectile abilities, subclasses will use this to find the location to spawn a projectile from
	UPROPERTY(EditAnywhere, Category = "DA|Combat")
	FName ProjectileSocketName;

	UPROPERTY(EditAnywhere, Category = "DA|Combat")
	TObjectPtr<UAnimMontage> AttackMontage;
	
	// Calls InitAbilitySystem for setup of player characters on server and client
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	// Calls InitAbilitySystem for setup of non-player characters (like AI NPCs) on server only
	virtual void BeginPlay() override;

	UFUNCTION()
	virtual void OnHealthChanged(UDaAttributeComponent* HealthComponent, float OldHealth, float NewHealth, AActor* InstigatorActor);
	
	UFUNCTION()
	virtual void OnDeathStarted(AActor* OwningActor, AActor* InstigatorActor);
	
	UFUNCTION()
	virtual void OnDeathFinished(AActor* OwningActor);
	
	/* Can be used to identify the type of character. Defaults to Character.Type, but subclass such as AI or Player bases classes can set Character.Type.AI, or Character.Type.Player,
	 * subclasses could get more specific like Character.Type.Player.Spaceship or Character.Type.AI.Enemy but these tags will need to be defined as needed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DA|GameplayTags")
	FGameplayTag CharacterTypeGameplayTag;

	/* Can be used to identify a specific avatar actor character (and possibly what skin they are using) like Character.ID.Gideon.Inquisitor. Will be set to a default Character.ID tag if unspecified. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DA|GameplayTags")
	FGameplayTag CharacterIDGameplayTag;

	UPROPERTY(VisibleDefaultsOnly, Category="DA|UI")
	TObjectPtr<UDaWorldUserWidget> ActiveHealthBar;

	UPROPERTY(EditDefaultsOnly, Category="DA|UI")
	TSubclassOf<UUserWidget> HealthBarWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category="DA|UI")
	TSubclassOf<UUserWidget> DamagePopUpWidgetClass;
	
	// Enabled by default. Disable if characters are not using the default hitflash material function so we don't try to set material params when hit
	UPROPERTY(EditAnywhere, Category="DA|Effects")
	bool bUseDefaultHitFlash;
	
	// Parameter to trigger flash effect for MF_HitFlash Material Function
	UPROPERTY(VisibleAnywhere, Category="DA|Effects")
	FName HitFlashTimeParamName;

	// Parameter to set color for flash effect for MF_HitFlash Material Function
	UPROPERTY(VisibleAnywhere, Category="DA|Effects")
	FName HitFlashColorParamName;
	
	// Subclasses can create and install the ActiveHealthBar as needed, usually after taking damage the first time
	UFUNCTION(BlueprintCallable)
	void ShowSetHealthBarWidget();

	UFUNCTION(BlueprintCallable)
	void ShowDamagePopupWidget(float Damage);
};
