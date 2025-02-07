// Copyright Dream Awake Solutions LLC


#include "DaGameStateBase.h"

#include "../../../../../../../../../../Program Files/Epic Games/UE_5.5/Engine/Plugins/Runtime/ModelViewViewModel/Source/ModelViewViewModel/Public/MVVMGameSubsystem.h"
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

	// if (UGameInstance* GameInstance = GetGameInstance())
	// {
	// 	if (UMVVMGameSubsystem* ModelViewSubsystem = UGameInstance::GetSubsystem<UMVVMGameSubsystem>(GameInstance))
	// 	{
	// 		// Setup any global Data for UI to display here, such as list of players etc.
	// 		
	// 	}
	// }
}
