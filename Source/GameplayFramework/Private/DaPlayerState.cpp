// Copyright Dream Awake Solutions LLC


#include "DaPlayerState.h"

#include "AbilitySystem/DaAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/DaCharacterAttributeSet.h"
#include "AbilitySystem/Attributes/DaCombatAttributeSet.h"
#include "DaAttributeComponent.h"
#include "DaPawnData.h"
#include "DaPlayerController.h"
#include "DaSaveGame.h"
#include "Engine/AssetManager.h"
#include "CoreGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "DaCharacter.h"
#include "GameplayFramework.h"
#include "Inventory/DaInventoryComponent.h"

ADaPlayerState::ADaPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilitySystemComponent = ObjectInitializer.CreateDefaultSubobject<UDaAbilitySystemComponent>(this, TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	InventoryComp = CreateDefaultSubobject<UDaInventoryComponent>("InventoryComponent");
	
	// These attribute sets will be detected by AbilitySystemComponent::InitializeComponent. Keeping a reference so that the sets don't get garbage collected before that.
	HealthSet = CreateDefaultSubobject<UDaCharacterAttributeSet>(TEXT("HealthSet"));
	CombatSet = CreateDefaultSubobject<UDaCombatAttributeSet>(TEXT("CombatSet"));
	
	// AbilitySystemComponent needs to be updated at a high frequency.
	SetNetUpdateFrequency(100.0f);

	bReplicates = true;

	LoadedPawnData = nullptr;
}

void ADaPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADaPlayerState, Credits);
	DOREPLIFETIME(ADaPlayerState, Level);

}

ADaPlayerController* ADaPlayerState::GetDaPlayerController() const
{
	return Cast<ADaPlayerController>(GetOwner());
}

UAbilitySystemComponent* ADaPlayerState::GetAbilitySystemComponent() const
{
	return GetDaAbilitySystemComponent();
}

void ADaPlayerState::InitializePlayerPawnData()
{
	check(PlayerPawnDataTable);

	if (LoadedPawnData == nullptr && HasAuthority())
	{
		TArray<FPlayerCharacterInfoRow*> Rows;
		PlayerPawnDataTable->GetAllRows("", Rows);
	
		UAssetManager& AssMan = UAssetManager::Get();
		for (FPlayerCharacterInfoRow* SelectedRow: Rows)
		{
			TArray<FName> Bundles; 
			FStreamableDelegate Delegate = FStreamableDelegate::CreateUObject(this, &ADaPlayerState::OnPlayerPawnDataLoaded, SelectedRow->PawnDataInfoId);
			AssMan.LoadPrimaryAsset(SelectedRow->PawnDataInfoId, Bundles, Delegate);
		}
	}
	else
	{
		// Actions are blocked during death (in DaGameplayAbility_Death), unblock on respawn here
		FGameplayTagContainer AbilityTypesToBlock;
		AbilityTypesToBlock.AddTag(CoreGameplayTags::TAG_Action);
		AbilitySystemComponent->UnBlockAbilitiesWithTags(AbilityTypesToBlock);
		
		// Player character respawned and player state ability system component already has set attribute set
		// so we just need to set up the player character's attribute component so UI will see health changes
		InitCharacterAttributes(true);
	}
}

void ADaPlayerState::OnPlayerPawnDataLoaded(FPrimaryAssetId LoadedId)
{
	UAssetManager& AssMan = UAssetManager::Get();

	LoadedPawnData = Cast<UDaPawnData>(AssMan.GetPrimaryAssetObject(LoadedId));
	if (LoadedPawnData)
	{
		AbilitySystemComponent->InitAbilitiesWithPawnData(LoadedPawnData);
		InitCharacterAttributes(true);
	}
}

