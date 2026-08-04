// Copyright Dream Awake Solutions LLC


#include "AbilitySystem/Effects/DaGameplayEffect_ConditionWorn.h"

#include "CoreGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UDaGameplayEffect_ConditionWorn::UDaGameplayEffect_ConditionWorn()
{
	// Infinite, not HasDuration: the equipment manager owns the lifetime and removes the handle
	// when the band changes or the slot empties. (An Instant effect could not grant tags at all.)
	DurationPolicy = EGameplayEffectDurationType::Infinite;
}

void UDaGameplayEffect_ConditionWorn::PostInitProperties()
{
	Super::PostInitProperties();

	// UE 5.3 moved granted tags off UGameplayEffect and into a component; the deprecated
	// InheritableOwnedTagsContainer is no longer read when the effect is applied.
	//
	// This cannot go in the constructor: UGameplayEffect::GEComponents is private, so
	// FindOrAddComponent is the only way in, and it calls NewObject with an empty name — fatal
	// inside a UObject constructor ("NewObject with empty name can't be used to create default
	// subobjects"). FObjectInitializer leaves the constructor scope before it calls
	// PostInitProperties, which makes this the earliest legal point.
	if (IsInAsyncLoadingThread() || IsInParallelLoadingThread())
	{
		// A serialized subclass brings its own components in through serialization.
		return;
	}

	UTargetTagsGameplayEffectComponent& TargetTags = FindOrAddComponent<UTargetTagsGameplayEffectComponent>();
	FInheritedTagContainer TagChanges = TargetTags.GetConfiguredTargetTagChanges();
	TagChanges.AddTag(CoreGameplayTags::TAG_Item_Condition_Worn);
	TargetTags.SetAndApplyTargetTagChanges(TagChanges);
}
