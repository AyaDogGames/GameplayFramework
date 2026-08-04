// Copyright Dream Awake Solutions LLC

#include "Equipment/DaEquipmentList.h"

#include "Equipment/DaEquipmentManagerComponent.h"

void FDaAppliedEquipmentEntry::PreReplicatedRemove(const FDaEquipmentList& InArraySerializer)
{
	if (InArraySerializer.OwnerComponent)
	{
		InArraySerializer.OwnerComponent->HandleUnequipped(*this);
	}
}

void FDaAppliedEquipmentEntry::PostReplicatedAdd(const FDaEquipmentList& InArraySerializer)
{
	if (InArraySerializer.OwnerComponent)
	{
		InArraySerializer.OwnerComponent->HandleEquipped(*this);
	}
}

void FDaAppliedEquipmentEntry::PostReplicatedChange(const FDaEquipmentList& InArraySerializer)
{
}
