// Copyright Dream Awake Solutions LLC

#include "DaGameModeBase.h"
#include "DaCharacter.h"
#include "DaPlayerState.h"
#include "DaSaveGameSubsystem.h"
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

	if (UGameInstanceSubsystem* GIS = GetGameInstance()->GetSubsystemBase(SaveGameSubsystemClass))
	{
		SaveGameSubsystem = Cast<UDaSaveGameSubsystem>(GIS);
	}

	if (SaveGameSubsystem)
	{
		FString SelectedSaveSlot = UGameplayStatics::ParseOption(Options, "SaveGame");
		SaveGameSubsystem->LoadSaveGame(SelectedSaveSlot);
	} else
	{
		ErrorMessage = FString::Printf(TEXT("SaveGame Subsystem not found in GameInstance: %s"), *GetNameSafe(GetGameInstance()));
	}
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
		//UDaSaveGameSubsystem* SG = GetGameInstance()->GetSubsystem<UDaSaveGameSubsystem>();
		SaveGameSubsystem->WriteSaveGame();
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
	//UDaSaveGameSubsystem* SG = GetGameInstance()->GetSubsystem<UDaSaveGameSubsystem>();
	SaveGameSubsystem->HandleStartingNewPlayer(NewPlayer);
	
	// // Will call BeginPlaying State in Player Controller so make sure our data is setup before this calls super
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	
	// Override spawn location
	if (bRespawnAtLastSaveLocation)
	{
		SaveGameSubsystem->OverrideSpawnTransform(NewPlayer);
	}
}

