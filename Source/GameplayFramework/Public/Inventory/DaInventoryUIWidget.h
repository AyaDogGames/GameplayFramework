// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "UI/DaUserWidgetBase.h"
#include "DaInventoryUIWidget.generated.h"

class UDaInventoryItemBase;

/**
 * 
 */
UCLASS(Blueprintable, ClassGroup=(Custom))
class GAMEPLAYFRAMEWORK_API UDaInventoryUIWidget : public UDaUserWidgetBase, public IUserObjectListEntry
{
	GENERATED_BODY()

public:

	/* IUserObjectListEntry */
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;
	virtual void NativeOnItemExpansionChanged(bool bIsExpanded) override;
	virtual void NativeOnEntryReleased() override;
	/* IUserObjectListEntry */
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (ExposeOnSpawn="true"))
	UDaInventoryItemBase* InventoryItem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (ExposeOnSpawn="true"))
	FName SlotName;
	
};
