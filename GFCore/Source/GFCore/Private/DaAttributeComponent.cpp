// Copyright Dream Awake Solutions LLC


#include "DaAttributeComponent.h"

#include "AbilitySystemInterface.h"
#include "AbilitySystem/DaAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/DaCharacterAttributeSet.h"
#include "DaGameModeBase.h"
#include "CoreGameplayTags.h"
#include "GameplayFramework.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"


// Sets default values for this component's properties
UDaAttributeComponent::UDaAttributeComponent()
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);

	AbilitySystemComponent = nullptr;
	AttributeSet = nullptr;
	DeathState = EDaDeathState::NotDead;
}

void UDaAttributeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDaAttributeComponent, DeathState);
}

void UDaAttributeComponent::OnUnregister()
{
	UninitializeFromAbilitySystem();

	Super::OnUnregister();
}

void UDaAttributeComponent::InitializeWithAbilitySystem(UDaAbilitySystemComponent* InASC)
{
	AActor* Owner = GetOwner();
	check(Owner);

	if (AbilitySystemComponent)
	{
		LOG_ERROR("DaAttributeComponent: Health component for owner [%s] has already been initialized with an ability system.", *GetNameSafe(Owner));
		return;
	}

	AbilitySystemComponent = InASC;
	if (!AbilitySystemComponent)
	{
		LOG_ERROR("DaAttributeComponent: Cannot initialize health component for owner [%s] with NULL ability system.", *GetNameSafe(Owner));
		return;
	}

	AttributeSet = AbilitySystemComponent->GetSet<UDaCharacterAttributeSet>();
	if (!AttributeSet)
	{
		LOG_ERROR("DaAttributeComponent:  Cannot initialize health component for owner [%s] with NULL health set on the ability system.", *GetNameSafe(Owner));
		return;
	}

	//LogOnScreen(this, FString::Printf(TEXT("DaAttributeComponent: Initialized AttributeSet: %s"), *GetNameSafe(AttributeSet)));
	
	// Register to listen for attribute changes.
	AttributeSet->OnHealthChanged.AddUObject(this, &UDaAttributeComponent::HandleHealthChanged);
	AttributeSet->OnMaxHealthChanged.AddUObject(this, &UDaAttributeComponent::HandleMaxHealthChanged);
	AttributeSet->OnOutOfHealth.AddUObject(this, &UDaAttributeComponent::HandleOutOfHealth);
	
	AttributeSet->OnManaChanged.AddUObject(this, &UDaAttributeComponent::HandleManaChanged);
	AttributeSet->OnMaxHealthChanged.AddUObject(this, &UDaAttributeComponent::HandleMaxManaChanged);

	// TEMP: Reset attributes to default values.  Eventually this will be driven by a spread sheet.
	//AbilitySystemComponent->SetNumericAttributeBase(UDaCharacterAttributeSet::GetHealthAttribute(), AttributeSet->GetMaxHealth());

	ClearGameplayTags();

	OnHealthChanged.Broadcast(this, AttributeSet->GetHealth(), AttributeSet->GetHealth(), nullptr);
	OnMaxHealthChanged.Broadcast(this, AttributeSet->GetMaxHealth(), AttributeSet->GetMaxHealth(), nullptr);

	OnManaChanged.Broadcast(this, AttributeSet->GetMana(), AttributeSet->GetMana(), nullptr);
	OnMaxManaChanged.Broadcast(this, AttributeSet->GetMaxMana(), AttributeSet->GetMaxMana(), nullptr);

	//MulticastOnHealthChanged(GetOwner(), nullptr, AttributeSet->GetHealth(), AttributeSet->GetHealth());
}

void UDaAttributeComponent::UninitializeFromAbilitySystem()
{
	ClearGameplayTags();

	if (AttributeSet)
	{
		AttributeSet->OnHealthChanged.RemoveAll(this);
		AttributeSet->OnMaxHealthChanged.RemoveAll(this);
		AttributeSet->OnOutOfHealth.RemoveAll(this);
		AttributeSet->OnManaChanged.RemoveAll(this);
		AttributeSet->OnMaxManaChanged.RemoveAll(this);
	}

	AttributeSet = nullptr;
	AbilitySystemComponent = nullptr;
}


