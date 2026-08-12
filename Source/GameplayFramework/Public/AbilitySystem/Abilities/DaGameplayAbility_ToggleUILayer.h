// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DaGameplayAbilityBase.h"
#include "GameplayTagContainer.h"
#include "DaGameplayAbility_ToggleUILayer.generated.h"

class UCommonActivatableWidget;

/**
 * UDaGameplayAbility_ToggleUILayer
 *
 * Push a widget onto a CommonUI layer, or pop it if this ability already pushed it. One button,
 * one screen, open and closed.
 *
 * Why an ability and not an input binding on the player controller: every gameplay key in this
 * framework already arrives as an Input.* gameplay tag on an ability spec
 * (ADaPlayerController::AbilityInputTagHeld -> UDaAbilitySystemComponent -> TryActivateAbility), so
 * a UI toggle authored this way needs no new input plumbing at all — an InputAction, an IMC key, an
 * input-config pair and an ability-set grant, exactly like every other button.
 *
 * NetExecutionPolicy is LocalOnly and deliberately so: pushing a widget is a client-side act, and a
 * ServerInitiated toggle would round-trip a keypress to open a menu (and open nothing at all for a
 * listen-server client, whose UI lives in its own process). Nothing here replicates.
 */
UCLASS()
class GAMEPLAYFRAMEWORK_API UDaGameplayAbility_ToggleUILayer : public UDaGameplayAbilityBase
{
	GENERATED_BODY()

public:

	UDaGameplayAbility_ToggleUILayer();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** True while this ability is holding a pushed widget. */
	UFUNCTION(BlueprintPure, Category="UI")
	bool IsContentPushed() const { return PushedWidget != nullptr; }

protected:

	/** Widget class to push. Soft so the ability (which is granted at startup) does not drag a
	 *  whole UI tree into memory before anyone opens it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
	TSoftClassPtr<UCommonActivatableWidget> WidgetClass;

	/** Which UI.Layer.* stack to push onto. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI", meta=(Categories="UI.Layer"))
	FGameplayTag LayerTag;

	/** Show the mouse cursor while the widget is up, and restore the previous setting on close.
	 *  A panel with buttons is unusable without it, and ADaPlayerController hides the cursor for
	 *  GameOnly play. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
	bool bShowMouseCursorWhileOpen = true;

private:

	/** Pop whatever we pushed, restoring the cursor. Safe to call when nothing is pushed. */
	void PopContent();

	UPROPERTY(Transient)
	TObjectPtr<UCommonActivatableWidget> PushedWidget;

	/** bShowMouseCursor as it was before we pushed, so closing puts it back. */
	bool bRestoreMouseCursor = false;
};
