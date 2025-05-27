// Copyright Dream Awake Solutions LLC


#include "DaCharacterBase.h"

#include "AbilitySystemComponent.h"
#include "CoreGameplayTags.h"
#include "DaAttributeComponent.h"
#include "DaGameModeBase.h"
#include "DaSaveGame.h"
#include "GameplayFramework.h"
#include "AbilitySystem/DaAbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UI/DaDamageWidget.h"
#include "UI/DaWorldUserWidget.h"
#include "Util/ColorConstants.h"


// Sets default values
ADaCharacterBase::ADaCharacterBase()
{
	CharacterIDGameplayTag = CoreGameplayTags::TAG_Character_ID;
	CharacterTypeGameplayTag = CoreGameplayTags::TAG_Character_Type;

	AttributeComponent = CreateDefaultSubobject<UDaAttributeComponent>("AttributeComponent");

	GetMesh()->SetGenerateOverlapEvents(true);
	
	bUseDefaultHitFlash = true;
	HitFlashTimeParamName = "HitFlashTime";
	HitFlashColorParamName = "HitFlashColor";

	ProjectileSocketName = "ProjectileSocket";
}

UAbilitySystemComponent* ADaCharacterBase::GetAbilitySystemComponent() const
{
	return Cast<UAbilitySystemComponent>(AbilitySystemComponent);
}

void ADaCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (CharacterTypeGameplayTag.MatchesAny(FGameplayTagContainer(CoreGameplayTags::TAG_Character_Type_AI)))
	{
		InitAbilitySystem();
	}
}

void ADaCharacterBase::OnHealthChanged(UDaAttributeComponent* HealthComponent, float OldHealth, float NewHealth,
	AActor* InstigatorActor)
{
	//LogOnScreen(this, FString::Printf(TEXT("ADCharacterBase::ONHealthChanged: %f, Owner:%s Instigator: %s"), NewHealth, *GetNameSafe(this), *GetNameSafe(InstigatorActor)));
	float Delta = NewHealth - OldHealth;
	if (Delta != 0.0f)
	{

		if (UDaAttributeComponent::IsActorAlive(this))
		{
			ShowSetHealthBarWidget();
			ShowDamagePopupWidget(Delta);
		}
		
		if (bUseDefaultHitFlash)
		{
			// If using our Material function, set delta time to trigger the hit flash, then Green if healing or red if damaged
			GetMesh()->SetScalarParameterValueOnMaterials(HitFlashTimeParamName, GetWorld()->TimeSeconds);
			GetMesh()->SetVectorParameterValueOnMaterials(HitFlashColorParamName, Delta > 0 ? FVector(UE::Geometry::LinearColors::Green3f()) : FVector(UE::Geometry::LinearColors::Red3f()));
		}
	}
}

void ADaCharacterBase::OnDeathStarted(AActor* OwningActor, AActor* InstigatorActor)
{
	// Ragdoll
	// could also use anim bp, but will code it here.
	GetMesh()->SetAllBodiesSimulatePhysics(true);
	GetMesh()->SetCollisionProfileName("Ragdoll");

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement();
}

void ADaCharacterBase::OnDeathFinished(AActor* OwningActor)
{
	SetLifeSpan(3.0f);
}

FVector ADaCharacterBase::GetProjectileSocketLocation_Implementation()
{
	return GetMesh()->GetSocketLocation(ProjectileSocketName);
}

UAnimMontage* ADaCharacterBase::GetAttackMontage_Implementation()
{
	return AttackMontage;
}

void ADaCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (CharacterTypeGameplayTag.MatchesAny(FGameplayTagContainer(CoreGameplayTags::TAG_Character_Type_Player)))
	{
		//server
		InitAbilitySystem();
	}
}

void ADaCharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (CharacterTypeGameplayTag.MatchesAny(FGameplayTagContainer(CoreGameplayTags::TAG_Character_Type_Player)))
	{
		//client
		InitAbilitySystem();
	}
}

void ADaCharacterBase::InitAbilitySystem()
{
	// setup must be implemented by sub classes
	
	AttributeComponent->OnHealthChanged.AddDynamic(this, &ThisClass::OnHealthChanged);
	AttributeComponent->OnDeathStarted.AddDynamic(this, &ThisClass::OnDeathStarted);
	AttributeComponent->OnDeathFinished.AddDynamic(this, &ThisClass::OnDeathFinished);
}

void ADaCharacterBase::UpgradeAttribute(const FGameplayTag& AttributeTag, int32 Amount)
{
	AbilitySystemComponent->UpgradeAttribute(AttributeTag, Amount);
}

void ADaCharacterBase::ApplyEffectToSelf(const TSubclassOf<UGameplayEffect>& GameplayEffectClass, const float Level) const
{
	check(IsValid(GetAbilitySystemComponent()));
	if (GameplayEffectClass)
	{
		FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponent()->MakeEffectContext();
		ContextHandle.AddSourceObject(this);
		const FGameplayEffectSpecHandle SpecHandle = GetAbilitySystemComponent()->MakeOutgoingSpec(GameplayEffectClass, Level, ContextHandle);
		GetAbilitySystemComponent()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), GetAbilitySystemComponent());
	}
}

void ADaCharacterBase::InitDefaultAttributes() const
{
	if (!DefaultVitalAttributes)
	{
		LOG_WARNING("No DefaultVitalAttributes GameplayEffect set on Character: %s", *GetNameSafe(this));	
	}

	if (DefaultPrimaryAttributes)
		ApplyEffectToSelf(DefaultPrimaryAttributes, 1.f);

	if (DefaultSecondaryAttributes)
		ApplyEffectToSelf(DefaultSecondaryAttributes, 1.f);

	if (DefaultVitalAttributes)
		ApplyEffectToSelf(DefaultVitalAttributes, 1.f);
}

void ADaCharacterBase::ShowSetHealthBarWidget()
{
	if (HealthBarWidgetClass && ActiveHealthBar == nullptr)
	{
		ActiveHealthBar = CreateWidget<UDaWorldUserWidget>(GetWorld(), HealthBarWidgetClass);
		if (ActiveHealthBar)
		{
			ActiveHealthBar->AttachedActor = this;
			ActiveHealthBar->AddToViewport();
		}
	}
}

void ADaCharacterBase::ShowDamagePopupWidget(float Damage)
{
	if (DamagePopUpWidgetClass)
	{
		UDaDamageWidget* DamagePopUpWidget = CreateWidget<UDaDamageWidget>(GetWorld(), DamagePopUpWidgetClass);
		//LOG("Damage popup widget: %s", *GetNameSafe(DamagePopUpWidget));
		if (DamagePopUpWidget)
		{
			DamagePopUpWidget->AttachedActor = this;
			DamagePopUpWidget->Damage = Damage;
			DamagePopUpWidget->AddToViewport();
		}
	}
}