void UDaAttributeComponent::ClearGameplayTags()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(CoreGameplayTags::TAG_Status_Death_Dying, 0);
		AbilitySystemComponent->SetLooseGameplayTagCount(CoreGameplayTags::TAG_Status_Death_Dead, 0);
	}
}

float UDaAttributeComponent::GetHealth() const
{
	return (AttributeSet ? AttributeSet->GetHealth() : 0.0f);
}

float UDaAttributeComponent::GetMaxHealth() const
{
	return (AttributeSet ? AttributeSet->GetMaxHealth() : 0.0f);
}

bool UDaAttributeComponent::IsAtMaxHealth() const
{
	return GetHealth() >= GetMaxHealth();
}

float UDaAttributeComponent::GetHealthNormalized() const
{
	if (AttributeSet)
	{
		const float Health = AttributeSet->GetHealth();
		const float MaxHealth = AttributeSet->GetMaxHealth();

		return ((MaxHealth > 0.0f) ? (Health / MaxHealth) : 0.0f);
	}

	return 0.0f;
}

float UDaAttributeComponent::GetMana() const
{
	return (AttributeSet ? AttributeSet->GetMana() : 0.0f);
}

float UDaAttributeComponent::GetMaxMana() const
{
	return (AttributeSet ? AttributeSet->GetMaxMana() : 0.0f);
}

void UDaAttributeComponent::HandleHealthChanged(AActor* DamageInstigator, AActor* DamageCauser,
                                                const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue)
{
	//MulticastOnHealthChanged(GetOwner(), DamageInstigator, OldValue, NewValue);
	OnHealthChanged.Broadcast(this, OldValue, NewValue, DamageInstigator);

}

void UDaAttributeComponent::MulticastOnHealthChanged_Implementation(AActor* Owner, AActor* DamageInstigator,
	float OldHealth, float NewHealth)
{
	//LogOnScreen(this, FString::Printf(TEXT("DaAttributeComponent: HANDLE HEALTH CHANGE %s %s "), *GetNameSafe(Owner), *GetNameSafe(DamageInstigator)));
	OnHealthChanged.Broadcast(this, OldHealth, NewHealth, DamageInstigator);
}


void UDaAttributeComponent::HandleMaxHealthChanged(AActor* DamageInstigator, AActor* DamageCauser,
	const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue)
{
	OnMaxHealthChanged.Broadcast(this, OldValue, NewValue, DamageInstigator);
}

