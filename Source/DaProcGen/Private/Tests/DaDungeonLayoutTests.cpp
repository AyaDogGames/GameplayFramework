// Copyright Dream Awake Solutions LLC. All Rights Reserved.

#include "CoreMinimal.h"
#include "Algo/Reverse.h"
#include "Misc/AutomationTest.h"

#include "DaDungeonLayout.h"

#if WITH_AUTOMATION_TESTS

namespace DaProcGenTestHelpers
{
	/** Params that generate a real dungeon; every test that wants a working layout starts here. */
	static FDaDungeonLayoutParams GoodParams()
	{
		FDaDungeonLayoutParams Params;   // struct defaults: 32 cells, 4-7 rooms of 3-8, spacing 2
		return Params;
	}

	static bool IsWalkable(EDaTileType Type)
	{
		return Type == EDaTileType::Floor || Type == EDaTileType::Corridor || Type == EDaTileType::Entry;
	}

	static int32 CountOfType(const TArray<FDaLayoutTile>& Tiles, EDaTileType Type)
	{
		int32 Count = 0;
		for (const FDaLayoutTile& Tile : Tiles)
		{
			Count += (Tile.Type == Type) ? 1 : 0;
		}
		return Count;
	}

	/** Tiles are equal for our purposes when their integer content and their transform agree exactly. */
	static bool TilesIdentical(const FDaLayoutTile& A, const FDaLayoutTile& B)
	{
		return A.Grid == B.Grid
			&& A.Type == B.Type
			&& A.TileSeed == B.TileSeed
			&& A.Transform.Equals(B.Transform, 0.0);
	}
}

using namespace DaProcGenTestHelpers;

