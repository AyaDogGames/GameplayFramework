// Copyright Dream Awake Solutions LLC. All Rights Reserved.

#include "DaDungeonLayout.h"

#include "DaProcGenModule.h"
#include "Math/RandomStream.h"

namespace DaDungeonLayoutInternal
{
	/** An inclusive integer rectangle of cells. */
	struct FRoomRect
	{
		int32 MinX = 0;
		int32 MinY = 0;
		int32 MaxX = 0;
		int32 MaxY = 0;

		/** Integer centre — biased low on even sides, deliberately, so it is exactly reproducible. */
		FIntPoint Centre() const
		{
			return FIntPoint((MinX + MaxX) / 2, (MinY + MaxY) / 2);
		}

		/** True when this rect comes within Spacing cells of Other (Spacing 0 = plain overlap test). */
		bool OverlapsInflated(const FRoomRect& Other, int32 Spacing) const
		{
			return !(Other.MinX - Spacing > MaxX
				|| Other.MaxX + Spacing < MinX
				|| Other.MinY - Spacing > MaxY
				|| Other.MaxY + Spacing < MinY);
		}
	};

	/** Params as the generator actually uses them: clamped to what the grid can hold. */
	struct FResolvedParams
	{
		int32 Extent = 0;
		int32 SizeMin = 0;
		int32 SizeMax = 0;
		int32 CountMin = 0;
		int32 CountMax = 0;
		int32 Spacing = 0;
		int32 Attempts = 0;
		bool bViable = false;
	};

	static FResolvedParams Resolve(const FDaDungeonLayoutParams& Params)
	{
		FResolvedParams Out;
		Out.Extent = FMath::Max(Params.GridExtent, 1);

		// Rooms keep a one-cell margin so the wall rind always lands inside the grid.
		const int32 MaxRoomSide = Out.Extent - 2;
		if (MaxRoomSide < 2)
		{
			return Out;   // not viable: nothing can be placed
		}

		Out.SizeMin = FMath::Clamp(Params.RoomSizeMin, 2, MaxRoomSide);
		Out.SizeMax = FMath::Clamp(Params.RoomSizeMax, Out.SizeMin, MaxRoomSide);
		Out.CountMin = FMath::Max(Params.RoomCountMin, 1);
		Out.CountMax = FMath::Max(Params.RoomCountMax, Out.CountMin);
		Out.Spacing = FMath::Max(Params.MinRoomSpacing, 0);
		Out.Attempts = FMath::Max(Params.MaxPlacementAttempts, 1);
		Out.bViable = true;
		return Out;
	}

	/** Carve one axis-aligned run of cells (inclusive both ends). */
	static void CarveRun(TSet<FIntPoint>& Cells, int32 FromX, int32 ToX, int32 FromY, int32 ToY)
	{
		const int32 StepX = (ToX > FromX) ? 1 : ((ToX < FromX) ? -1 : 0);
		const int32 StepY = (ToY > FromY) ? 1 : ((ToY < FromY) ? -1 : 0);
		int32 X = FromX;
		int32 Y = FromY;
		Cells.Add(FIntPoint(X, Y));
		while (X != ToX || Y != ToY)
		{
			X += StepX;
			Y += StepY;
			Cells.Add(FIntPoint(X, Y));
		}
	}
}

