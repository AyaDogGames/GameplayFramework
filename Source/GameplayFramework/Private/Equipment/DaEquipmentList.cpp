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
	// SpawnedActors references that were unmapped when the entry first arrived (the actor
	// channel had not opened yet) resolve through a later change delta, and this is the only
	// notification clients get for it.
	if (InArraySerializer.OwnerComponent)
	{
		InArraySerializer.OwnerComponent->HandleChanged(*this);
	}
}
