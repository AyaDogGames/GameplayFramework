// Copyright Dream Awake Solutions LLC. All Rights Reserved.

#include "DaProcGenActor.h"

#include "DaProcGenModule.h"
#include "DaProcGenParams.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "PCGComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DaProcGenActor)

ADaProcGenActor::ADaProcGenActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// The seed is the only replicated state; everything downstream of it is rebuilt locally.
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// PCG needs the owning actor to have real bounds or it refuses to register the component and then
	// schedules nothing. The box is the only reason this actor has a primitive component at all.
	LayoutBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("LayoutBounds"));
	LayoutBounds->SetupAttachment(Root);
	LayoutBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LayoutBounds->SetGenerateOverlapEvents(false);
	LayoutBounds->SetHiddenInGame(true);
	// Sized from the DEFAULT params here so the value is serialized into every instance: a map-placed
	// actor registers its PCG component long before anything calls ApplyLayoutBounds again.
	ApplyLayoutBounds();

	PCGComponent = CreateDefaultSubobject<UPCGComponent>(TEXT("PCGComponent"));
	// On demand only: nothing may generate before a seed exists, and the seed arrives after BeginPlay.
	PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
	PCGComponent->bActivated = true;
	PCGComponent->bIsComponentPartitioned = false;
}

const FDaDungeonLayoutParams& ADaProcGenActor::ResolveLayoutParams() const
{
	// Asset REPLACES the inline struct — never a per-field merge, which would make "which value am I
	// actually generating with" unanswerable from either place.
	return ParamsAsset ? ParamsAsset->Layout : LayoutParams;
}

void ADaProcGenActor::ApplyLayoutBounds()
{
	if (!LayoutBounds)
	{
		return;
	}

	const FDaDungeonLayoutParams& Params = ResolveLayoutParams();

	// The lattice runs from the actor origin out to (GridExtent * CellSize) on +X/+Y (corner pivot),
	// so the box that describes it is offset by half a span, not centred on the actor.
	const double Span = FMath::Max(1.0, static_cast<double>(Params.GridExtent) * static_cast<double>(Params.CellSize));
	const double Height = FMath::Max(100.0, static_cast<double>(Params.CellSize));

	LayoutBounds->SetBoxExtent(FVector(Span * 0.5, Span * 0.5, Height), /*bUpdateOverlaps=*/false);
	LayoutBounds->SetRelativeLocation(FVector(Span * 0.5, Span * 0.5, 0.0));
}

void ADaProcGenActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ApplyLayoutBounds();
}

void ADaProcGenActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	ApplyLayoutBounds();

	// Assign the graph up front so the component is fully configured before any seed arrives; the
	// generate path re-assigns it anyway, which is what makes a graph swap between runs stick.
	if (PCGComponent && DressingGraph)
	{
		PCGComponent->SetGraphLocal(DressingGraph);
	}
}

void ADaProcGenActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADaProcGenActor, RunSeed);
}

void ADaProcGenActor::ServerGenerate(int32 InSeed)
{
	if (!HasAuthority())
	{
		UE_LOG(DA_ProcGen, Warning, TEXT("%s: ServerGenerate called without authority; ignored."), *GetName());
		return;
	}

	RunSeed = InSeed;
	// OnRep never fires on the authority, so drive the local generate directly.
	GenerateLocal();
}

void ADaProcGenActor::OnRep_RunSeed()
{
	GenerateLocal();
}

