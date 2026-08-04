// Copyright Dream Awake Solutions LLC


#include "DaSaveGameSubsystem.h"

#include "DaGameInstanceBase.h"
#include "EngineUtils.h"
#include "DaPlayerState.h"
#include "DaSaveGame.h"
#include "DaSaveGameSettings.h"
#include "GameplayFramework.h"
#include "GameFramework/GameStateBase.h"
#include "DaSaveInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

void UDaSaveGameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// TODO: Setup Defaults in settings so they can be done centrally
	// const UDaSaveGameSettings* SGSettings = GetDefault<UDaSaveGameSettings>();
	// Access defaults from DefaultGame.ini
	//LoadSlotName = SGSettings->SaveSlotName;
}

/* Static */
UDaSaveGame* UDaSaveGameSubsystem::GetSaveSlotData(const FString& SlotName, int32 SlotIndex) const
{
	USaveGame* SaveGameObject = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(SlotName, SlotIndex))
	{
		SaveGameObject = UGameplayStatics::LoadGameFromSlot(SlotName, SlotIndex);
	}
	else
	{
		TSubclassOf<UDaSaveGame> SaveGameClass = UDaSaveGame::StaticClass();
		UDaGameInstanceBase* GI = Cast<UDaGameInstanceBase>(GetGameInstance());
		if (GI->SaveGameClass != nullptr)
		{
			SaveGameClass = GI->SaveGameClass;
		} 
		SaveGameObject = UGameplayStatics::CreateSaveGameObject(SaveGameClass);
		//LogOnScreen(GetWorld(), FString::Printf(TEXT("DaSaveGameSubsystem: Created SaveGame: %s"), *GetNameSafe(SaveGameObject)));
	}

	UDaSaveGame* SaveGame = Cast<UDaSaveGame>(SaveGameObject);
	return SaveGame;
}

/* Static */
void UDaSaveGameSubsystem::DeleteSlot(const FString& SlotName, int32 SlotIndex)
{
	if (UGameplayStatics::DoesSaveGameExist(SlotName, SlotIndex))
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, SlotIndex);
	}
}

UDaSaveGame* UDaSaveGameSubsystem::RetrieveInGameSaveData() const
{
	UDaGameInstanceBase* GI = Cast<UDaGameInstanceBase>(GetGameInstance());

	const FString InGameLoadSlotName = GI->LoadSlotName;
	const int32 InGameLoadSlotIndex = GI->LoadSlotIndex;

	return GetSaveSlotData(InGameLoadSlotName, InGameLoadSlotIndex);
}

/* Save */
void UDaSaveGameSubsystem::SaveInGameProgressData(TFunction<void(UDaSaveGame*)> SaveDataCallback)
{
	UDaGameInstanceBase* GI = Cast<UDaGameInstanceBase>(GetGameInstance());

	const FString InGameLoadSlotName = GI->LoadSlotName;
	const int32 InGameLoadSlotIndex = GI->LoadSlotIndex;
	CurrentSaveGame = GetSaveSlotData(InGameLoadSlotName, InGameLoadSlotIndex);

	SaveDataCallback(CurrentSaveGame);

	// Save world and everything else
	WriteSaveGame();
	
	UGameplayStatics::SaveGameToSlot(CurrentSaveGame, InGameLoadSlotName, InGameLoadSlotIndex);
	OnSaveGameWritten.Broadcast(CurrentSaveGame);
}

void UDaSaveGameSubsystem::DebugLogCurrentSaveGameInfo(const FString& AdditionalLoggingText)
{
	//LogOnScreen(this, FString::Printf(TEXT("DaSaveGameSubsystem: SaveGame: %s, %s"), *GetNameSafe(CurrentSaveGame), *AdditionalLoggingText));
}

void UDaSaveGameSubsystem::SaveSlotData(const FString& LoadSlotName, int32 SlotIndex, bool bClearExisting,
                                        TFunction<void(UDaSaveGame*)> SaveDataCallback)
{
	if (UGameplayStatics::DoesSaveGameExist(LoadSlotName, SlotIndex))
	{
		if (bClearExisting)
		{
			UGameplayStatics::DeleteGameInSlot(LoadSlotName, SlotIndex);
		}
	}

	// Load SaveGame Data from slot, if it doesn't exist create new one, either way save it as current
	CurrentSaveGame = GetSaveSlotData(LoadSlotName, SlotIndex);

	SaveDataCallback(CurrentSaveGame);
	UGameplayStatics::SaveGameToSlot(CurrentSaveGame, LoadSlotName, SlotIndex);
	OnSaveGameWritten.Broadcast(CurrentSaveGame);
}

void UDaSaveGameSubsystem::HandleStartingNewPlayer(AController* NewPlayer)
{
	
	ADaPlayerState* PS = NewPlayer->GetPlayerState<ADaPlayerState>();
	if (PS)
	{
		PS->LoadPlayerState(CurrentSaveGame);
	}
}

