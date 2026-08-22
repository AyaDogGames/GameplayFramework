// Copyright Dream Awake Solutions LLC. All Rights Reserved.

#include "DaProcGenLibrary.h"

TArray<FDaLayoutTile> UDaProcGenLibrary::GenerateLayoutTiles(int32 Seed, const FDaDungeonLayoutParams& Params)
{
	TArray<FDaLayoutTile> Tiles;
	FDaDungeonLayout::Generate(Seed, Params, Tiles);
	return Tiles;
}

int64 UDaProcGenLibrary::GetLayoutHash(int32 Seed, const FDaDungeonLayoutParams& Params)
{
	TArray<FDaLayoutTile> Tiles;
	if (!FDaDungeonLayout::Generate(Seed, Params, Tiles))
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
