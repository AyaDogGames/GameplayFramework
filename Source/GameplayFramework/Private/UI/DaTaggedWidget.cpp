// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/DaTaggedWidget.h"

UDaTaggedWidget::UDaTaggedWidget(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void UDaTaggedWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!IsDesignTime())
	{
		// Listen for tag changes on our hidden tags
		//@TODO: That thing I said

		// Set our initial visibility value (checking the tags, etc...)
		SetVisibility(GetVisibility());
	}
}

void UDaTaggedWidget::NativeDestruct()
{
	if (!IsDesignTime())
	{
		//@TODO: Stop listening for tag changes
	}
	
	Super::NativeDestruct();
}

void UDaTaggedWidget::SetVisibility(ESlateVisibility InVisibility)
{
#if WITH_EDITORONLY_DATA
	if (IsDesignTime())
	{
		Super::SetVisibility(InVisibility);
		return;
	}
#endif

	// Remember what the caller requested; even if we're currently being
	// suppressed by a tag we should respect this call when we're done
	bWantsToBeVisible = ConvertSerializedVisibilityToRuntime(InVisibility).IsVisible();
	if (bWantsToBeVisible)
	{
		ShownVisibility = InVisibility;
	}
	else
	{
		HiddenVisibility = InVisibility;
	}

	//@TODO: Foo->HasAnyTags(HiddenByTags);
	const bool bHasHiddenTags = false;

	// Actually apply the visibility
	const ESlateVisibility DesiredVisibility = (bWantsToBeVisible && !bHasHiddenTags) ? ShownVisibility : HiddenVisibility;
	if (GetVisibility() != DesiredVisibility)
	{
		Super::SetVisibility(DesiredVisibility);
	}
}

void UDaTaggedWidget::OnWatchedTagsChanged()
{
	//@TODO: Foo->HasAnyTags(HiddenByTags);
	const bool bHasHiddenTags = false;

	// Actually apply the visibility
	const ESlateVisibility DesiredVisibility = (bWantsToBeVisible && !bHasHiddenTags) ? ShownVisibility : HiddenVisibility;
	if (GetVisibility() != DesiredVisibility)
	{
		Super::SetVisibility(DesiredVisibility);
	}
}
