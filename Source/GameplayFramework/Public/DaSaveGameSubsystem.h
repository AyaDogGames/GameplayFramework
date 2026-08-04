// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DaSaveGameSubsystem.generated.h"

class UMVVMViewModelBase;
class UDaSaveGame;
class APlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSaveGameSignature, class UDaSaveGame*, SaveObject);

/**
 * 
 */
UCLASS(meta=(DisplayName="SaveGame System"))
class GAMEPLAYFRAMEWORK_API UDaSaveGameSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	
	void SaveSlotData(const FString& LoadSlotName, int32 SlotIndex, bool bClearExisting, TFunction<void(UDaSaveGame*)> SaveDataCallback);
	
	UDaSaveGame* GetSaveSlotData(const FString& SlotName, int32 SlotIndex) const;
	static void DeleteSlot(const FString& SlotName, int32 SlotIndex);

	UDaSaveGame* RetrieveInGameSaveData() const;
	void SaveInGameProgressData(TFunction<void(UDaSaveGame*)> SaveDataCallback);

	void DebugLogCurrentSaveGameInfo(const FString& AdditionalLoggingText);
	
	// Restore Serialized data from PlayerState into Player
	void HandleStartingNewPlayer(AController* NewPlayer);

	// Restore Spawn Transform using stored data per PlayerState after being fully initialized
	UFUNCTION(BlueprintCallable)
	bool OverrideSpawnTransform(AController* NewPlayer);
	
	UFUNCTION(BlueprintCallable, Category="SaveGame")
	void WriteSaveGame();

	// Load from disk, optional slot name
	UFUNCTION(BlueprintCallable, Category="SaveGame")
	void LoadSaveGame(FString InSlotName="", int32 InSlotIndex=0);

	/** Re-apply the loaded save data to a live PlayerState (what HandleStartingNewPlayer does
	 *  on possess). Exposed so a save/load round trip can be driven without a respawn. */
	UFUNCTION(BlueprintCallable, Category="SaveGame")
	bool ReloadPlayerState(APlayerState* PlayerState);

	UPROPERTY(BlueprintAssignable)
	FOnSaveGameSignature OnSaveGameLoaded;

	UPROPERTY(BlueprintAssignable)
	FOnSaveGameSignature OnSaveGameWritten;

	// initialize subsystem, good moment to load in SaveGameSettings variables
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

protected:
	
	UPROPERTY()
	TObjectPtr<UDaSaveGame> CurrentSaveGame;
	
};
