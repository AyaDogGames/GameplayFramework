// Copyright Dream Awake Solutions LLC


#include "UI/DaStatMenuWidgetController.h"

#include "CoreGameplayTags.h"
#include "AbilitySystem/DaAbilitySystemComponent.h"
#include "AbilitySystem/DaAbilitySystemLibrary.h"
#include "AbilitySystem/Attributes/DaBaseAttributeSet.h"

FGameplayTagContainer UDaStatMenuWidgetController::GetPrimaryAttributeTags()
{
	return GetAttributeTags(CoreGameplayTags::AttributesStatsPrimary);
}

FGameplayTagContainer UDaStatMenuWidgetController::GetSecondaryAttributeTags()
{
	return GetAttributeTags(CoreGameplayTags::AttributesStatsSecondary);
}

FGameplayTagContainer UDaStatMenuWidgetController::GetAttributeTags(const FGameplayTag& ParentTag) const
{
	int32 FoundTags = 0;
	FGameplayTagContainer AttributeTags;
	for (auto It = AttributeSetTags.CreateConstIterator(); It; ++It)
	{
		if (!It->IsValid()) continue;
		
		if (UDaBaseAttributeSet* AttributeSet = AbilitySystemComponent->GetAttributeSetForTag(*It))
		{
			FoundTags += UDaAbilitySystemLibrary::GetAttributesWithParentTag(ParentTag, AttributeSet->GetAttributeTags(), AttributeTags);
		}
	}

	if (FoundTags == 0)
	{
		LOG_WARNING("UDaStatMenuWidgetController: No Tags with Parent tag %s Found", *ParentTag.ToString());
	}
	
	return AttributeTags;
}