bool UDaSaveGameSubsystem::OverrideSpawnTransform(AController* NewPlayer)
{
	if (NewPlayer == nullptr || NewPlayer->IsPendingKillPending())
	{
		return false;
	}

	APlayerState* PS = NewPlayer->GetPlayerState<APlayerState>();
	if (PS == nullptr)
	{
		return false;
	}

	if (APawn* MyPawn = PS->GetPawn())
	{
		FPlayerSaveData* FoundData = CurrentSaveGame->GetPlayerData(PS);
		if (FoundData && FoundData->bResumeAtTransform)
		{		
			MyPawn->SetActorLocation(FoundData->Location);
			MyPawn->SetActorRotation(FoundData->Rotation);

			// PlayerState owner is a (Player)Controller
			AController* PC = Cast<AController>(PS->GetOwner());
			// Set control rotation to change camera direction, setting Pawn rotation is not enough
			PC->SetControlRotation(FoundData->Rotation);

			return true;
		}
	}

	return false;
}

bool UDaSaveGameSubsystem::ReloadPlayerState(APlayerState* PlayerState)
{
	ADaPlayerState* PS = Cast<ADaPlayerState>(PlayerState);
	if (PS == nullptr || CurrentSaveGame == nullptr)
	{
		return false;
	}

	PS->LoadPlayerState(CurrentSaveGame);
	return true;
}

void UDaSaveGameSubsystem::WriteSaveGame()
{
	if (CurrentSaveGame == nullptr)
	{
		// Nothing loaded or created yet (no slot name configured); callers that go through
		// SaveInGameProgressData always have one by this point.
		LOG("WriteSaveGame: no CurrentSaveGame to write into.");
		return;
	}

	// Clear arrays, may contain data from previously loaded SaveGame
	CurrentSaveGame->SavedPlayers.Empty();
	CurrentSaveGame->SavedActors.Empty();

	AGameStateBase* GS = GetWorld()->GetGameState();
	if (GS == nullptr)
	{
		// Warn about failure to save?
		return;
	}

	// Iterate all player states, we don't have proper ID to match yet (requires Steam or EOS)
	for (int32 i = 0; i < GS->PlayerArray.Num(); i++)
	{
		ADaPlayerState* PS = Cast<ADaPlayerState>(GS->PlayerArray[i]);
		if (PS)
		{
			PS->SavePlayerState(CurrentSaveGame);
			break; // single player only at this point
		}
	}

	// Iterate the entire world of actors
	for (FActorIterator It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		
		// Only interested in our 'actors with a saveable interface', skip actors that are being destroyed
		if (Actor->IsPendingKillPending() || !Actor->Implements<UDaSaveInterface>())
		{
			continue;
		}

		FActorSaveData ActorData;
		ActorData.ActorName = Actor->GetFName();
		ActorData.Transform = Actor->GetActorTransform();

		// Pass the array to fill with data from Actor
		FMemoryWriter MemWriter(ActorData.ByteData);

		FObjectAndNameAsStringProxyArchive Ar(MemWriter, true);
		// Find only variables with UPROPERTY(SaveGame)
		Ar.ArIsSaveGame = true;
		// Converts Actor's SaveGame UPROPERTIES into binary array
		Actor->Serialize(Ar);

		CurrentSaveGame->SavedActors.Add(ActorData);
	}
}

void UDaSaveGameSubsystem::LoadSaveGame(FString InSlotName, int32 SlotIndex)
{
	// Update slot name first if specified, otherwise keeps default name

	if (UGameplayStatics::DoesSaveGameExist(InSlotName, SlotIndex))
	{
		CurrentSaveGame = Cast<UDaSaveGame>(UGameplayStatics::LoadGameFromSlot(InSlotName, SlotIndex));
		if (CurrentSaveGame == nullptr)
		{
			LOG("Failed to load SaveGame Data.");
			return;
		}

		LOG("Loaded SaveGame Data.");
		
		// Iterate the entire world of actors
		for (FActorIterator It(GetWorld()); It; ++It)
		{
			AActor* Actor = *It;
			
			// Only interested in our 'gameplay actors'
			if (!Actor->Implements<UDaSaveInterface>())
			{
				continue;
			}

			for (FActorSaveData ActorData : CurrentSaveGame->SavedActors)
			{
				if (ActorData.ActorName == Actor->GetFName())
				{
					if (IDaSaveInterface::Execute_ShouldLoadTransform(Actor))
					{
						Actor->SetActorTransform(ActorData.Transform);
					}
					
					FMemoryReader MemReader(ActorData.ByteData);

					FObjectAndNameAsStringProxyArchive Ar(MemReader, true);
					Ar.ArIsSaveGame = true;
					// Convert binary array back into actor's variables
					Actor->Serialize(Ar);

					IDaSaveInterface::Execute_LoadActor(Actor);
				}
			}
		}

		OnSaveGameLoaded.Broadcast(CurrentSaveGame);
	}
	else
	{
		TSubclassOf<UDaSaveGame> SaveGameClass = UDaSaveGame::StaticClass();
		UDaGameInstanceBase* GI = Cast<UDaGameInstanceBase>(GetGameInstance());
		if (GI->SaveGameClass != nullptr)
		{
			SaveGameClass = GI->SaveGameClass;
		} 
		CurrentSaveGame = Cast<UDaSaveGame>(UGameplayStatics::CreateSaveGameObject(SaveGameClass));
	}
}
