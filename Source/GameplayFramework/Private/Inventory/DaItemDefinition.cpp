// Copyright Dream Awake Solutions LLC

#include "Inventory/DaItemDefinition.h"
#include "AbilitySystem/DaAbilitySet.h"

FPrimaryAssetId UDaItemDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId("ItemDefinition", GetFName());
}
