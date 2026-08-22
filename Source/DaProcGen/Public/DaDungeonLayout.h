// Copyright Dream Awake Solutions LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DaDungeonLayout.generated.h"

/** What a generated tile is for. Dressing (mesh choice) keys off this. */
UENUM(BlueprintType)
enum class EDaTileType : uint8
{
	/** Interior of a room. */
	Floor,
	/** Connective tissue carved between rooms. */
	Corridor,
	/** The 8-neighbour rind around the walkable set. */
	Wall,
	/** Interior of the entry room (room 0). Walkable, dressed distinctly. */
	Entry
};

/**
 * One cell of a generated layout.
 *
 * Everything a consumer needs to place something: where on the integer grid, what it is, the world
 * transform on the lattice, and a per-tile seed so downstream variation (PCG mesh selection, prop
 * rotation, ...) is a pure function of the run seed and the cell — never of iteration order.
 */
USTRUCT(BlueprintType)
struct DAPROCGEN_API FDaLayoutTile
{
	GENERATED_BODY()

	/** Integer grid coordinates, origin at the grid's (0,0) corner. */
	UPROPERTY(BlueprintReadOnly, Category = "ProcGen")
	FIntPoint Grid = FIntPoint::ZeroValue;

	/** What this cell is. */
	UPROPERTY(BlueprintReadOnly, Category = "ProcGen")
	EDaTileType Type = EDaTileType::Floor;

	/** Grid * CellSize on XY, Z=0, identity rotation. Corner-pivot convention (AuraContent tiles). */
	UPROPERTY(BlueprintReadOnly, Category = "ProcGen")
	FTransform Transform;

	/** HashCombine(GetTypeHash(Grid), RunSeed), as int32. Feeds PCG per-point Seed. */
	UPROPERTY(BlueprintReadOnly, Category = "ProcGen")
	int32 TileSeed = 0;
};

/** Tuning for FDaDungeonLayout::Generate. All layout decisions are integer-grid; CellSize only scales the output transforms. */
USTRUCT(BlueprintType)
struct DAPROCGEN_API FDaDungeonLayoutParams
{
	GENERATED_BODY()

	/** World size of one cell in uu. 200 matches the AuraContent 1x1 tile lattice. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcGen", meta = (ClampMin = "1"))
	float CellSize = 200.f;

	/** Cells per side of the square grid the dungeon is carved out of. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcGen", meta = (ClampMin = "1"))
	int32 GridExtent = 32;

	/** Fewer rooms than this placed within MaxPlacementAttempts is a failed generate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcGen", meta = (ClampMin = "1"))
	int32 RoomCountMin = 4;

	/** Upper end of the room-count roll (clamped up to RoomCountMin). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcGen")
	int32 RoomCountMax = 7;

	/** Smallest room side, in cells. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcGen", meta = (ClampMin = "2"))
	int32 RoomSizeMin = 3;

	/** Largest room side, in cells (clamped up to RoomSizeMin, and down to what the grid can hold). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcGen")
	int32 RoomSizeMax = 8;

	/** Cells that must separate two room rects. 0 lets rooms touch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcGen", meta = (ClampMin = "0"))
	int32 MinRoomSpacing = 2;

	/** Rejection-sampling budget for the whole room set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcGen", meta = (ClampMin = "1"))
	int32 MaxPlacementAttempts = 200;
};

/**
 * Seeded dungeon layout generator: rooms, L-corridors, wall rind.
 *
 * Deterministic by construction — one FRandomStream and integer grid math, no floating-point
 * accumulation in any decision, and a final sort so the tile ARRAY (not just its hash) is a pure
 * function of (Seed, Params). That is what lets every machine in a session generate the same
 * dungeon from one replicated int instead of replicating geometry.
 *
 * Engine-math only: no spawning, no PCG, no world. Unit-testable off a seed alone.
 */
struct DAPROCGEN_API FDaDungeonLayout
{
	/**
	 * Build a layout.
	 *
	 * @return false only when MaxPlacementAttempts is exhausted below RoomCountMin (OutTiles is left
	 *         empty). Callers re-roll with a derived seed; a caller that cannot must treat an empty
	 *         layout as valid-but-featureless rather than as a crash.
	 */
	static bool Generate(int32 Seed, const FDaDungeonLayoutParams& Params, TArray<FDaLayoutTile>& OutTiles);

	/**
	 * Order-independent fingerprint of a tile set (per-tile integer hashes, sorted, folded).
	 * Integer inputs only — no transform floats — so it agrees bit-exactly across machines.
	 * This is the multiplayer agreement check.
	 */
	static uint32 HashTiles(const TArray<FDaLayoutTile>& Tiles);
};
