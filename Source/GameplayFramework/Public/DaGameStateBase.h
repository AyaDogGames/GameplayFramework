// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/GameStateBase.h"
#include "DaGameStateBase.generated.h"

/**
 * 
 */
UCLASS()
class GAMEPLAYFRAMEWORK_API ADaGameStateBase : public AGameStateBase, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:

	ADaGameStateBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// Gets the ability system component used for game wide things
	UFUNCTION(BlueprintCallable, Category = "DAGameState")
	UDaAbilitySystemComponent* GetDaAbilitySystemComponent() const { return AbilitySystemComponent; }

protected:

	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// How much game time passes every real world second. Default is 1 which means game time matches real world time. If set to 10 for example, then 10 minutes passes every second.
	UPROPERTY(EditDefaultsOnly, Replicated, Category = "DAGameState")
	float GameTimeInterval;

private:

	// The ability system component subobject for game-wide things (primarily gameplay cues)
	UPROPERTY(VisibleAnywhere, Category = "DAGameState")
	TObjectPtr<UDaAbilitySystemComponent> AbilitySystemComponent;

	virtual void PostInitializeComponents() override;
};
