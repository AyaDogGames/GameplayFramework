// Copyright Dream Awake Solutions LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DaDungeonLayout.h"
#include "GameFramework/Actor.h"

#include "DaProcGenActor.generated.h"

class UBoxComponent;
class UPCGComponent;
class UPCGGraphInterface;

/**
 * Actor that owns one generated dungeon: the seed, the tiles built from it, and the PCG component that
 * dresses them with meshes.
 *
 * Multiplayer model (spec section 4): the SERVER picks the seed and it replicates as a single int.
 * Every machine — server and every client — then generates its own tiles from that int and kicks its
 * own local PCG generate. No geometry, no transforms, and no point data cross the wire; the agreement
 * comes from FDaDungeonLayout being integer-deterministic, which is what unit W2 exists to prove.
 *
 * Generation is on demand, never on BeginPlay: RunSeed 0 means "no dungeon yet", and the PCG component
 * is constructed with GenerateOnDemand so nothing fires before a seed exists.
 */
UCLASS(BlueprintType)
class DAPROCGEN_API ADaProcGenActor : public AActor
{
	GENERATED_BODY()

public:
	ADaProcGenActor();

	/** Tuning handed to FDaDungeonLayout::Generate. Must match on every machine — it is config, not replicated state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcGen")
	FDaDungeonLayoutParams LayoutParams;

	/** Graph run against the emitted tile points. Null generates the layout and dresses nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcGen")
	TObjectPtr<UPCGGraphInterface> DressingGraph;

	/** The run's seed. 0 = nothing generated yet. Server-set, replicated, and the ONLY thing that crosses the wire. */
	UPROPERTY(ReplicatedUsing = OnRep_RunSeed, BlueprintReadOnly, Category = "ProcGen")
	int32 RunSeed = 0;

	/** How many derived-seed re-rolls to try before giving up and leaving the layout empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcGen", meta = (ClampMin = "1"))
	int32 MaxGenerateAttempts = 8;

	/** Authority entry point: set the run seed and generate locally. Clients follow via OnRep_RunSeed. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "ProcGen")
	void ServerGenerate(int32 InSeed);

	/** Fingerprint of the LOCALLY generated tiles (0 before a successful generate). The MP agreement check. */
	UFUNCTION(BlueprintCallable, Category = "ProcGen")
	int64 GetLayoutHash() const;

	/** Copy of the locally generated tiles (Blueprint/Python surface). C++ callers want GetTilesRef(). */
	UFUNCTION(BlueprintCallable, Category = "ProcGen")
	TArray<FDaLayoutTile> GetTiles() const;

	/** The locally generated tiles, no copy. The PCG source element reads this. */
	const TArray<FDaLayoutTile>& GetTilesRef() const { return Tiles; }

	/** Number of locally generated tiles. */
	UFUNCTION(BlueprintCallable, Category = "ProcGen")
	int32 GetTileCount() const { return Tiles.Num(); }

	/** The seed the layout was actually built from — RunSeed, or a derived re-roll seed if the first roll failed. */
	UFUNCTION(BlueprintCallable, Category = "ProcGen")
	int32 GetEffectiveSeed() const { return EffectiveSeed; }

	/**
	 * Total instance count across every InstancedStaticMeshComponent PCG dressed this actor with
	 * (the actor's own components plus anything attached below it). The runtime-generation assert.
	 */
	UFUNCTION(BlueprintCallable, Category = "ProcGen")
	int32 GetDressedInstanceCount() const;

	/** Per-mesh instance counts, keyed by the static mesh's path name. W3's determinism assert reads this. */
	UFUNCTION(BlueprintCallable, Category = "ProcGen")
	TMap<FString, int32> GetDressedInstanceCountsByMesh() const;

	/** The PCG component doing the dressing. */
	UFUNCTION(BlueprintCallable, Category = "ProcGen")
	UPCGComponent* GetPCGComponent() const { return PCGComponent; }

	//~Begin AActor interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	//~End AActor interface

protected:
	/** Clients: the seed landed, build the same dungeon locally. */
	UFUNCTION()
	void OnRep_RunSeed();

	/**
	 * Build tiles from RunSeed and kick the dressing graph. Runs identically on server and clients.
	 *
	 * Bounded re-roll per the spec: Generate() failing (params unsatisfiable for this seed) is retried
	 * with Seed' = HashCombine(Seed, Attempt) up to MaxGenerateAttempts, then logs a warning and leaves
	 * an empty layout — which consumers must treat as valid-but-featureless, never as a crash.
	 */
	void GenerateLocal();

	/**
	 * Size LayoutBounds to the volume the layout will occupy.
	 *
	 * Not cosmetic: UPCGComponent REFUSES TO REGISTER on an actor whose primitive components give it
	 * a degenerate box ("[RegisterOrUpdateExecutionSource] Component has invalid bounds, not
	 * registered nor updated"), and an unregistered component silently schedules no generation task.
	 * A bare USceneComponent root is exactly that case, which is why this actor carries a box.
	 */
	void ApplyLayoutBounds();

	/** The volume the dungeon occupies. Gives PCG its bounds; W4's scatter layer gets a shape to sample. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProcGen")
	TObjectPtr<UBoxComponent> LayoutBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProcGen")
	TObjectPtr<UPCGComponent> PCGComponent;

private:
	/** Locally generated, never replicated. */
	TArray<FDaLayoutTile> Tiles;

	/** Seed the current Tiles were built from (differs from RunSeed only after a re-roll). */
	int32 EffectiveSeed = 0;
};
