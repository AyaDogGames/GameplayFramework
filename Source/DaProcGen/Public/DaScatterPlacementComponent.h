// Copyright Dream Awake Solutions LLC. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "DaScatterPlacementComponent.generated.h"

class UBoxComponent;
class UPCGComponent;
class UPCGGraphInterface;

/**
 * Broadcast on the SERVER only, once a scatter run's points have been extracted from PCG.
 *
 * The transforms are world space and already truncated to MaxPoints. Consumers decide what to place;
 * this component never spawns anything itself and never references a spawn manager, which is what
 * keeps DaProcGen free of any dependency on the gameplay framework's spawning layer.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDaOnScatterPointsReady, const TArray<FTransform>&, Points);

/**
 * Seeded PCG point scatter, reduced to a list of world transforms.
 *
 * The shape is deliberately the same as ADaProcGenActor's: a seed goes in, a PCG graph runs LOCALLY,
 * and the result is read back on a later frame. The difference is what comes out — this component runs
 * a graph with NO mesh spawner in it, so the points themselves are the product, handed to whoever
 * placed the component through OnPointsReady (or polled with GetLastPoints, which is what a smoke
 * driving the editor from outside uses).
 *
 * SERVER ONLY, on purpose, and NOT for the same reason the dungeon is every-machine. A dungeon is
 * static geometry every machine can rebuild from a seed; scattered gameplay actors are replicated
 * actors that must exist exactly once, on the authority, or every client would spawn its own copy.
 *
 * PCG facts this class exists to encapsulate, all of them learned the hard way in W2/W3:
 *   - UPCGComponent::Generate/SetGraph are NetMulticast UFUNCTIONs. Use GenerateLocal/SetGraphLocal.
 *   - a UPCGComponent on an actor with degenerate bounds is never registered and silently schedules
 *     nothing, so SamplingExtent can put a box on the owner rather than requiring one.
 *   - generation is asynchronous; the result is only readable from the component's
 *     OnPCGGraphGeneratedDelegate (the 5.8 native multicast — the BlueprintAssignable twin is
 *     OnPCGGraphGeneratedExternal), never in the frame that kicked it.
 */
UCLASS(ClassGroup = (ProcGen), meta = (BlueprintSpawnableComponent))
class DAPROCGEN_API UDaScatterPlacementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDaScatterPlacementComponent();

	/** Graph run to produce the points. Its output pin must carry point data; a mesh spawner in it defeats the purpose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcGen")
	TObjectPtr<UPCGGraphInterface> ScatterGraph;

	/** Hard cap on the points handed out. Truncation keeps PCG's own point order, so it is deterministic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcGen", meta = (ClampMin = "1"))
	int32 MaxPoints = 64;

	/**
	 * Half-size of the volume to scatter in. ZERO means "use whatever bounds the owning actor already
	 * has" — which is the ADaProcGenActor case, where LayoutBounds already describes the dungeon. Any
	 * other host actor needs a real primitive or PCG refuses to register the component at all, so a
	 * non-zero extent makes this component provision its own box rather than fail silently.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcGen")
	FVector SamplingExtent = FVector::ZeroVector;

	/** Centre of that box, relative to the owner's root. Ignored when SamplingExtent is zero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProcGen")
	FVector SamplingOffset = FVector::ZeroVector;

	/** Server-only. Fires once per completed scatter run, after the points have been extracted. */
	UPROPERTY(BlueprintAssignable, Category = "ProcGen")
	FDaOnScatterPointsReady OnPointsReady;

	/**
	 * Authority entry point: run ScatterGraph with this seed on a PCG component of our own.
	 *
	 * Returns immediately — PCG is asynchronous. The points arrive on a later frame through
	 * OnPointsReady / GetLastPoints; IsScatterPending() is true in between.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "ProcGen")
	void GenerateScatter(int32 InSeed);

	/**
	 * The last run's points, truncated to MaxPoints. Empty before the first run completes.
	 *
	 * This is the poll surface that exists alongside OnPointsReady: an external driver (a smoke, a
	 * debug HUD tick) can read it without holding a delegate binding across frames.
	 */
	UFUNCTION(BlueprintCallable, Category = "ProcGen")
	TArray<FTransform> GetLastPoints() const;

	/** The same array, no copy, for C++ callers. */
	const TArray<FTransform>& GetLastPointsRef() const { return LastPoints; }

	/** How many points the last run handed out (== GetLastPoints().Num(), without the copy). */
	UFUNCTION(BlueprintCallable, Category = "ProcGen")
	int32 GetLastPointCount() const { return LastPoints.Num(); }

	/** How many points the graph produced BEFORE the MaxPoints truncation. Tells a cap apart from a thin graph. */
	UFUNCTION(BlueprintCallable, Category = "ProcGen")
	int32 GetLastRawPointCount() const { return LastRawPointCount; }

	/** Seed of the last run that was kicked (0 = none). */
	UFUNCTION(BlueprintCallable, Category = "ProcGen")
	int32 GetScatterSeed() const { return ScatterSeed; }

	/** How many runs have COMPLETED and produced points. The "did it actually finish" counter for pollers. */
	UFUNCTION(BlueprintCallable, Category = "ProcGen")
	int32 GetPointsReadyCount() const { return PointsReadyCount; }

	/** True between GenerateScatter and the extraction that follows it. */
	UFUNCTION(BlueprintCallable, Category = "ProcGen")
	bool IsScatterPending() const { return bScatterPending; }

	/** The PCG component this scatter runs on — created on first use, and never the host's other ones. */
	UFUNCTION(BlueprintCallable, Category = "ProcGen")
	UPCGComponent* GetScatterPCGComponent() const { return ScatterPCGComponent; }

	//~Begin UActorComponent interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End UActorComponent interface

protected:
	/**
	 * Our own PCG component, created on demand and remembered.
	 *
	 * Deliberately NOT "the first UPCGComponent on the owner": ADaProcGenActor already owns one that
	 * dresses the dungeon, and reusing it would replace the dungeon with the scatter output.
	 */
	UPCGComponent* ResolveScatterComponent();

	/** Give the owner PCG-legal bounds when SamplingExtent asks for an explicit volume. */
	void ApplySamplingBounds();

	/** Bound to the PCG component's native OnPCGGraphGeneratedDelegate; reads the output and broadcasts. */
	void HandleScatterGenerated(UPCGComponent* InComponent);

	/** Walk the generated data collection and collect every point transform, in PCG's own order. */
	void ExtractPoints(const UPCGComponent* InComponent, TArray<FTransform>& OutPoints, int32& OutRawCount) const;

	/** Box provisioned by SamplingExtent. Null when the owner's own bounds are used instead. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProcGen")
	TObjectPtr<UBoxComponent> ScatterBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProcGen")
	TObjectPtr<UPCGComponent> ScatterPCGComponent;

private:
	/** Last extracted points, world space, already capped. */
	TArray<FTransform> LastPoints;

	int32 LastRawPointCount = 0;
	int32 ScatterSeed = 0;
	int32 PointsReadyCount = 0;
	bool bScatterPending = false;

	/** Handle for the native generated-delegate binding, so EndPlay can take it back off. */
	FDelegateHandle GeneratedDelegateHandle;
};
