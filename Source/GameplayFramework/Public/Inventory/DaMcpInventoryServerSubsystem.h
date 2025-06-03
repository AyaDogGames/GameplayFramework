#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DaInventoryItemBase.h"
#include "DaMcpInventoryServerSubsystem.generated.h"

/**
 * Simple subsystem that sends inventory updates to a remote MCP server.
 */
UCLASS()
class GAMEPLAYFRAMEWORK_API UDaMcpInventoryServerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Send request to add an item to the server controlled inventory. */
    UFUNCTION(BlueprintCallable, Category="MCP")
    void AddItemToServer(AActor* Owner, const FDaInventoryItemData& ItemData);

    /** Send request to remove an item from the server controlled inventory. */
    UFUNCTION(BlueprintCallable, Category="MCP")
    void RemoveItemFromServer(AActor* Owner, const FGuid& ItemID);

    /** Query inventory from the server for the specified owner. */
    UFUNCTION(BlueprintCallable, Category="MCP")
    void QueryInventoryFromServer(AActor* Owner);

protected:
    void SendRequest(const FString& Url, const FString& Verb, const FString& Payload);
};

