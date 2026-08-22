// Copyright Dream Awake Solutions LLC. All Rights Reserved.

#include "DaScatterPlacementComponent.h"

#include "DaProcGenModule.h"
#include "Components/BoxComponent.h"
#include "Data/PCGBasePointData.h"
#include "GameFramework/Actor.h"
#include "PCGComponent.h"
#include "PCGData.h"
#include "PCGGraph.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DaScatterPlacementComponent)

UDaScatterPlacementComponent::UDaScatterPlacementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// The component holds no replicated state: the SERVER scatters, and whatever the server spawns at
	// those transforms is what reaches clients. Replicating the point list would duplicate that.
	SetIsReplicatedByDefault(false);
}

void UDaScatterPlacementComponent::ApplySamplingBounds()
{
	if (SamplingExtent.IsNearlyZero())
	{
		// Caller wants the owner's existing bounds (ADaProcGenActor::LayoutBounds, typically).
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (!ScatterBounds)
	{
		USceneComponent* Root = Owner->GetRootComponent();

		ScatterBounds = NewObject<UBoxComponent>(Owner, TEXT("DaScatterBounds"));
		ScatterBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ScatterBounds->SetGenerateOverlapEvents(false);
		ScatterBounds->SetHiddenInGame(true);

		if (Root)
		{
			ScatterBounds->SetupAttachment(Root);
		}
		else
		{
			// A bare AActor host has no root at all, and an unrooted primitive gives PCG the degenerate
			// bounds that make it refuse to register. Rooting the box is the least invasive fix: the
			// actor had no transform hierarchy to disturb.
			Owner->SetRootComponent(ScatterBounds);
		}

		Owner->AddInstanceComponent(ScatterBounds);
		ScatterBounds->RegisterComponent();
	}

	ScatterBounds->SetBoxExtent(SamplingExtent, /*bUpdateOverlaps=*/false);

	// Only when the box hangs off a root: on an actor whose root IS the box, a relative location is the
	// ACTOR's location, so honouring the offset here would silently teleport the host.
	if (ScatterBounds != Owner->GetRootComponent())
	{
		ScatterBounds->SetRelativeLocation(SamplingOffset);
	}
	else if (!SamplingOffset.IsNearlyZero())
	{
		UE_LOG(DA_ProcGen, Warning, TEXT("%s: SamplingOffset ignored — the scatter box is the host's root, so place the host itself."), *GetName());
	}
}

UPCGComponent* UDaScatterPlacementComponent::ResolveScatterComponent()
{
	if (ScatterPCGComponent)
	{
		return ScatterPCGComponent;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	// A NEW component, never one already on the actor: ADaProcGenActor's PCG component is dressing the
	// dungeon, and pointing the scatter graph at it would tear the dungeon down and replace it.
	ScatterPCGComponent = NewObject<UPCGComponent>(Owner, TEXT("DaScatterPCGComponent"));
	ScatterPCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
	ScatterPCGComponent->bActivated = true;
	ScatterPCGComponent->bIsComponentPartitioned = false;
	Owner->AddInstanceComponent(ScatterPCGComponent);
	ScatterPCGComponent->RegisterComponent();

	// The 5.8 native completion hook, verified on UPCGComponent: FOnPCGGraphGenerated
	// OnPCGGraphGeneratedDelegate, broadcast OUTSIDE the WITH_EDITOR block in PostProcessGraph, i.e.
	// runtime-legal, and fired only after GeneratedGraphOutput has been filled in.
	GeneratedDelegateHandle = ScatterPCGComponent->OnPCGGraphGeneratedDelegate.AddUObject(
		this, &UDaScatterPlacementComponent::HandleScatterGenerated);

	return ScatterPCGComponent;
}

void UDaScatterPlacementComponent::GenerateScatter(int32 InSeed)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		UE_LOG(DA_ProcGen, Warning, TEXT("%s: GenerateScatter called without authority; ignored."), *GetName());
		return;
	}

	if (!ScatterGraph)
	{
		UE_LOG(DA_ProcGen, Warning, TEXT("%s: no ScatterGraph set; nothing to generate."), *GetName());
		return;
	}

	ApplySamplingBounds();

	UPCGComponent* PCG = ResolveScatterComponent();
	if (!PCG)
	{
		UE_LOG(DA_ProcGen, Warning, TEXT("%s: could not create a PCG component to scatter with."), *GetName());
		return;
	}

	ScatterSeed = InSeed;
	// SetGraphLocal/GenerateLocal, not SetGraph/Generate: the latter pair are NetMulticast UFUNCTIONs,
	// and a scatter that multicast itself would run on every client as well as the server.
	PCG->SetGraphLocal(ScatterGraph);
	PCG->Seed = InSeed;

	bScatterPending = true;
	PCG->CleanupLocalImmediate(/*bRemoveComponents=*/true);
	PCG->GenerateLocal(/*bForce=*/true);

	UE_LOG(DA_ProcGen, Log, TEXT("%s: scatter kicked with seed %d (graph %s)."),
		*GetName(), InSeed, *GetNameSafe(ScatterGraph));
}

void UDaScatterPlacementComponent::ExtractPoints(const UPCGComponent* InComponent, TArray<FTransform>& OutPoints, int32& OutRawCount) const
{
	OutPoints.Reset();
	OutRawCount = 0;

	if (!InComponent)
	{
		return;
	}

	const FPCGDataCollection& Output = InComponent->GetGeneratedGraphOutput();
	for (const FPCGTaggedData& Tagged : Output.TaggedData)
	{
		// The double hop the smokes already do by hand: TaggedData entry -> Data -> point data.
		const UPCGBasePointData* PointData = Cast<const UPCGBasePointData>(Tagged.Data);
		if (!PointData)
		{
			continue;
		}

		const int32 NumPoints = PointData->GetNumPoints();
		OutRawCount += NumPoints;

		for (int32 Index = 0; Index < NumPoints; ++Index)
		{
			// Truncation keeps PCG's own point order rather than sorting by anything we invent, so the
			// same seed hands out the same first N points on every run and on every machine.
			if (OutPoints.Num() >= FMath::Max(1, MaxPoints))
			{
				return;
			}

			OutPoints.Add(PointData->GetTransform(Index));
		}
	}
}

void UDaScatterPlacementComponent::HandleScatterGenerated(UPCGComponent* InComponent)
{
	if (!InComponent || InComponent != ScatterPCGComponent)
	{
		// Someone else's component; ours is the only one we bound to, but be explicit about it.
		return;
	}

	ExtractPoints(InComponent, LastPoints, LastRawPointCount);
	bScatterPending = false;
	++PointsReadyCount;

	UE_LOG(DA_ProcGen, Log, TEXT("%s: scatter seed %d produced %d point(s) (%d before the MaxPoints cap of %d)."),
		*GetName(), ScatterSeed, LastPoints.Num(), LastRawPointCount, MaxPoints);

	OnPointsReady.Broadcast(LastPoints);
}

TArray<FTransform> UDaScatterPlacementComponent::GetLastPoints() const
{
	return LastPoints;
}

void UDaScatterPlacementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ScatterPCGComponent && GeneratedDelegateHandle.IsValid())
	{
		ScatterPCGComponent->OnPCGGraphGeneratedDelegate.Remove(GeneratedDelegateHandle);
		GeneratedDelegateHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}
