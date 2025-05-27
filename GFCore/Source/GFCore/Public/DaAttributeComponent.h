// Copyright Dream Awake Solutions LLC

#pragma once

#include "Components/ActorComponent.h"
#include "DaAttributeComponent.generated.h"

class UDaAttributeComponent;
class UObject;
class AActor;
class UDaCharacterAttributeSet;
class UDaAbilitySystemComponent;

struct FGameplayEffectSpec;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDaHealth_DeathFinishedEvent, AActor*, OwningActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDaHealth_DeathStartedEvent, AActor*, OwningActor, AActor*, InstigatorActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FDaAttributeChanged, UDaAttributeComponent*, HealthComponent, float, OldValue, float, NewValue, AActor*, Instigator);

/**
 * EDaDeathState
 *
 *	Defines current state of death.
 */
UENUM(BlueprintType)
enum class EDaDeathState : uint8
{
	NotDead = 0,
	DeathStarted,
	DeathFinished
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GFCORE_API UDaAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UDaAttributeComponent();

	// Returns the health component if one exists on the specified actor.
	UFUNCTION(BlueprintPure, Category = "DA|Attributes")
	static UDaAttributeComponent* FindAttributeComponent(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<UDaAttributeComponent>() : nullptr); }

	UFUNCTION(BlueprintCallable, Category = "DA|Attributes")
	static bool IsActorAlive(AActor* Actor);
	
	// Initialize the component using an ability system component.
	UFUNCTION(BlueprintCallable, Category = "DA|Attributes")
	void InitializeWithAbilitySystem(UDaAbilitySystemComponent* InASC);

	// Uninitialize the component, clearing any references to the ability system.
	UFUNCTION(BlueprintCallable, Category = "DA|Attributes")
	void UninitializeFromAbilitySystem();

	// Returns the current health value.
	UFUNCTION(BlueprintCallable, Category = "DA|Attributes")
	float GetHealth() const;

	// Returns the current maximum health value.
	UFUNCTION(BlueprintCallable, Category = "DA|Attributes")
	float GetMaxHealth() const;

	// Returns the current maximum health value.
	UFUNCTION(BlueprintCallable, Category = "DA|Attributes")
	bool IsAtMaxHealth() const;
	
	// Returns the current health in the range [0.0, 1.0].
	UFUNCTION(BlueprintCallable, Category = "DA|Attributes")
	float GetHealthNormalized() const;

	// Returns the current health value.
	UFUNCTION(BlueprintCallable, Category = "DA|Attributes")
	float GetMana() const;

	// Returns the current maximum health value.
	UFUNCTION(BlueprintCallable, Category = "DA|Attributes")
	float GetMaxMana() const;
	
	UFUNCTION(BlueprintCallable, Category = "DA|Attributes")
	EDaDeathState GetDeathState() const { return DeathState; }

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "DA|Attributes", Meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool IsDeadOrDying() const { return (DeathState > EDaDeathState::NotDead); }
	
	// Begins the death sequence for the owner.
	virtual bool StartDeath(AActor* InstigatorActor);

	// Ends the death sequence for the owner.
	virtual void FinishDeath();

	// Applies enough damage to kill the owner.
	virtual void DamageSelfDestruct(bool bFellOutOfWorld = false);
	
	// Delegate fired when the health value has changed. This is called on the client but the instigator may not be valid
	UPROPERTY(BlueprintAssignable)
	FDaAttributeChanged OnHealthChanged;

	// Delegate fired when the max health value has changed. This is called on the client but the instigator may not be valid
	UPROPERTY(BlueprintAssignable)
	FDaAttributeChanged OnMaxHealthChanged;

	// Delegate fired when the health value has changed. This is called on the client but the instigator may not be valid
	UPROPERTY(BlueprintAssignable)
	FDaAttributeChanged OnManaChanged;

	// Delegate fired when the max health value has changed. This is called on the client but the instigator may not be valid
	UPROPERTY(BlueprintAssignable)
	FDaAttributeChanged OnMaxManaChanged;
	
	// Delegate fired when the death sequence has started.
	UPROPERTY(BlueprintAssignable)
	FDaHealth_DeathStartedEvent OnDeathStarted;

	// Delegate fired when the death sequence has finished.
	UPROPERTY(BlueprintAssignable)
	FDaHealth_DeathFinishedEvent OnDeathFinished;

protected:

	virtual void OnUnregister() override;

	void ClearGameplayTags();

	virtual void HandleHealthChanged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue);
	virtual void HandleMaxHealthChanged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue);
	virtual void HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue);
	virtual void HandleManaChanged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue);
	virtual void HandleMaxManaChanged(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastOnHealthChanged(AActor* Owner, AActor* DamageInstigator, float OldHealth, float NewHealth);
	
	
	UFUNCTION()
	virtual void OnRep_DeathState(EDaDeathState OldDeathState);

protected:

	// Ability system used by this component.
	UPROPERTY()
	TObjectPtr<UDaAbilitySystemComponent> AbilitySystemComponent;

	// Attribute set used by this component.
	UPROPERTY()
	TObjectPtr<const UDaCharacterAttributeSet> AttributeSet;

	// Replicated state used to handle dying.
	UPROPERTY(ReplicatedUsing = OnRep_DeathState)
	EDaDeathState DeathState;
	
};