void UDaAttributeComponent::HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser,
	const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue)
{
#if WITH_SERVER_CODE
	if (AbilitySystemComponent && DamageEffectSpec)
	{
		// Send the "GameplayEvent.Death" gameplay event through the owner's ability system.  This can be used to trigger a death gameplay ability.
		{
			FGameplayEventData Payload;
			Payload.EventTag = CoreGameplayTags::TAG_Event_Death;
			Payload.Instigator = Cast<IAbilitySystemInterface>(DamageInstigator)->GetAbilitySystemComponent()->GetAvatarActor();
			Payload.Target = AbilitySystemComponent->GetAvatarActor();
			Payload.OptionalObject = DamageEffectSpec->Def;
			Payload.ContextHandle = DamageEffectSpec->GetEffectContext();
			Payload.InstigatorTags = *DamageEffectSpec->CapturedSourceTags.GetAggregatedTags();
			Payload.TargetTags = *DamageEffectSpec->CapturedTargetTags.GetAggregatedTags();
			Payload.EventMagnitude = DamageMagnitude;

			//FScopedPredictionWindow NewScopedWindow(AbilitySystemComponent, true);
			AbilitySystemComponent->HandleGameplayEvent(Payload.EventTag, &Payload);

			//LogOnScreen(this, FString::Printf(TEXT("UDaAttributeComponent::HandleOutOfHealth Death Gameplay.Event DeathState: %hhd, Owner: (%s)"), DeathState, *GetNameSafe(GetOwner())));
			//LOG("UDaAttributeComponent::HandleOutOfHealth Death Gameplay.Event DeathState: %hhd, Owner: (%s)", DeathState, *GetNameSafe(GetOwner()));

		}

		//@TODO: Send a standardized verb message that other systems can observe
		// {
		// 	FLyraVerbMessage Message;
		// 	Message.Verb = TAG_Lyra_Elimination_Message;
		// 	Message.Instigator = DamageInstigator;
		// 	Message.InstigatorTags = *DamageEffectSpec->CapturedSourceTags.GetAggregatedTags();
		// 	Message.Target = ULyraVerbMessageHelpers::GetPlayerStateFromObject(AbilitySystemComponent->GetAvatarActor());
		// 	Message.TargetTags = *DamageEffectSpec->CapturedTargetTags.GetAggregatedTags();
		// 	//@TODO: Fill out context tags, and any non-ability-system source/instigator tags
		// 	//@TODO: Determine if it's an opposing team kill, self-own, team kill, etc...
		//
		// 	UGameplayMessageSubsystem& MessageSystem = UGameplayMessageSubsystem::Get(GetWorld());
		// 	MessageSystem.BroadcastMessage(Message.Verb, Message);
		// }
		
		//@TODO: assist messages (could compute from damage dealt elsewhere)?
	}

#endif // #if WITH_SERVER_CODE
}

void UDaAttributeComponent::HandleManaChanged(AActor* DamageInstigator, AActor* DamageCauser,
	const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue)
{
	OnManaChanged.Broadcast(this, OldValue, NewValue, DamageInstigator);
}

void UDaAttributeComponent::HandleMaxManaChanged(AActor* DamageInstigator, AActor* DamageCauser,
	const FGameplayEffectSpec* DamageEffectSpec, float DamageMagnitude, float OldValue, float NewValue)
{
	OnMaxManaChanged.Broadcast(this, OldValue, NewValue, DamageInstigator);
}

void UDaAttributeComponent::OnRep_DeathState(EDaDeathState OldDeathState)
{
	const EDaDeathState NewDeathState = DeathState;
	
	// Revert the death state for now since we rely on StartDeath and FinishDeath to change it.
	DeathState = OldDeathState;

	//LogOnScreen(this, FString::Printf(TEXT("UDaAttributeComponent::OnRep_DeathState: %hhd, %hhd, Owner: (%s)"), OldDeathState, NewDeathState, *GetNameSafe(GetOwner())));

	if (OldDeathState > NewDeathState)
	{
		// The server is trying to set us back but we've already predicted past the server state.
		LOG_WARNING("UDaAttributeComponent: Predicted past server death state [%d] -> [%d] for owner [%s].", (uint8)OldDeathState, (uint8)NewDeathState, *GetNameSafe(GetOwner()));
		return;
	}

	if (OldDeathState == EDaDeathState::NotDead)
	{
		if (NewDeathState == EDaDeathState::DeathStarted)
		{
			StartDeath(nullptr);
		}
		else if (NewDeathState == EDaDeathState::DeathFinished)
		{
			StartDeath(nullptr);
			FinishDeath();
		}
		else
		{
			LOG_ERROR("UDaAttributeComponent: Invalid death transition [%d] -> [%d] for owner [%s].", (uint8)OldDeathState, (uint8)NewDeathState, *GetNameSafe(GetOwner()));
		}
	}
	else if (OldDeathState == EDaDeathState::DeathStarted)
	{
		if (NewDeathState == EDaDeathState::DeathFinished)
		{
			FinishDeath();
		}
		else
		{
			LOG_ERROR("UDaAttributeComponent:  Invalid death transition [%d] -> [%d] for owner [%s].", (uint8)OldDeathState, (uint8)NewDeathState, *GetNameSafe(GetOwner()));
		}
	}

	ensureMsgf((DeathState == NewDeathState), TEXT("UDaAttributeComponent: Death transition failed [%d] -> [%d] for owner [%s]."), (uint8)OldDeathState, (uint8)NewDeathState, *GetNameSafe(GetOwner()));
}

