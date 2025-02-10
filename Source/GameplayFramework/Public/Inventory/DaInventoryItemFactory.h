// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "DaInventoryItemFactory.generated.h"

struct FDaInventoryItemData;
class UDaInventoryItemBase;

UINTERFACE(MinimalAPI)
class UDaInventoryItemFactory : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class GAMEPLAYFRAMEWORK_API IDaInventoryItemFactory
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<UDaInventoryItemBase> DetermineInventoryItemClassFromTags(const FGameplayTagContainer& Tags) const = 0;
	virtual TSubclassOf<UDaInventoryItemBase> DetermineInventoryItemClass(const UObject* SourceObject) const = 0;
	virtual UDaInventoryItemBase* CreateInventoryItem(const UObject* SourceObject) const = 0;
	
};

UCLASS()
class GAMEPLAYFRAMEWORK_API UDaBaseInventoryItemFactory : public UObject, public IDaInventoryItemFactory
{
	GENERATED_BODY()

public:
	virtual TSubclassOf<UDaInventoryItemBase> DetermineInventoryItemClassFromTags(const FGameplayTagContainer& Tags) const override;
	virtual TSubclassOf<UDaInventoryItemBase> DetermineInventoryItemClass(const UObject* SourceObject) const override;
	virtual UDaInventoryItemBase* CreateInventoryItem(const UObject* SourceObject) const override;
};