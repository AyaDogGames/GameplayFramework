// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DaGameModeBase.generated.h"

class UDaSaveGame;
class UDaSaveGameSubsystem;
class AController;

/**
 * 
 */
UCLASS(Config = Game)
class GFCORE_API ADaGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level")
	FString DefaultMapName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level")
	TSoftObjectPtr<UWorld> DefaultMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level")
	TMap<FString, TSoftObjectPtr<UWorld>> Maps;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SaveGame")
	TSubclassOf<UDaSaveGameSubsystem> SaveGameSubsystemClass;

	UPROPERTY()
	UDaSaveGameSubsystem* SaveGameSubsystem;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayerStart")
	FName DefaultPlayerStartTag;
	
	UFUNCTION(BlueprintCallable, Category = "Level")
	void TravelToMap(const FString& MapName);

	UFUNCTION(BlueprintCallable, Category = "Level")
	void TravelToMapWithOptions(const FString& MapName, bool bResetOptions, const FString& Options);

	UFUNCTION(BlueprintCallable, Category = "SaveGame")
	void WriteSaveGame();
	
protected:
	
	// if true will auto respawn if false, let subclasses handle how we respawn or end, example might be go to spectator mode or start menu
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Respawn")
	bool bAutoRespawnPlayer;

	// if auto respawn is true, how long to wait between respawns
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Respawn")
	float RespawnDelay;

	// respawn at last save if true or a start point if false
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Respawn")
	bool bRespawnAtLastSaveLocation;
	
	UFUNCTION()
	void RespawnPlayerElapsed(AController* Controller);

	virtual void BeginPlay() override;

	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

public:

	ADaGameModeBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	
	// subclass should call super if override to handle respawn behavior but can check if a player or AI was killed and act accordingly
	virtual void OnActorKilled(AActor* VictimActor, AActor* KillerActor);
	
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	UDaSaveGame* RetrieveInGameSaveData();
	void SaveInGameProgressData(TFunction<void(UDaSaveGame*)> SaveDataCallback);
	
};
