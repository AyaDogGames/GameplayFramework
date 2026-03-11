// Fill out your copyright notice in the Description page of Project Settings.


#include "CECollectibleProxy.h"

#include "CECollectibleData.h"
#include "GameFramework/PlayerState.h"
#include "Inventory/DaInventoryComponent.h"


UCECollectibleProxy* UCECollectibleProxy::CreateCollectibleProxyFromDataRef(const FCECollectibleDataDef* DataRef,
                                                                            const TSubclassOf<AActor>& ClassToSpawn)
{
	if (DataRef == nullptr)
	{
		return nullptr;
	}

	UCECollectibleProxy* CollectibleProxy = NewObject<UCECollectibleProxy>();
	CollectibleProxy->CollectibleDataRef = *DataRef;
	CollectibleProxy->CollectibleActorClass = ClassToSpawn;
	return CollectibleProxy;
}

void UCECollectibleProxy::AddToInventory_Implementation(APawn* InstigatorPawn, bool bDestroyActor)
{
	if (!InstigatorPawn)
		return;

	APlayerState* PS = InstigatorPawn->GetPlayerState();
	if (PS)
	{
		UDaInventoryComponent* InvComp = UDaInventoryComponent::GetInventoryFromActor(PS);
		if (InvComp)
		{
			InvComp->AddItem(ItemDefinitionID);
		}
	}
}
