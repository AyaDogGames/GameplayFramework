// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "DaGameInstanceBase.generated.h"

class UDaSaveGame;
/**
 * 
 */
UCLASS()
class GAMEPLAYFRAMEWORK_API UDaGameInstanceBase : public UGameInstance
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SaveGame")
	TSubclassOf<UDaSaveGame> SaveGameClass;
	
	UPROPERTY()
	FName PlayerStartTag = FName();

	/* Save slot the game mode loads on InitGame and writes on autosave. Blueprint-writable so
	 * slot selection (main menu, save/load screen) can set it before travel. */
	UPROPERTY(BlueprintReadWrite, Category = "SaveGame")
	FString LoadSlotName = FString();

	UPROPERTY(BlueprintReadWrite, Category = "SaveGame")
	int32 LoadSlotIndex = 0;
	
};
