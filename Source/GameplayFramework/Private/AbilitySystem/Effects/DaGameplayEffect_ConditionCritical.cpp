// Copyright Dream Awake Solutions LLC


#include "AbilitySystem/Effects/DaGameplayEffect_ConditionCritical.h"

#include "CoreGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UDaGameplayEffect_ConditionCritical::UDaGameplayEffect_ConditionCritical()
{
	// See UDaGameplayEffect_ConditionWorn for why this is Infinite.
	DurationPolicy = EGameplayEffectDurationType::Infinite;
}

void UDaGameplayEffect_ConditionCritical::PostInitProperties()
{
	Super::PostInitProperties();

	// Tags are configured here rather than in the constructor — see
	// UDaGameplayEffect_ConditionWorn::PostInitProperties for the reason.
	if (IsInAsyncLoadingThread() || IsInParallelLoadingThread())
	{
		return;
	}

	UTargetTagsGameplayEffectComponent& TargetTags = FindOrAddComponent<UTargetTagsGameplayEffectComponent>();
	FInheritedTagContainer TagChanges = TargetTags.GetConfiguredTargetTagChanges();
	TagChanges.AddTag(CoreGameplayTags::TAG_Item_Condition_Critical);
	TargetTags.SetAndApplyTargetTagChanges(TagChanges);
}
