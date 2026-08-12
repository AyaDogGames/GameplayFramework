// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "DaConditionBand.generated.h"

/**
 * Which penalty an item's Condition currently earns it (see FDaConditionConfig).
 *
 * Its own header on purpose: the authority-side owner of the concept is
 * UDaEquipmentManagerComponent (which applies the band's penalty effect), but read-only consumers
 * — the UI view-model, a shop or repair widget, the item debug overlay — want the enum without
 * the equipment component's replication machinery. UDaEquipmentManagerComponent::ComputeConditionBand
 * stays the single function that decides which band a number falls into.
 */
UENUM(BlueprintType)
enum class EDaConditionBand : uint8
{
	Normal,
	Worn,
	Critical,
	/** Condition 0: the item is inert. It auto-unequips and cannot be re-equipped until repaired. */
	Broken
};
