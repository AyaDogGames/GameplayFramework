// Copyright Dream Awake Solutions LLC


#include "DaGameStateBase.h"

#include "AbilitySystem/DaAbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"

ADaGameStateBase::ADaGameStateBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilitySystemComponent = ObjectInitializer.CreateDefaultSubobject<UDaAbilitySystemComponent>(this, TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	GameTimeInterval = 1.0f;
}

UAbilitySystemComponent* ADaGameStateBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ADaGameStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(ADaGameStateBase, GameTimeInterval, COND_None, REPNOTIFY_Always);
}

void ADaGameStateBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	check(AbilitySystemComponent);
	AbilitySystemComponent->InitAbilityActorInfo(/*Owner=*/ this, /*Avatar=*/ this);

}
