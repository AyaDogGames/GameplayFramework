// Copyright Dream Awake Solutions LLC


#include "AbilitySystem/Abilities/DaGameplayAbility_ToggleUILayer.h"

#include "CommonActivatableWidget.h"
#include "CoreGameplayTags.h"
#include "GameFramework/PlayerController.h"
#include "GameplayFramework.h"
#include "UI/DaCommonUIExtensions.h"

UDaGameplayAbility_ToggleUILayer::UDaGameplayAbility_ToggleUILayer()
{
	// InstancedPerActor: the pushed widget is per-player state that has to survive between the
	// press that opens the panel and the press that closes it.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	LayerTag = CoreGameplayTags::TAG_UI_Layer_GameMenu;
}

void UDaGameplayAbility_ToggleUILayer::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	APlayerController* PC = ActorInfo ? ActorInfo->PlayerController.Get() : nullptr;
	if (!PC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// Second press: close.
	if (PushedWidget)
	{
		PopContent();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (WidgetClass.IsNull() || !LayerTag.IsValid())
	{
		LOG_WARNING("%s: no WidgetClass or LayerTag set, nothing to toggle", *GetName());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// Synchronous: this is a keypress opening a menu, and the alternative is a frame where the
	// button did nothing visible.
	UClass* LoadedClass = WidgetClass.LoadSynchronous();
	if (!LoadedClass)
	{
		LOG_WARNING("%s: could not load widget class %s", *GetName(), *WidgetClass.ToString());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	PushedWidget = UDaCommonUIExtensions::PushContentToLayer_ForPlayer(PC, LayerTag, LoadedClass);
	if (!PushedWidget)
	{
		LOG_WARNING("%s: nothing was pushed onto %s — is the primary game layout loaded and does it "
			"register that layer?", *GetName(), *LayerTag.ToString());
	}
	else if (bShowMouseCursorWhileOpen)
	{
		bRestoreMouseCursor = PC->bShowMouseCursor;
		PC->bShowMouseCursor = true;
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UDaGameplayAbility_ToggleUILayer::PopContent()
{
	if (!PushedWidget)
	{
		return;
	}

	UDaCommonUIExtensions::PopContentFromLayer(PushedWidget);
	PushedWidget = nullptr;

	if (bShowMouseCursorWhileOpen)
	{
		if (const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo())
		{
			if (APlayerController* PC = Info->PlayerController.Get())
			{
				PC->bShowMouseCursor = bRestoreMouseCursor;
			}
		}
	}
}