bool FDaDungeonLayout::Generate(int32 Seed, const FDaDungeonLayoutParams& Params, TArray<FDaLayoutTile>& OutTiles)
{
	using namespace DaDungeonLayoutInternal;

	OutTiles.Reset();

	const FResolvedParams Resolved = Resolve(Params);
	if (!Resolved.bViable)
	{
		UE_LOG(DA_ProcGen, Warning,
			TEXT("FDaDungeonLayout::Generate: GridExtent %d is too small to hold any room."), Params.GridExtent);
		return false;
	}

	// --- rooms: bounded rejection sampling -----------------------------------
	// One stream, integer draws only. Every attempt consumes the same draws in the same order, so
	// the room set is a pure function of (Seed, Params) on every machine.
	FRandomStream Stream(Seed);
	const int32 TargetRoomCount = Stream.RandRange(Resolved.CountMin, Resolved.CountMax);

	TArray<FRoomRect> Rooms;
	Rooms.Reserve(TargetRoomCount);
	for (int32 Attempt = 0; Attempt < Resolved.Attempts && Rooms.Num() < TargetRoomCount; ++Attempt)
	{
		const int32 Width = Stream.RandRange(Resolved.SizeMin, Resolved.SizeMax);
		const int32 Height = Stream.RandRange(Resolved.SizeMin, Resolved.SizeMax);

		// Interior placement window: [1, Extent - 1 - Side] keeps the rind inside the grid.
		const int32 MaxX = Resolved.Extent - 1 - Width;
		const int32 MaxY = Resolved.Extent - 1 - Height;
		if (MaxX < 1 || MaxY < 1)
		{
			continue;
		}

		FRoomRect Candidate;
		Candidate.MinX = Stream.RandRange(1, MaxX);
		Candidate.MinY = Stream.RandRange(1, MaxY);
		Candidate.MaxX = Candidate.MinX + Width - 1;
		Candidate.MaxY = Candidate.MinY + Height - 1;

		bool bClear = true;
		for (const FRoomRect& Placed : Rooms)
		{
			if (Candidate.OverlapsInflated(Placed, Resolved.Spacing))
			{
				bClear = false;
				break;
			}
		}
		if (bClear)
		{
			Rooms.Add(Candidate);
		}
	}

	if (Rooms.Num() < Resolved.CountMin)
	{
		UE_LOG(DA_ProcGen, Warning,
			TEXT("FDaDungeonLayout::Generate: seed %d placed %d of the %d required rooms within %d attempts; caller should re-roll."),
			Seed, Rooms.Num(), Resolved.CountMin, Resolved.Attempts);
		OutTiles.Reset();
		return false;
	}

	// --- room interiors -------------------------------------------------------
	TSet<FIntPoint> RoomCells;
	TSet<FIntPoint> EntryCells;
	for (int32 RoomIndex = 0; RoomIndex < Rooms.Num(); ++RoomIndex)
	{
		const FRoomRect& Room = Rooms[RoomIndex];
		for (int32 X = Room.MinX; X <= Room.MaxX; ++X)
		{
			for (int32 Y = Room.MinY; Y <= Room.MaxY; ++Y)
			{
				const FIntPoint Cell(X, Y);
				RoomCells.Add(Cell);
				if (RoomIndex == 0)
				{
					EntryCells.Add(Cell);
				}
			}
		}
	}

	// --- corridors: each room joins the nearest already-connected room --------
	// Placement order drives the iteration and Manhattan distance breaks toward the lowest index,
	// so the corridor set has no dependence on container iteration order.
	TSet<FIntPoint> CorridorCells;
	TArray<int32> Connected;
	Connected.Add(0);
	for (int32 RoomIndex = 1; RoomIndex < Rooms.Num(); ++RoomIndex)
	{
		const FIntPoint From = Rooms[RoomIndex].Centre();

		int32 NearestRoom = Connected[0];
		int32 NearestDistance = MAX_int32;
		for (int32 CandidateIndex : Connected)
		{
			const FIntPoint To = Rooms[CandidateIndex].Centre();
			const int32 Distance = FMath::Abs(To.X - From.X) + FMath::Abs(To.Y - From.Y);
			if (Distance < NearestDistance)
			{
				NearestDistance = Distance;
				NearestRoom = CandidateIndex;
			}
		}

		const FIntPoint To = Rooms[NearestRoom].Centre();
		CarveRun(CorridorCells, From.X, To.X, From.Y, From.Y);   // X leg, at the source row
		CarveRun(CorridorCells, To.X, To.X, From.Y, To.Y);       // Y leg, at the target column
		Connected.Add(RoomIndex);
	}

	// --- classify + wall rind -------------------------------------------------
	TMap<FIntPoint, EDaTileType> TypeByCell;
	TypeByCell.Reserve(RoomCells.Num() + CorridorCells.Num());
	for (const FIntPoint& Cell : RoomCells)
	{
		TypeByCell.Add(Cell, EntryCells.Contains(Cell) ? EDaTileType::Entry : EDaTileType::Floor);
	}
	for (const FIntPoint& Cell : CorridorCells)
	{
		if (!TypeByCell.Contains(Cell))
		{
			TypeByCell.Add(Cell, EDaTileType::Corridor);
		}
	}

	TArray<FIntPoint> WalkableCells;
	TypeByCell.GenerateKeyArray(WalkableCells);
	for (const FIntPoint& Cell : WalkableCells)
	{
		for (int32 DX = -1; DX <= 1; ++DX)
		{
			for (int32 DY = -1; DY <= 1; ++DY)
			{
				if (DX == 0 && DY == 0)
				{
					continue;
				}
				const FIntPoint Neighbour(Cell.X + DX, Cell.Y + DY);
				if (Neighbour.X < 0 || Neighbour.X >= Resolved.Extent
					|| Neighbour.Y < 0 || Neighbour.Y >= Resolved.Extent)
				{
					continue;
				}
				if (!TypeByCell.Contains(Neighbour))
				{
					TypeByCell.Add(Neighbour, EDaTileType::Wall);
				}
			}
		}
	}

	// --- emit -----------------------------------------------------------------
	const float CellSize = FMath::Max(Params.CellSize, 0.01f);
	OutTiles.Reserve(TypeByCell.Num());
	for (const TPair<FIntPoint, EDaTileType>& Pair : TypeByCell)
	{
		FDaLayoutTile Tile;
		Tile.Grid = Pair.Key;
		Tile.Type = Pair.Value;
		Tile.Transform = FTransform(FVector(Pair.Key.X * CellSize, Pair.Key.Y * CellSize, 0.0));
		Tile.TileSeed = static_cast<int32>(HashCombine(GetTypeHash(Pair.Key), static_cast<uint32>(Seed)));
		OutTiles.Add(MoveTemp(Tile));
	}

	// TMap iteration order is an implementation detail; the ARRAY has to be reproducible too,
	// because the dressing layer compares instance transforms element by element.
	OutTiles.Sort([](const FDaLayoutTile& A, const FDaLayoutTile& B)
	{
		return (A.Grid.X != B.Grid.X) ? (A.Grid.X < B.Grid.X) : (A.Grid.Y < B.Grid.Y);
	});

	UE_LOG(DA_ProcGen, Verbose,
		TEXT("FDaDungeonLayout::Generate: seed %d -> %d rooms, %d tiles."), Seed, Rooms.Num(), OutTiles.Num());
	return true;
}

uint32 FDaDungeonLayout::HashTiles(const TArray<FDaLayoutTile>& Tiles)
{
	if (Tiles.Num() == 0)
	{
		return 0;
	}

	// Integer content only — no transform floats — so the fingerprint agrees bit-exactly between a
	// server and a client that may be running different content scales or platforms.
	TArray<uint32> TileHashes;
	TileHashes.Reserve(Tiles.Num());
	for (const FDaLayoutTile& Tile : Tiles)
	{
		uint32 TileHash = GetTypeHash(Tile.Grid);
		TileHash = HashCombine(TileHash, static_cast<uint32>(Tile.Type));
		TileHash = HashCombine(TileHash, static_cast<uint32>(Tile.TileSeed));
		TileHashes.Add(TileHash);
	}

	// Sorted fold: the fingerprint describes the SET, so a caller that reordered the array (or
	// gathered it from a different container) still agrees.
	TileHashes.Sort();

	uint32 Result = static_cast<uint32>(Tiles.Num());
	for (uint32 TileHash : TileHashes)
	{
		Result = HashCombine(Result, TileHash);
	}
	return Result;
}
