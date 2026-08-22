// Copyright Dream Awake Solutions LLC. All Rights Reserved.

#include "DaPCGLayoutSource.h"

#include "DaProcGenActor.h"
#include "DaProcGenModule.h"
#include "PCGContext.h"
#include "Data/PCGBasePointData.h"
#include "GameFramework/Actor.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DaPCGLayoutSource)

#define LOCTEXT_NAMESPACE "DaPCGLayoutSource"

FPCGElementPtr UDaPCGLayoutSourceSettings::CreateElement() const
{
	return MakeShared<FDaPCGLayoutSourceElement>();
}

bool FDaPCGLayoutSourceElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDaPCGLayoutSourceElement::Execute);
	check(Context);

	const UDaPCGLayoutSourceSettings* Settings = Context->GetInputSettings<UDaPCGLayoutSourceSettings>();
	check(Settings);

	// The node is only meaningful under a ProcGen actor: that is where the tiles live.
	AActor* TargetActor = Context->GetTargetActor(nullptr);
	ADaProcGenActor* ProcGenActor = Cast<ADaProcGenActor>(TargetActor);
	if (!ProcGenActor && TargetActor)
	{
		ProcGenActor = Cast<ADaProcGenActor>(TargetActor->GetOwner());
	}

	if (!ProcGenActor)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoProcGenActor", "Da Layout Source must run on a DaProcGenActor (none found for this component)."));
		return true;
	}

	const TArray<FDaLayoutTile>& Tiles = ProcGenActor->GetTilesRef();
	const int32 NumTiles = Tiles.Num();

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	FPCGTaggedData& Output = Outputs.Emplace_GetRef();

	UPCGBasePointData* PointData = FPCGContext::NewPointData_AnyThread(Context);
	check(PointData);
	Output.Data = PointData;

	PointData->SetNumPoints(NumTiles, /*bInitializeValues=*/false);
	PointData->AllocateProperties(EPCGPointNativeProperties::All);

	if (NumTiles == 0)
	{
		// An empty layout is legal (see ADaProcGenActor::GenerateLocal); emit empty point data, not an error.
		UE_LOG(DA_ProcGen, Verbose, TEXT("DaLayoutSource: %s has no tiles; emitting empty point data."), *ProcGenActor->GetName());
		return true;
	}

	UPCGMetadata* Metadata = PointData->MutableMetadata();
	FPCGMetadataAttribute<int32>* TileTypeAttribute = nullptr;
	if (Metadata && Settings->TileTypeAttributeName != NAME_None)
	{
		TileTypeAttribute = Metadata->FindOrCreateAttribute<int32>(
			Settings->TileTypeAttributeName, /*DefaultValue=*/0, /*bAllowsInterpolation=*/false, /*bOverrideParent=*/true);

		if (!TileTypeAttribute)
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoTileTypeAttribute", "Could not create the tile-type attribute; points will carry transforms and seeds only."));
		}
	}

	// Corner-pivot tile transforms are grid-local; put them where the actor is.
	const FTransform ActorTransform = ProcGenActor->GetActorTransform();
	// Effective, not inline: an actor tuned by a UDaProcGenParams asset has a different cell size, and
	// point bounds that disagree with the lattice make the spawner's culling and density wrong.
	const double HalfCell = 0.5 * static_cast<double>(ProcGenActor->GetEffectiveLayoutParams().CellSize);
	const FVector CellExtents = Settings->bSetPointBoundsFromCellSize ? FVector(HalfCell) : FVector(1.0);

	FPCGPointValueRanges OutRanges(PointData, /*bAllocate=*/false);

	for (int32 Index = 0; Index < NumTiles; ++Index)
	{
		const FDaLayoutTile& Tile = Tiles[Index];

		OutRanges.TransformRange[Index] = Tile.Transform * ActorTransform;
		OutRanges.DensityRange[Index] = 1.0f;
		OutRanges.SteepnessRange[Index] = 1.0f;
		OutRanges.BoundsMinRange[Index] = -CellExtents;
		OutRanges.BoundsMaxRange[Index] = CellExtents;
		OutRanges.ColorRange[Index] = FVector4::One();
		// The ONLY source of downstream variation: derived from (Grid, RunSeed), never from index or time.
		OutRanges.SeedRange[Index] = Tile.TileSeed;
		OutRanges.MetadataEntryRange[Index] = PCGInvalidEntryKey;

		if (TileTypeAttribute && Metadata)
		{
			Metadata->InitializeOnSet(OutRanges.MetadataEntryRange[Index]);
			TileTypeAttribute->SetValue(OutRanges.MetadataEntryRange[Index], static_cast<int32>(Tile.Type));
		}
	}

	UE_LOG(DA_ProcGen, Verbose, TEXT("DaLayoutSource: emitted %d points for %s."), NumTiles, *ProcGenActor->GetName());

	return true;
}

#undef LOCTEXT_NAMESPACE