void ADaPlayerState::InitCharacterAttributes(bool bReset) const
{
	UDaAttributeComponent* AttributeComp = GetPawn()->GetComponentByClass<UDaAttributeComponent>();
	AttributeComp->InitializeWithAbilitySystem(AbilitySystemComponent);
	
	// ASC and AttributeComponent are initialized, setup hud and any primary attributes defaults
	if(ADaCharacter* Character = Cast<ADaCharacter>(GetPawn()))
	{
		Character->InitPlayerHUD();

		// Notify Blueprints they can activate abilites or whatever
		Character->OnCharacterAbilitiesLoaded();
		
		if (bReset)
			Character->LoadProgress();
	}
	
}

void ADaPlayerState::LoadPlayerState_Implementation(UDaSaveGame* SaveObject)
{
	if (SaveObject)
	{
		FPlayerSaveData* FoundData = SaveObject->GetPlayerData(this);
		if (FoundData)
		{
			// Makes sure we trigger credits changed event
			AdjustCredits(FoundData->Credits);
			AdjustLevel(FoundData->Level);
			PersonalRecordTime = FoundData->PersonalRecordTime;
		}
		else
		{
			LOG("Could not find SaveGame data for player id '%i'.", GetPlayerId());
		}

		if (InventoryComp && FoundData)
		{
			InventoryComp->LoadInventory(FoundData->SavedInventory);
		}
	}
}

void ADaPlayerState::SavePlayerState_Implementation(UDaSaveGame* SaveObject)
{
	if (SaveObject)
	{
		FPlayerSaveData SaveData;
		SaveData.Credits = Credits;
		SaveData.Level = Level;
		SaveData.PersonalRecordTime = PersonalRecordTime;

		// Stored as FString for simplicity (original Steam ID is uint64)
		SaveData.PlayerID = GetUniqueId()->ToString();
		//SaveData.PlayerID = GetPlayerId();
		
		// May not be alive while we save
		if (APawn* MyPawn = GetPawn())
		{
			SaveData.Location = MyPawn->GetActorLocation();
			SaveData.Rotation = MyPawn->GetActorRotation();
			SaveData.bResumeAtTransform = true;
		}
		
		if (InventoryComp)
		{
			SaveData.SavedInventory = InventoryComp->SaveInventory();
		}

		SaveObject->SavedPlayers.Add(SaveData);
	}
}

bool ADaPlayerState::UpdatePersonalRecord(float NewTime)
{
	// Higher time is better
	if (NewTime > PersonalRecordTime)
	{
		float OldRecord = PersonalRecordTime;
		PersonalRecordTime = NewTime;
		OnRecordTimeChanged.Broadcast(this, PersonalRecordTime, OldRecord);
		return true;
	}
	return false;
}

int32 ADaPlayerState::GetCredits() const
{
	return Credits;
}

void ADaPlayerState::AdjustCredits(int32 Delta)
{
	if (HasAuthority())
	{
		Credits += Delta;

		// even though using onRepNotify still have to trigger events on server
		OnCreditsChanged.Broadcast(GetInstigator(), Credits, Delta);
	}
}

void ADaPlayerState::OnRep_Credits(int32 OldCredits)
{
	float Delta = Credits-OldCredits;
	
	// LOG("PlayerState: %s gets %f credits", *GetNameSafe(this), Delta);
	// FString Msg = FString::Printf(TEXT("PlayerState: %s gets %f credits"), *GetNameSafe(this), Delta);
	// LogOnScreen(this, Msg, HasAuthority() ? FColor::Green : FColor::Red);
	
	OnCreditsChanged.Broadcast(this, Credits, Delta);
}

void ADaPlayerState::OnRep_Level(int32 OldLevel)
{
	OnLevelChanged.Broadcast(this, Level, OldLevel);
}

void ADaPlayerState::AdjustLevel(int32 NewLevel)
{
	if (HasAuthority())
	{
		// Just override the level here
		int32 OldLevel = Level;
		Level = NewLevel;

		OnLevelChanged.Broadcast(this, Level, OldLevel);
	}
}