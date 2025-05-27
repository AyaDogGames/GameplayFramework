// Copyright Dream Awake Solutions LLC

#include "DaGameModeBase.h"
#include "DaCharacter.h"
#include "DaGameInstanceBase.h"
#include "DaPlayerState.h"
#include "DaSaveGame.h"
#include "DaSaveGameSubsystem.h"
#include "GameplayFramework.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

ADaGameModeBase::ADaGameModeBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoRespawnPlayer = true;
	bRespawnAtLastSaveLocation = false;
	RespawnDelay = 3.0f;
	SaveGameSubsystemClass = UDaSaveGameSubsystem::StaticClass();
	SaveGameSubsystem = nullptr;
}

void ADaGameModeBase::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	LOG_WARNING("ADaGameModeBase::InitGame Options: %s", *Options);

	if (UDaGameInstanceBase* GameInstance = Cast<UDaGameInstanceBase>(GetGameInstance()))
	{
		SaveGameSubsystem = Cast<UDaSaveGameSubsystem>(GameInstance->GetSubsystemBase(SaveGameSubsystemClass));
		if (SaveGameSubsystem)
		{
			FString SelectedSaveSlot = UGameplayStatics::ParseOption(Options, "SaveGame");
			if (!SelectedSaveSlot.IsEmpty())
			{
				LogOnScreen(this, FString::Printf(TEXT("Loading SelectedSaveSlot from options: %s"), *SelectedSaveSlot));
				SaveGameSubsystem->LoadSaveGame(SelectedSaveSlot);
			}
			else if (!GameInstance->LoadSlotName.IsEmpty())
			{
				LogOnScreen(this, FString::Printf(TEXT("Loading SaveData From GameInstance: %s"), *GameInstance->LoadSlotName));
				SaveGameSubsystem->LoadSaveGame(GameInstance->LoadSlotName, GameInstance->LoadSlotIndex);
			}
			SaveGameSubsystem->DebugLogCurrentSaveGameInfo(FString(TEXT(">>> ADaGameModeBase::InitGame")));

		}
		else
		{
			ErrorMessage = FString::Printf(TEXT("SaveGame Subsystem not found in GameInstance: %s"), *GetNameSafe(GetGameInstance()));
		}
	}
}

inline void ADaGameModeBase::BeginPlay()
{
	Super::BeginPlay();
	
	Maps.Add(DefaultMapName, DefaultMap);

	SaveGameSubsystem->DebugLogCurrentSaveGameInfo(FString(TEXT(">>> ADaGameModeBase::BeginPlay")));
}

AActor* ADaGameModeBase::ChoosePlayerStart_Implementation(AController* Player)
{
	if (UDaGameInstanceBase* GameInstance = Cast<UDaGameInstanceBase>(GetGameInstance()))
	{
		TArray<AActor*> Actors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), Actors);
		if (Actors.Num() > 0)
		{
			AActor* SelectedActor = Actors[0];
			for (AActor* Actor : Actors)
			{
				if (APlayerStart* PlayerStart = Cast<APlayerStart>(Actor))
				{
					if (PlayerStart->PlayerStartTag == GameInstance->PlayerStartTag)
					{
						SelectedActor = PlayerStart;
						break;
					}
				}
			}
			return SelectedActor;
		}
	}
	return nullptr;
}

void ADaGameModeBase::TravelToMap(const FString& MapName)
{
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Maps.FindChecked(MapName));
}

void ADaGameModeBase::TravelToMapWithOptions(const FString& MapName, bool bResetOptions, const FString& Options)
{
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Maps.FindChecked(MapName), bResetOptions, Options);
}

void ADaGameModeBase::OnActorKilled(AActor* VictimActor, AActor* KillerActor)
{
	//LogOnScreen(this, FString::Printf(TEXT("ADaGameModeBase::OnActorKilled VictimActor: (%s) KillerActor: (%s)"), *GetNameSafe(VictimActor), *GetNameSafe(KillerActor)));
	//LOG("ADaGameModeBase::OnActorKilled VictimActor: (%s) KillerActor: (%s)", *GetNameSafe(VictimActor), *GetNameSafe(KillerActor));

	// player dies -> timer elapsed, respawn player
	ADaCharacter* Player = Cast<ADaCharacter>(VictimActor);
	if (Player)
	{
		if (bAutoRespawnPlayer)
		{
			FTimerHandle TimerHandle_RespawnDelay;
			FTimerDelegate Delegate;
			Delegate.BindUObject(this, &ThisClass::RespawnPlayerElapsed, Player->GetController());
			GetWorldTimerManager().SetTimer(TimerHandle_RespawnDelay, Delegate, RespawnDelay, false);
		}
		
		// Store time if it was better than previous record
		ADaPlayerState* PS = Player->GetPlayerState<ADaPlayerState>();
		if (PS)
		{
			PS->UpdatePersonalRecord(GetWorld()->TimeSeconds);
		}
		
		// AutoSave on Player Death
		WriteSaveGame();
	}
}

void ADaGameModeBase::RespawnPlayerElapsed(AController* Controller)
{
	if (ensure(Controller))
	{
		//detach
		Controller->UnPossess();

		//respawn
		RestartPlayer(Controller);
	}
}

void ADaGameModeBase::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	SaveGameSubsystem->HandleStartingNewPlayer(NewPlayer);

	SaveGameSubsystem->DebugLogCurrentSaveGameInfo(FString(TEXT(">>> ADaGameModeBase::HandleStartingNewPlayer_Implementation")));

	// // Will call BeginPlaying State in Player Controller so make sure our data is setup before this calls super
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	
	// Override spawn location
	if (bRespawnAtLastSaveLocation)
	{
		SaveGameSubsystem->OverrideSpawnTransform(NewPlayer);
	}
}

UDaSaveGame* ADaGameModeBase::RetrieveInGameSaveData()
{
	SaveGameSubsystem->DebugLogCurrentSaveGameInfo(FString(TEXT(">>> ADaGameModeBase::RetrieveInGameSaveData")));
	return SaveGameSubsystem->RetrieveInGameSaveData();
}

void ADaGameModeBase::SaveInGameProgressData(TFunction<void(UDaSaveGame*)> SaveDataCallback)
{
	SaveGameSubsystem->DebugLogCurrentSaveGameInfo(FString(TEXT(">>> ADaGameModeBase::SaveInGameProgressData")));
	SaveGameSubsystem->SaveInGameProgressData(SaveDataCallback);
}

void ADaGameModeBase::WriteSaveGame()
{
	SaveGameSubsystem->DebugLogCurrentSaveGameInfo(FString(TEXT(">>> ADaGameModeBase::WriteSaveGame")));
	SaveInGameProgressData([this](UDaSaveGame* SaveData)
	{
		SaveData->bFirstTimeLoadIn = false;
	});
}