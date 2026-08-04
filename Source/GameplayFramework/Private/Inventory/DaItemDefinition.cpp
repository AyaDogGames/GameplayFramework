// Copyright Dream Awake Solutions LLC

#include "Inventory/DaItemDefinition.h"
#include "AbilitySystem/DaAbilitySet.h"
#include "AbilitySystem/Effects/DaGameplayEffect_ConditionCritical.h"
#include "AbilitySystem/Effects/DaGameplayEffect_ConditionWorn.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "DaItemDefinition"

FDaConditionConfig::FDaConditionConfig()
	: WornEffect(UDaGameplayEffect_ConditionWorn::StaticClass())
	, CriticalEffect(UDaGameplayEffect_ConditionCritical::StaticClass())
{
}

FPrimaryAssetId UDaItemDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId("ItemDefinition", GetFName());
}

#if WITH_EDITOR
EDataValidationResult UDaItemDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (ConditionConfig.bUsesCondition)
	{
		// The cap is what Condition is filled to at acquisition and repaired up to. A cap of 0 means
		// every instance is created at 0, and 0 is Broken: the item can never be equipped, and no
		// amount of credits can repair it.
		const int32 Cap = ConditionConfig.GetConditionCap(ConditionConfig.DefaultGrade);
		if (Cap <= 0)
		{
			Context.AddError(FText::Format(
				LOCTEXT("ConditionCapZero", "bUsesCondition is set but the grade-{0} condition cap works out to {1} "
					"(CapBase {2} + CapPerGrade {3} * grade). Every instance would be created Broken and could never "
					"be equipped or repaired."),
				ConditionConfig.DefaultGrade, Cap, ConditionConfig.CapBase, ConditionConfig.CapPerGrade));
			Result = EDataValidationResult::Invalid;
		}

		if (MaxStackCount > 1)
		{
			// Condition is per-instance state on ONE entry; a stack is one entry standing in for many
			// items. The acquisition path stacks first and never stamps condition on what it stacked
			// into, so these items silently arrive without any.
			Context.AddError(FText::Format(
				LOCTEXT("ConditionStackable", "bUsesCondition is set with MaxStackCount={0}. Condition is "
					"per-instance state and cannot be shared by a stack — set MaxStackCount to 1."),
				MaxStackCount));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