/**
 * The multiplayer promise: one seed in, the same dungeon out — the same ARRAY, not merely the same
 * fingerprint, because W3's dressing asserts identical instance transforms too.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDaProcGenLayoutDeterminismTest,
	"DaProcGen.Layout.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::SmokeFilter)

bool FDaProcGenLayoutDeterminismTest::RunTest(const FString& Parameters)
{
	const FDaDungeonLayoutParams Params = GoodParams();

	TArray<FDaLayoutTile> First;
	TArray<FDaLayoutTile> Second;
	UTEST_TRUE("Generate(1337) succeeds", FDaDungeonLayout::Generate(1337, Params, First));
	UTEST_TRUE("Generate(1337) again succeeds", FDaDungeonLayout::Generate(1337, Params, Second));
	UTEST_TRUE("layout is not empty", First.Num() > 0);

	UTEST_EQUAL("same seed produces the same tile count", Second.Num(), First.Num());
	bool bAllIdentical = true;
	for (int32 Index = 0; Index < First.Num() && Index < Second.Num(); ++Index)
	{
		bAllIdentical &= TilesIdentical(First[Index], Second[Index]);
	}
	TestTrue(TEXT("same seed produces an element-wise identical tile array"), bAllIdentical);

	const uint32 HashA = FDaDungeonLayout::HashTiles(First);
	const uint32 HashB = FDaDungeonLayout::HashTiles(Second);
	TestEqual(TEXT("same seed produces the same hash"), static_cast<int64>(HashA), static_cast<int64>(HashB));
	TestTrue(TEXT("hash of a real layout is non-zero"), HashA != 0);

	// Order independence: the fingerprint is a sorted fold, so a shuffled set must hash the same.
	TArray<FDaLayoutTile> Reversed = First;
	Algo::Reverse(Reversed);
	TestEqual(TEXT("hash is order independent"),
		static_cast<int64>(FDaDungeonLayout::HashTiles(Reversed)), static_cast<int64>(HashA));

	// Different seeds must actually diverge (checked over several, so one unlucky pair is not the test).
	int32 Divergent = 0;
	const int32 OtherSeeds[] = { 1338, 4242, 7777, -19 };
	for (int32 Seed : OtherSeeds)
	{
		TArray<FDaLayoutTile> Other;
		if (FDaDungeonLayout::Generate(Seed, Params, Other))
		{
			Divergent += (FDaDungeonLayout::HashTiles(Other) != HashA) ? 1 : 0;
		}
	}
	TestEqual(TEXT("every other seed produced a different layout hash"), Divergent, static_cast<int32>(UE_ARRAY_COUNT(OtherSeeds)));

	return true;
}

/** A dungeon whose rooms cannot be walked between is not a dungeon. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDaProcGenLayoutConnectivityTest,
	"DaProcGen.Layout.Connectivity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::SmokeFilter)

bool FDaProcGenLayoutConnectivityTest::RunTest(const FString& Parameters)
{
	const FDaDungeonLayoutParams Params = GoodParams();

	// Several seeds: connectivity is a property of the algorithm, not of one lucky roll.
	const int32 Seeds[] = { 1, 1337, 4242, 7777, 90210 };
	for (int32 Seed : Seeds)
	{
		TArray<FDaLayoutTile> Tiles;
		UTEST_TRUE(*FString::Printf(TEXT("Generate(%d) succeeds"), Seed),
			FDaDungeonLayout::Generate(Seed, Params, Tiles));

		TSet<FIntPoint> Walkable;
		FIntPoint EntryCell(MIN_int32, MIN_int32);
		for (const FDaLayoutTile& Tile : Tiles)
		{
			if (IsWalkable(Tile.Type))
			{
				Walkable.Add(Tile.Grid);
			}
			if (Tile.Type == EDaTileType::Entry && EntryCell.X == MIN_int32)
			{
				EntryCell = Tile.Grid;
			}
		}
		UTEST_TRUE(*FString::Printf(TEXT("seed %d has an entry tile"), Seed), EntryCell.X != MIN_int32);

		// 4-neighbour flood fill from the entry room.
		TSet<FIntPoint> Visited;
		TArray<FIntPoint> Frontier;
		Visited.Add(EntryCell);
		Frontier.Add(EntryCell);
		const FIntPoint Neighbours[] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
		while (Frontier.Num() > 0)
		{
			const FIntPoint Cell = Frontier.Pop(EAllowShrinking::No);
			for (const FIntPoint& Offset : Neighbours)
			{
				const FIntPoint Next = Cell + Offset;
				if (Walkable.Contains(Next) && !Visited.Contains(Next))
				{
					Visited.Add(Next);
					Frontier.Add(Next);
				}
			}
		}
		TestEqual(*FString::Printf(TEXT("seed %d: flood fill from the entry reaches every walkable tile"), Seed),
			Visited.Num(), Walkable.Num());
	}

	return true;
}

/** Structural invariants: one tile per cell, on the lattice, in bounds, walled all the way round. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDaProcGenLayoutStructureTest,
	"DaProcGen.Layout.Structure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::SmokeFilter)

bool FDaProcGenLayoutStructureTest::RunTest(const FString& Parameters)
{
	FDaDungeonLayoutParams Params = GoodParams();
	Params.CellSize = 200.f;

	TArray<FDaLayoutTile> Tiles;
	UTEST_TRUE("Generate succeeds", FDaDungeonLayout::Generate(4242, Params, Tiles));
	UTEST_TRUE("layout is not empty", Tiles.Num() > 0);

	TMap<FIntPoint, EDaTileType> ByCell;
	bool bInBounds = true;
	bool bOnLattice = true;
	bool bUniqueCells = true;
	for (const FDaLayoutTile& Tile : Tiles)
	{
		bUniqueCells &= !ByCell.Contains(Tile.Grid);
		ByCell.Add(Tile.Grid, Tile.Type);

		bInBounds &= Tile.Grid.X >= 0 && Tile.Grid.X < Params.GridExtent
			&& Tile.Grid.Y >= 0 && Tile.Grid.Y < Params.GridExtent;

		const FVector Expected(Tile.Grid.X * Params.CellSize, Tile.Grid.Y * Params.CellSize, 0.0);
		bOnLattice &= Tile.Transform.GetLocation().Equals(Expected, 0.0)
			&& Tile.Transform.GetRotation().Equals(FQuat::Identity, 0.0)
			&& Tile.Transform.GetScale3D().Equals(FVector::OneVector, 0.0);
	}
	TestTrue(TEXT("exactly one tile per grid cell"), bUniqueCells);
	TestTrue(TEXT("every tile is inside the grid"), bInBounds);
	TestTrue(TEXT("every tile transform sits on the CellSize lattice, unrotated and unscaled"), bOnLattice);

	const int32 WallCount = CountOfType(Tiles, EDaTileType::Wall);
	const int32 FloorCount = CountOfType(Tiles, EDaTileType::Floor);
	const int32 EntryCount = CountOfType(Tiles, EDaTileType::Entry);
	const int32 CorridorCount = CountOfType(Tiles, EDaTileType::Corridor);
	TestTrue(TEXT("the layout has walls"), WallCount > 0);
	TestTrue(TEXT("the layout has room floor"), FloorCount > 0);
	TestTrue(TEXT("the layout has exactly one entry room's worth of entry tiles"), EntryCount > 0);
	TestTrue(TEXT("the layout has corridors"), CorridorCount > 0);

	// The rind is complete: no walkable tile is exposed to an unclassified cell, and no wall floats.
	bool bRindComplete = true;
	bool bNoFloatingWalls = true;
	for (const TPair<FIntPoint, EDaTileType>& Pair : ByCell)
	{
		bool bTouchesWalkable = false;
		for (int32 DX = -1; DX <= 1; ++DX)
		{
			for (int32 DY = -1; DY <= 1; ++DY)
			{
				if (DX == 0 && DY == 0)
				{
					continue;
				}
				const FIntPoint Neighbour(Pair.Key.X + DX, Pair.Key.Y + DY);
				const EDaTileType* NeighbourType = ByCell.Find(Neighbour);
				if (IsWalkable(Pair.Value))
				{
					// Every neighbour of a walkable cell must exist and be walkable or wall.
					bRindComplete &= (NeighbourType != nullptr);
				}
				if (NeighbourType && IsWalkable(*NeighbourType))
				{
					bTouchesWalkable = true;
				}
			}
		}
		if (Pair.Value == EDaTileType::Wall)
		{
			bNoFloatingWalls &= bTouchesWalkable;
		}
	}
	TestTrue(TEXT("every walkable tile is fully surrounded (8-neighbour rind is complete)"), bRindComplete);
	TestTrue(TEXT("no wall tile floats away from the walkable set"), bNoFloatingWalls);

	return true;
}

/** The per-tile seed is the documented pure function of cell and run seed — PCG variation rides on it. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDaProcGenLayoutTileSeedTest,
	"DaProcGen.Layout.TileSeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::SmokeFilter)

bool FDaProcGenLayoutTileSeedTest::RunTest(const FString& Parameters)
{
	const FDaDungeonLayoutParams Params = GoodParams();
	const int32 RunSeed = 7777;

	TArray<FDaLayoutTile> Tiles;
	UTEST_TRUE("Generate succeeds", FDaDungeonLayout::Generate(RunSeed, Params, Tiles));

	bool bSeedsMatchFormula = true;
	for (const FDaLayoutTile& Tile : Tiles)
	{
		const int32 Expected = static_cast<int32>(HashCombine(GetTypeHash(Tile.Grid), static_cast<uint32>(RunSeed)));
		bSeedsMatchFormula &= (Tile.TileSeed == Expected);
	}
	TestTrue(TEXT("every TileSeed is HashCombine(GetTypeHash(Grid), RunSeed)"), bSeedsMatchFormula);

	// A different run seed must re-seed the same cells, or PCG variation would be identical run to run.
	TArray<FDaLayoutTile> Other;
	UTEST_TRUE("Generate with another seed succeeds", FDaDungeonLayout::Generate(RunSeed + 1, Params, Other));
	TMap<FIntPoint, int32> OtherSeedByCell;
	for (const FDaLayoutTile& Tile : Other)
	{
		OtherSeedByCell.Add(Tile.Grid, Tile.TileSeed);
	}
	int32 Shared = 0;
	int32 Differing = 0;
	for (const FDaLayoutTile& Tile : Tiles)
	{
		if (const int32* OtherSeed = OtherSeedByCell.Find(Tile.Grid))
		{
			++Shared;
			Differing += (*OtherSeed != Tile.TileSeed) ? 1 : 0;
		}
	}
	TestTrue(TEXT("the two layouts share cells to compare"), Shared > 0);
	TestEqual(TEXT("every shared cell got a different tile seed under a different run seed"), Differing, Shared);

	return true;
}

/** Unsatisfiable params fail cleanly, and the documented derived-seed re-roll gets a caller out. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDaProcGenLayoutFailurePathTest,
	"DaProcGen.Layout.FailurePath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::SmokeFilter)

bool FDaProcGenLayoutFailurePathTest::RunTest(const FString& Parameters)
{
	// Six 4x4 rooms with 3 cells of spacing cannot fit in an 8x8 grid, and the budget is tiny.
	FDaDungeonLayoutParams Impossible;
	Impossible.GridExtent = 8;
	Impossible.RoomCountMin = 6;
	Impossible.RoomCountMax = 6;
	Impossible.RoomSizeMin = 4;
	Impossible.RoomSizeMax = 4;
	Impossible.MinRoomSpacing = 3;
	Impossible.MaxPlacementAttempts = 5;

	TArray<FDaLayoutTile> Tiles;
	Tiles.AddDefaulted(3);   // pre-fill: a failed generate must not leave the caller's junk behind
	TestFalse(TEXT("unsatisfiable params fail"), FDaDungeonLayout::Generate(99, Impossible, Tiles));
	TestEqual(TEXT("a failed generate empties the output array"), Tiles.Num(), 0);
	TestEqual(TEXT("hash of an empty layout is 0"), static_cast<int64>(FDaDungeonLayout::HashTiles(Tiles)), static_cast<int64>(0));

	// Failing is deterministic too: the same impossible params fail the same way every time.
	TArray<FDaLayoutTile> Again;
	TestFalse(TEXT("unsatisfiable params fail again"), FDaDungeonLayout::Generate(99, Impossible, Again));

	// The bounded re-roll a caller is told to do (Seed' = HashCombine(Seed, Attempt), cap 8) must
	// terminate: with sane params it lands, with impossible params it gives up with an empty layout.
	auto BoundedReRoll = [](int32 Seed, const FDaDungeonLayoutParams& Params, TArray<FDaLayoutTile>& Out) -> int32
	{
		for (int32 Attempt = 0; Attempt < 8; ++Attempt)
		{
			const int32 DerivedSeed = (Attempt == 0)
				? Seed
				: static_cast<int32>(HashCombine(static_cast<uint32>(Seed), static_cast<uint32>(Attempt)));
			if (FDaDungeonLayout::Generate(DerivedSeed, Params, Out))
			{
				return Attempt;
			}
		}
		Out.Reset();
		return INDEX_NONE;
	};

	TArray<FDaLayoutTile> Recovered;
	const int32 ImpossibleAttempts = BoundedReRoll(99, Impossible, Recovered);
	TestEqual(TEXT("the bounded re-roll gives up on impossible params"), ImpossibleAttempts, INDEX_NONE);
	TestEqual(TEXT("giving up leaves an empty layout, not a partial one"), Recovered.Num(), 0);

	// A grid that is only just big enough: the re-roll should still land, and land deterministically.
	FDaDungeonLayoutParams Tight = GoodParams();
	Tight.GridExtent = 16;
	Tight.RoomCountMin = 3;
	Tight.RoomCountMax = 4;
	Tight.RoomSizeMin = 3;
	Tight.RoomSizeMax = 4;
	Tight.MaxPlacementAttempts = 60;

	TArray<FDaLayoutTile> TightA;
	TArray<FDaLayoutTile> TightB;
	const int32 AttemptsA = BoundedReRoll(2026, Tight, TightA);
	const int32 AttemptsB = BoundedReRoll(2026, Tight, TightB);
	TestTrue(TEXT("the bounded re-roll lands on tight-but-satisfiable params"), AttemptsA != INDEX_NONE);
	TestEqual(TEXT("the re-roll takes the same number of attempts every time"), AttemptsB, AttemptsA);
	TestEqual(TEXT("the re-rolled layout is identical run to run"),
		static_cast<int64>(FDaDungeonLayout::HashTiles(TightB)),
		static_cast<int64>(FDaDungeonLayout::HashTiles(TightA)));

	return true;
}

#endif // WITH_AUTOMATION_TESTS
