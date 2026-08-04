// Copyright Dream Awake Solutions LLC

#include "Equipment/DaEquipmentList.h"

void FDaAppliedEquipmentEntry::PreReplicatedRemove(const FDaEquipmentList& InArraySerializer)
{
	// Filled in Task 4: forwards to OwnerComponent->HandleUnequipped(*this).
}

void FDaAppliedEquipmentEntry::PostReplicatedAdd(const FDaEquipmentList& InArraySerializer)
{
	// Filled in Task 4: forwards to OwnerComponent->HandleEquipped(*this).
}

void FDaAppliedEquipmentEntry::PostReplicatedChange(const FDaEquipmentList& InArraySerializer)
{
}