bool UDaAttributeComponent::IsActorAlive(AActor* Actor)
{
	UDaAttributeComponent* AttributeComp = FindAttributeComponent(Actor);
	if (AttributeComp)
	{
		return !AttributeComp->IsDeadOrDying();
	}

	// if it doesnt have something to manage its health its alive by default
	return true;
}

bool UDaAttributeComponent::StartDeath(AActor* InstigatorActor)
{
	if (DeathState != EDaDeathState::NotDead)
	{
		return false;
	}
	
	DeathState = EDaDeathState::DeathStarted;
	
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(CoreGameplayTags::TAG_Status_Death_Dying, 1);
	}

	AActor* Owner = GetOwner();
	check(Owner);

	OnDeathStarted.Broadcast(Owner, InstigatorActor);

	if (Owner->HasAuthority())
	{
		ADaGameModeBase* GameMode = Cast<ADaGameModeBase>(UGameplayStatics::GetGameMode(GetWorld()));
		GameMode->OnActorKilled(Owner, InstigatorActor);
	}
	
	Owner->ForceNetUpdate();

	return true;
}

void UDaAttributeComponent::FinishDeath()
{
	if (DeathState != EDaDeathState::DeathStarted)
	{
		return;
	}
	
	DeathState = EDaDeathState::DeathFinished;

	//LogOnScreen(this, FString::Printf(TEXT("UDaAttributeComponent::FinishDeath called: %hhd, Owner: (%s)"), DeathState, *GetNameSafe(GetOwner())));

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(CoreGameplayTags::TAG_Status_Death_Dead, 1);
	}

	AActor* Owner = GetOwner();
	check(Owner);

	OnDeathFinished.Broadcast(Owner);

	Owner->ForceNetUpdate();
}

void UDaAttributeComponent::DamageSelfDestruct(bool bFellOutOfWorld)
{
	if ((DeathState == EDaDeathState::NotDead) && AbilitySystemComponent)
	{
		LOG_ERROR("UDaAttributeComponent:  DamageSelfDestruct not implemented [%d] for owner [%s].", (uint8)bFellOutOfWorld, *GetNameSafe(GetOwner()));

		// const TSubclassOf<UGameplayEffect> DamageGE = ULyraAssetManager::GetSubclass(ULyraGameData::Get().DamageGameplayEffect_SetByCaller);
		// if (!DamageGE)
		// {
		// 	UE_LOG(LogLyra, Error, TEXT("LyraHealthComponent: DamageSelfDestruct failed for owner [%s]. Unable to find gameplay effect [%s]."), *GetNameSafe(GetOwner()), *ULyraGameData::Get().DamageGameplayEffect_SetByCaller.GetAssetName());
		// 	return;
		// }
		//
		// FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(DamageGE, 1.0f, AbilitySystemComponent->MakeEffectContext());
		// FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
		//
		// if (!Spec)
		// {
		// 	UE_LOG(LogLyra, Error, TEXT("LyraHealthComponent: DamageSelfDestruct failed for owner [%s]. Unable to make outgoing spec for [%s]."), *GetNameSafe(GetOwner()), *GetNameSafe(DamageGE));
		// 	return;
		// }
		//
		// Spec->AddDynamicAssetTag(CoreGameplayTags::TAG_Gameplay_DamageSelfDestruct);
		//
		// if (bFellOutOfWorld)
		// {
		// 	Spec->AddDynamicAssetTag(CoreGameplayTags::TAG_Gameplay_FellOutOfWorld);
		// }
		//
		// const float DamageAmount = GetMaxHealth();
		//
		// Spec->SetSetByCallerMagnitude(SharedGameplayTags::SetByCaller_Damage, DamageAmount);
		// AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec);
	}
}
