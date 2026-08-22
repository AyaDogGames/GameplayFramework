// Copyright Dream Awake Solutions LLC. All Rights Reserved.

#pragma once

#include "PCGElement.h"
#include "PCGSettings.h"

#include "DaPCGLayoutSource.generated.h"

/**
 * PCG source node: turns the owning ADaProcGenActor's already-generated layout into point data.
 *
 * This is the runtime-legal input path for the dungeon layer. PCGCreatePoints' PointsToCreate array is
 * an editor-only authoring surface (spike fact), and PCGDataFromActor would need the tiles reflected
 * onto some other actor first — emitting them straight out of an element keeps the layout in exactly
 * one place (FDaDungeonLayout) and costs no extra actor.
 *
 * The node has NO input pins and one point output pin ("Out"). Per point:
 *   - Transform = tile transform composed with the actor transform (grid lattice, corner pivot)
 *   - Seed      = FDaLayoutTile::TileSeed, i.e. HashCombine(GetTypeHash(Grid), RunSeed) and nothing
 *                 else — that is what makes downstream weighted mesh selection agree across machines
 *   - Density   = 1, Steepness = 1, Color = white, Bounds = one grid cell centred on the point
 *   - int32 attribute TileTypeAttributeName = EDaTileType as an integer, for filter branches
 *
 * Deliberately NOT cacheable: the tile array lives on the actor, outside anything PCG hashes into an
 * element CRC, so a cached result would survive a re-seed and silently hand back the previous dungeon.
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UDaPCGLayoutSourceSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("DaLayoutSource")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("DaPCGLayoutSource", "NodeTitle", "Da Layout Source"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("DaPCGLayoutSource", "NodeTooltip", "Emits one point per tile of the owning DaProcGenActor's generated dungeon layout."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	/** Name of the int32 attribute carrying EDaTileType as an integer (Floor=0, Corridor=1, Wall=2, Entry=3). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PCG_Overridable))
	FName TileTypeAttributeName = FName(TEXT("TileType"));

	/** Size each point's bounds to one grid cell (from the actor's LayoutParams.CellSize). Off leaves PCG's unit bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	bool bSetPointBoundsFromCellSize = true;

protected:
	//~Begin UPCGSettings interface
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return TArray<FPCGPinProperties>(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
	DAPROCGEN_API virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface
};

class FDaPCGLayoutSourceElement : public IPCGElement
{
public:
	/** The tiles are actor state PCG cannot see, so a cache hit would be a stale dungeon. Never cache. */
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

	/** Reads an AActor; do not run it off the game thread. */
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
