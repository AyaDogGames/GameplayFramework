// Copyright Dream Awake Solutions LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DaDungeonLayout.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DaProcGenLibrary.generated.h"

/**
 * Blueprint/Python entry points to the layout generator, so a smoke (or a debug widget) can drive it
 * with no actor and no world.
 */
UCLASS()
class DAPROCGEN_API UDaProcGenLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Generate and return the tiles, with the same bounded re-roll ADaProcGenActor does
	 * (FDaDungeonLayout::GenerateWithReroll), so this predicts the actor for ANY seed — not only one
	 * whose first roll lands. Empty array when every attempt fails. MaxAttempts should match the
	 * actor's MaxGenerateAttempts; both default to 8.
	 */
	UFUNCTION(BlueprintCallable, Category = "ProcGen")
	static TArray<FDaLayoutTile> GenerateLayoutTiles(int32 Seed, const FDaDungeonLayoutParams& Params, int32 MaxAttempts = 8);

	/**
	 * Generate (with the actor's re-roll, see GenerateLayoutTiles) and return
	 * FDaDungeonLayout::HashTiles of the result, widened to int64 so Python and Blueprint compare it
	 * safely (a uint32 does not survive a BP int). 0 when every attempt fails.
	 */
	UFUNCTION(BlueprintCallable, Category = "ProcGen")
	static int64 GetLayoutHash(int32 Seed, const FDaDungeonLayoutParams& Params, int32 MaxAttempts = 8);

	/** HashTiles of a tile set the caller already has (an actor's locally generated layout, say). */
	UFUNCTION(BlueprintCallable, Category = "ProcGen")
	static int64 HashLayoutTiles(const TArray<FDaLayoutTile>& Tiles);

	/** How many tiles of one type are in the set. Convenience for smokes and debug UI. */
	UFUNCTION(BlueprintCallable, Category = "ProcGen")
	static int32 CountTilesOfType(const TArray<FDaLayoutTile>& Tiles, EDaTileType Type);
};