void ADaProcGenActor::GenerateLocal()
{
	Tiles.Reset();
	EffectiveSeed = 0;
	++LocalGenerationCount;

	if (RunSeed == 0)
	{
		// A reset is a real state change, not a no-op: leaving the dressing standing would put the
		// previous dungeon's meshes over an empty layout, with every count reporting zero.
		if (PCGComponent)
		{
			PCGComponent->CleanupLocalImmediate(/*bRemoveComponents=*/true);
		}

		UE_LOG(DA_ProcGen, Log, TEXT("%s: RunSeed 0 — layout and dressing cleared."), *GetName());
		OnLayoutGenerated.Broadcast(RunSeed, 0);
		return;
	}

	const FDaDungeonLayoutParams& Params = ResolveLayoutParams();
	const int32 AttemptCap = FMath::Max(1, MaxGenerateAttempts);
	bool bGenerated = false;
	for (int32 Attempt = 0; Attempt < AttemptCap; ++Attempt)
	{
		// Attempt 0 uses the run seed verbatim so a healthy seed hashes to exactly what a caller predicts.
		const int32 AttemptSeed = (Attempt == 0)
			? RunSeed
			: static_cast<int32>(HashCombine(static_cast<uint32>(RunSeed), static_cast<uint32>(Attempt)));

		if (FDaDungeonLayout::Generate(AttemptSeed, Params, Tiles))
		{
			EffectiveSeed = AttemptSeed;
			bGenerated = true;
			if (Attempt > 0)
			{
				UE_LOG(DA_ProcGen, Log, TEXT("%s: layout seed %d needed %d re-roll(s); built from derived seed %d."),
					*GetName(), RunSeed, Attempt, AttemptSeed);
			}
			break;
		}
	}

	if (!bGenerated)
	{
		// Empty layout is a valid, featureless result. Consumers must not treat it as fatal.
		Tiles.Reset();
		UE_LOG(DA_ProcGen, Warning, TEXT("%s: layout generation failed for seed %d after %d attempts; layout is empty."),
			*GetName(), RunSeed, AttemptCap);
	}

	UE_LOG(DA_ProcGen, Log, TEXT("%s: generated %d tiles from seed %d (hash %lld, role %d)."),
		*GetName(), Tiles.Num(), RunSeed, GetLayoutHash(), static_cast<int32>(GetLocalRole()));

	if (PCGComponent)
	{
		// Keep the bounds honest if the params were changed since registration.
		ApplyLayoutBounds();

		// Same graph, same per-point seeds, same everything: the dressing agrees because the tiles do.
		PCGComponent->SetGraphLocal(DressingGraph);
		PCGComponent->Seed = RunSeed;

		if (DressingGraph)
		{
			// GenerateLocal (not Generate): Generate is a NetMulticast and every machine already has the
			// seed. bForce, because the component is otherwise free to consider itself already generated.
			PCGComponent->CleanupLocalImmediate(/*bRemoveComponents=*/true);
			PCGComponent->GenerateLocal(/*bForce=*/true);
		}
		else
		{
			UE_LOG(DA_ProcGen, Warning, TEXT("%s: no DressingGraph set; layout generated but nothing will be dressed."), *GetName());
		}
	}

	// Last, and on every path that got a layout: listeners are told the TILES are ready. The dressing
	// kicked above is asynchronous and is still in flight right now — see FDaOnLayoutGenerated.
	OnLayoutGenerated.Broadcast(RunSeed, Tiles.Num());
}

int64 ADaProcGenActor::GetLayoutHash() const
{
	if (Tiles.IsEmpty())
	{
		return 0;
	}

	return static_cast<int64>(FDaDungeonLayout::HashTiles(Tiles));
}

TArray<FDaLayoutTile> ADaProcGenActor::GetTiles() const
{
	return Tiles;
}

namespace DaProcGenActorLocal
{
	/** Walk the actor and everything attached below it, visiting every ISM component PCG may have added. */
	static void ForEachDressedISM(const AActor* InActor, TFunctionRef<void(const UInstancedStaticMeshComponent*)> Visit)
	{
		if (!InActor)
		{
			return;
		}

		for (const UActorComponent* Component : InActor->GetComponents())
		{
			if (const UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(Component))
			{
				Visit(ISM);
			}
		}

		TArray<AActor*> Attached;
		InActor->GetAttachedActors(Attached, /*bResetArray=*/true, /*bRecursivelyIncludeAttachedActors=*/true);
		for (const AActor* Child : Attached)
		{
			if (!Child)
			{
				continue;
			}

			for (const UActorComponent* Component : Child->GetComponents())
			{
				if (const UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(Component))
				{
					Visit(ISM);
				}
			}
		}
	}
}

int32 ADaProcGenActor::GetDressedInstanceCount() const
{
	int32 Total = 0;
	DaProcGenActorLocal::ForEachDressedISM(this, [&Total](const UInstancedStaticMeshComponent* ISM)
	{
		Total += ISM->GetInstanceCount();
	});

	return Total;
}

TMap<FString, int32> ADaProcGenActor::GetDressedInstanceCountsByMesh() const
{
	TMap<FString, int32> Counts;
	DaProcGenActorLocal::ForEachDressedISM(this, [&Counts](const UInstancedStaticMeshComponent* ISM)
	{
		const UStaticMesh* Mesh = ISM->GetStaticMesh();
		const FString Key = Mesh ? Mesh->GetPathName() : TEXT("None");
		Counts.FindOrAdd(Key) += ISM->GetInstanceCount();
	});

	return Counts;
}
