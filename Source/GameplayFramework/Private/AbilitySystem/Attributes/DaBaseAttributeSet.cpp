// Copyright Dream Awake Solutions LLC


#include "AbilitySystem/Attributes/DaBaseAttributeSet.h"

#include "AbilitySystem/DaAbilitySystemComponent.h"

class UWorld;

UDaBaseAttributeSet::UDaBaseAttributeSet()
{
}

UWorld* UDaBaseAttributeSet::GetWorld() const
{
	const UObject* Outer = GetOuter();
	check(Outer);

	return Outer->GetWorld();
}

UDaAbilitySystemComponent* UDaBaseAttributeSet::GetDaAbilitySystemComponent() const
{
	return Cast<UDaAbilitySystemComponent>(GetOwningAbilitySystemComponent());
}

FGameplayTagContainer UDaBaseAttributeSet::GetAttributeTags() const
{
	TArray<FGameplayTag> Tags;
	
	if (TagsToAttributes.GetKeys(Tags) > 0)
	{
		return FGameplayTagContainer::CreateFromArray(Tags);
	}

	return FGameplayTagContainer();
}
