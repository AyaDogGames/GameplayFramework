// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DaProjectile.generated.h"

class UNiagaraComponent;
class UParticleSystem;
class UProjectileMovementComponent;
class USphereComponent;
class UAudioComponent;
class USoundCue;

UCLASS()
class GAMEPLAYFRAMEWORK_API ADaProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ADaProjectile();

protected:

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Components")
	TObjectPtr<USphereComponent> SphereComp;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Components")
	TObjectPtr<UProjectileMovementComponent> MovementComp;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Components")
	TObjectPtr<UNiagaraComponent> EffectComp;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UAudioComponent> AudioComp;
	
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	TObjectPtr<USoundCue> HitSound;

	UPROPERTY(EditDefaultsOnly, Category="Effects")
	TObjectPtr<UParticleSystem> HitEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Effects|Shake")
	TSubclassOf<UCameraShakeBase> ImpactShake;

	UPROPERTY(EditDefaultsOnly, Category = "Effects|Shake")
	float ImpactShakeInnerRadius;

	UPROPERTY(EditDefaultsOnly, Category = "Effects|Shake")
	float ImpactShakeOuterRadius;
	
	UFUNCTION()
	virtual void OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void Explode();
	
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void PostInitializeComponents() override;
	
};
