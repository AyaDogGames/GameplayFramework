// Copyright Dream Awake Solutions LLC

#include "Inventory/DaItemDefinition.h"
#include "AbilitySystem/DaAbilitySet.h"
#include "AbilitySystem/Effects/DaGameplayEffect_ConditionCritical.h"
#include "AbilitySystem/Effects/DaGameplayEffect_ConditionWorn.h"

FDaConditionConfig::FDaConditionConfig()
	: WornEffect(UDaGameplayEffect_ConditionWorn::StaticClass())
	, CriticalEffect(UDaGameplayEffect_ConditionCritical::StaticClass())
{
}

FPrimaryAssetId UDaItemDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId("ItemDefinition", GetFName());
}
