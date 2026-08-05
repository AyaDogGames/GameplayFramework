// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DaItemDebugOverlay.generated.h"

/**
 * UDaItemDebugLibrary
 *
 * Console-driven debug overlay for the item / condition system, plus the tiny query surface
 * automated smokes and Blueprints use to drive it.
 *
 * The overlay itself is a pure Slate widget with no UMG asset behind it (it is built entirely in
 * Private/UI/DaItemDebugOverlay.cpp) so it works in any project that consumes this plugin without
 * any content to author or a theme to match. It is added straight to the game viewport of the
 * world it was toggled in, which is what makes it usable in a multi-window PIE session: each
 * window toggles its own overlay and acts on its own local player.
 *
 * Everything the overlay's buttons do goes through the normal BlueprintCallable component APIs
 * (UDaInventoryComponent / UDaEquipmentManagerComponent / ADaPlayerState), so a client window
 * exercises the same client->server RPC path a real UI would. It adds no RPC surface of its own.
 *
 * The whole overlay compiles out of shipping builds (UE_BUILD_SHIPPING); these functions stay,
 * and become no-ops that report "not visible".
 */
UCLASS()
class GAMEPLAYFRAMEWORK_API UDaItemDebugLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Show or hide the overlay for the first local player of WorldContextObject's world —
	 * the same thing the `Da.Debug.Items` console command does. Returns whether the overlay
	 * is visible AFTERWARDS (always false in a shipping build).
	 */
	UFUNCTION(BlueprintCallable, Category="Debug|Items", meta=(WorldContext="WorldContextObject"))
	static bool ToggleItemDebugOverlay(const UObject* WorldContextObject);

	/** Is the debug overlay currently on screen? */
	UFUNCTION(BlueprintPure, Category="Debug|Items")
	static bool IsItemDebugOverlayVisible();

	/**
	 * How many item rows the overlay drew the last time it refreshed, or 0 when it is hidden.
	 * Exists so an automated smoke can assert the overlay really found the local player's
	 * inventory rather than merely appearing.
	 */
	UFUNCTION(BlueprintPure, Category="Debug|Items")
	static int32 GetItemDebugOverlayRowCount();
};
