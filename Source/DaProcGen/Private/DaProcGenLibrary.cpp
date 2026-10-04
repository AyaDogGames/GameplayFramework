// Copyright Dream Awake Solutions LLC. All Rights Reserved.

#include "DaProcGenLibrary.h"

TArray<FDaLayoutTile> UDaProcGenLibrary::GenerateLayoutTiles(int32 Seed, const FDaDungeonLayoutParams& Params, int32 MaxAttempts)
{
	TArray<FDaLayoutTile> Tiles;
	int32 EffectiveSeed = 0;
	FDaDungeonLayout::GenerateWithReroll(Seed, Params, MaxAttempts, Tiles, EffectiveSeed);
	return Tiles;
}

int64 UDaProcGenLibrary::GetLayoutHash(int32 Seed, const FDaDungeonLayoutParams& Params, int32 MaxAttempts)
{
	TArray<FDaLayoutTile> Tiles;
	int32 EffectiveSeed = 0;
	if (FDaDungeonLayout::GenerateWithReroll(Seed, Params, MaxAttempts, Tiles, EffectiveSeed) == INDEX_NONE)
	{
		return 0;
	}
	return static_cast<int64>(FDaDungeonLayout::HashTiles(Tiles));
}

int64 UDaProcGenLibrary::HashLayoutTiles(const TArray<FDaLayoutTile>& Tiles)
{
	return static_cast<int64>(FDaDungeonLayout::HashTiles(Tiles));
}

int32 UDaProcGenLibrary::CountTilesOfType(const TArray<FDaLayoutTile>& Tiles, EDaTileType Type)
{
	int32 Count = 0;
	for (const FDaLayoutTile& Tile : Tiles)
	{
		if (Tile.Type == Type)
		{
			++Count;
		}
	}
	return Count;
}
