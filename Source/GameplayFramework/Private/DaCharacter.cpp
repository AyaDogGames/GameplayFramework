// Copyright Dream Awake Solutions LLC


#include "DaCharacter.h"

#include "Components/CapsuleComponent.h"
#include "AbilitySystem/DaAbilitySystemComponent.h"
#include "DaPlayerState.h"
#include "CoreGameplayTags.h"
#include "DaGameInstanceBase.h"
#include "DaGameModeBase.h"
#include "DaInspectableComponent.h"
#include "DaInspectableItem.h"
#include "DaInteractionComponent.h"
#include "DaPlayerController.h"
#include "DaSaveGame.h"
#include "DaSaveGameSubsystem.h"
#include "GameplayFramework.h"
#include "Kismet/GameplayStatics.h"
#include "UI/DaHUD.h"

// Sets default values
ADaCharacter::ADaCharacter()
{
	CharacterTypeGameplayTag = CoreGameplayTags::TAG_Character_Type_Player;

	InteractionComp = CreateDefaultSubobject<UDaInteractionComponent>(TEXT("InteractionComp"));

	PrimaryActorTick.bCanEverTick = false;
	
	// Collision settings. We are only interested in handling specific overlap events on the character mesh and not the capsule and not after any animations.
	GetMesh()->bUpdateOverlapsOnAnimationFinalize = false;
	//GetMesh()->SetGenerateOverlapEvents(true); <-- set in superclass
	GetCapsuleComponent()->SetGenerateOverlapEvents(false);
}

void ADaCharacter::InitAbilitySystem()
{
	ADaPlayerState* PS = GetPlayerState<ADaPlayerState>();
	if (PS)
	{
		PS->GetAbilitySystemComponent()->InitAbilityActorInfo(PS, this);
		Cast<UDaAbilitySystemComponent>(PS->GetAbilitySystemComponent())->AbilityActorInfoSet();
		AbilitySystemComponent = PS->GetDaAbilitySystemComponent();

		// Load AbilitySet and any other pawn data for this character
		// Will setup the ability system component with Abilities, Effects, and needed Attributes
		PS->InitializePlayerPawnData();

		// Attribute Component set in PlayerState after loading pawn data
		Super::InitAbilitySystem();

	}
}

void ADaCharacter::SaveProgress_Implementation(const FName& CheckpointTag)
{
	if (ADaGameModeBase* GameMode = Cast<ADaGameModeBase>(UGameplayStatics::GetGameMode(this)))
	{
		GameMode->SaveInGameProgressData([this, CheckpointTag](UDaSaveGame* SaveData)
		{
			SaveData->PlayerStartTag = CheckpointTag;

			// Save the player start tag in the game instance so that it can be used to load the player start on the client
			UDaGameInstanceBase* GameInstance = Cast<UDaGameInstanceBase>(GetGameInstance());
			GameInstance->PlayerStartTag = CheckpointTag;
		});
	}

	Super::SaveProgress_Implementation(CheckpointTag);
}

void ADaCharacter::PrimaryInteraction() const
{
	if (InteractionComp) InteractionComp->PrimaryInteract();
}

void ADaCharacter::InitPlayerHUD() const
{
	ADaPlayerController* PlayerController = Cast<ADaPlayerController>(GetController());
	if (PlayerController && IsLocallyControlled())
	{
		if (ADaHUD* HUD = Cast<ADaHUD>(PlayerController->GetHUD()))
		{
			HUD->RemoveOverlay();
			HUD->InitRootLayout(PlayerController);
		}
	}
}

int32 ADaCharacter::GetCharacterLevel()
{
	// Fallback to Level 1 as default  if player state isn't set
	int32 PlayerLevel = 1;
	if (ADaPlayerState* PS = GetPlayerState<ADaPlayerState>())
	{
		PlayerLevel = PS->GetPlayerLevel();
	}
	return PlayerLevel;
}

void ADaCharacter::OnDeathStarted(AActor* OwningActor, AActor* InstigatorActor)
{
	// Death
	if(APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}

	// Call super to ragdoll
	Super::OnDeathStarted(OwningActor, InstigatorActor);
}

void ADaCharacter::SetInspectedItem(AActor* Item)
{
	if (InspectedItem != nullptr)
	{
		LOG_WARNING("Inspected Item is being set but is already set previously.")

		if (UDaInspectableComponent* PrevInspectComponent = InspectedItem->FindComponentByClass<UDaInspectableComponent>())
		{
			PrevInspectComponent->OnInspectStateChanged.RemoveDynamic(this, &ThisClass::InspectedItemStateChanged);
		}
		InspectedItem = nullptr;

	}

	// Only set new item if it implements the interface
	if (Item && Item->GetClass()->ImplementsInterface(UDaInspectableInterface::StaticClass()))
	{
		InspectedItem = Item;
		if (UDaInspectableComponent* InspectComponent = InspectedItem->FindComponentByClass<UDaInspectableComponent>())
		{
			InspectComponent->OnInspectStateChanged.AddDynamic(this, &ThisClass::InspectedItemStateChanged);
		}
	}

}

void ADaCharacter::SetMovingItem(AActor* Item)
{
	MovingItem = Item;
}

void ADaCharacter::InspectedItemStateChanged(AActor* Item, AActor* InspectingActor, bool IsInspecting)
{
	if (!IsInspecting && InspectedItem == Item)
	{
		if (UDaInspectableComponent* InspectComponent = InspectedItem->FindComponentByClass<UDaInspectableComponent>())
		{
			InspectComponent->OnInspectStateChanged.RemoveDynamic(this, &ThisClass::InspectedItemStateChanged);
		}
		InspectedItem = nullptr;
	}
}



