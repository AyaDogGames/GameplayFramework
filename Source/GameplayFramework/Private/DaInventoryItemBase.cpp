// Copyright Dream Awake Solutions LLC


#include "DaInventoryItemBase.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/DaAbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"

ADaInventoryItemBase::ADaInventoryItemBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	
	AbilitySystemComponent = CreateDefaultSubobject<UDaAbilitySystemComponent>("AbilitySystemComp");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}


UAbilitySystemComponent* ADaInventoryItemBase::GetAbilitySystemComponent() const
{
	return Cast<UAbilitySystemComponent>(AbilitySystemComponent);
}
