// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DaConditionComponent.generated.h"

class APawn;
class UDaInventoryComponent;
class UDaItemDefinition;
class UMaterialInstanceDynamic;
class UMaterialInterface;
struct FDaInventoryEntry;

/**
 * UDaConditionComponent
 *
 * Wear driver for one item's visual actor (an equipment actor, a world pickup, a shop display).
 * It keeps that actor's materials in step with the item INSTANCE's Item.Stat.Condition through
 * the parameter contract in docs/ConditionMaterialContract.md:
 *
 *   Da_Wear_Intensity  1 - Condition/Cap  (0 = factory fresh, 1 = one hit from broken)
 *   Da_Wear_Seed       stable per-instance variation in [0,1], derived from the item's GUID
 *   Da_Wear_Grade      Grade/10
 *
 * Purely cosmetic: it replicates nothing, because everything it reads (the entry's stats) is
 * already replicated, so a listen host and a remote client compute the same numbers from their
 * own copy of the inventory. Only material slots that implement at least one contract parameter
 * are touched — dropping this component on an actor whose materials know nothing about wear
 * costs one scan and then does nothing.
 *
 * Which item a visual represents is usually implicit: an equipment actor asks the wearer's
 * UDaEquipmentManagerComponent which entry spawned it. Visuals nobody equips (a rack of loot,
 * a repair-shop preview) name their item explicitly through SetItem.
 */
UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GAMEPLAYFRAMEWORK_API UDaConditionComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UDaConditionComponent();

	/** Item instance this visual represents. Leave invalid on an equipment actor — the component
	 *  finds the item whose equipment entry spawned its owner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Condition")
	FGuid ItemID;

	/** Name this visual's item and push its wear immediately. The item is looked up in the
	 *  wearer's inventory (owner pawn -> PlayerState), which is what an equipment actor wants. */
	UFUNCTION(BlueprintCallable, Category="Condition")
	void SetItemID(FGuid NewItemID);

	/** Name both halves: for a visual that hangs off no pawn (shop display, loot rack, preview)
	 *  there is no inventory to find, so the caller supplies the one holding the item. */
	UFUNCTION(BlueprintCallable, Category="Condition")
	void SetItem(UDaInventoryComponent* Inventory, FGuid NewItemID);

	/** Push wear numbers the caller already has, bypassing item resolution entirely, and stop
	 *  looking for an item. For a visual whose item is NOT in any inventory to be found: a dropped
	 *  pickup (the entry left the inventory when it hit the ground, but the actor must still look as
	 *  worn as the item that was dropped), a loot preview built from a template, a cinematic prop.
	 *  All three values are clamped to [0,1] — Grade is the CONTRACT value (Grade/10), not 0..10. */
	UFUNCTION(BlueprintCallable, Category="Condition")
	void SetExplicitWear(float Intensity, float Seed, float Grade);

	/** The item the component is currently driving: the explicit ItemID, or the one it resolved
	 *  from the owning pawn's equipment. Invalid until it has resolved one. */
	UFUNCTION(BlueprintPure, Category="Condition")
	FGuid GetResolvedItemID() const { return ResolvedItemID; }

	/** Re-read the item's Condition and push the contract parameters into the owner's materials.
	 *  Idempotent. Returns false while the item cannot be resolved yet (a freshly spawned
	 *  equipment actor, a client still waiting on bunches) — the retry timer keeps trying. */
	UFUNCTION(BlueprintCallable, Category="Condition")
	bool RefreshWearParameters();

	/** Last pushed Da_Wear_Intensity; 0 before any item resolves. */
	UFUNCTION(BlueprintPure, Category="Condition")
	float GetWearIntensity() const { return WearIntensity; }

	/** Last pushed Da_Wear_Seed. */
	UFUNCTION(BlueprintPure, Category="Condition")
	float GetWearSeed() const { return WearSeed; }

	/** Last pushed Da_Wear_Grade. */
	UFUNCTION(BlueprintPure, Category="Condition")
	float GetWearGrade() const { return WearGrade; }

	/** The dynamic material instances this component drives (contract-implementing slots only).
	 *  Read their parameters to see exactly what the component pushed. */
	UFUNCTION(BlueprintPure, Category="Condition")
	TArray<UMaterialInstanceDynamic*> GetWearMaterials() const;

	/** Per-instance variation value in [0,1]. Stable for the life of the instance and across
	 *  save/load, because it comes from the entry GUID (UDaInventoryComponent::GetItemSeed).
	 *  16 hash bits: 65536 distinguishable variants, and no negative values to trip a material on
	 *  (GetItemSeed is a signed hash). */
	UFUNCTION(BlueprintPure, Category="Condition")
	static float MakeWearSeed(FGuid ForItemID);

	/** Contract parameter names (see docs/ConditionMaterialContract.md). */
	static const FName WearIntensityParameterName;
	static const FName WearSeedParameterName;
	static const FName WearGradeParameterName;

	/** Does this material expose any of the three contract parameters? Public so a caller that
	 *  drives its own MIDs (ADaItemActor's dropped-pickup fallback, when the pickup class carries no
	 *  condition component) can apply the same "only touch materials that opted in" rule. */
	static bool ImplementsWearContract(const UMaterialInterface* Material);

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** An equipment actor is spawned BEFORE its equipment entry is published, and on a client the
	 *  pawn, its equipment list and the PlayerState's inventory each arrive in their own bunch —
	 *  so the first look for the item almost never succeeds. These bound how long we keep looking. */
	UPROPERTY(EditDefaultsOnly, Category="Condition", meta=(ClampMin="0.01"))
	float ResolveRetryInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category="Condition", meta=(ClampMin="0"))
	float ResolveRetryTimeout = 10.f;

private:

	void StartResolveRetry();
	void StopResolveRetry();
	void RetryResolve();

	/** Explicit ItemID, else the item whose equipment entry lists our owner in SpawnedActors. */
	FGuid ResolveItemID() const;

	/** The pawn wearing this visual: our owner's attach-parent chain first (equipment actors are
	 *  attached to the character mesh), then its Owner chain — which is the only link that exists
	 *  on the frame an equipment actor spawns, since attachment happens after FinishSpawning. */
	APawn* ResolveOwningPawn() const;

	/** Explicitly supplied inventory (SetItem) first, else the wearer's: pawn's PlayerState,
	 *  pawn, or our own owner actor. */
	UDaInventoryComponent* ResolveInventory() const;

	UDaItemDefinition* ResolveItemDefinition(FPrimaryAssetId ItemDefinitionID) const;

	/** Create, once, a MID for every owner mesh material slot whose material implements the
	 *  contract. Latches only after it has seen at least one material slot, so an actor whose mesh
	 *  is assigned after BeginPlay still gets scanned when the retry comes back round. */
	void EnsureWearMaterials();

	/** Write the three cached values into every MID this component drives. */
	void PushWearParameters();

	/** RefreshWearParameters, plus the second half of "settled": true only when an item resolved AND
	 *  at least one material slot has been seen. The retry loop's stop condition, so a resolved item
	 *  on an actor with no mesh yet keeps the search alive. */
	bool RefreshAndIsSettled();

	/** Subscribe to the resolved inventory's entry-changed broadcast, so decay and repair move the
	 *  visual. That broadcast reaches clients too: it fires from the FastArray's replication
	 *  callbacks as well as on the authority. */
	void EnsureInventoryBinding();

	UFUNCTION()
	void OnInventoryEntryChanged(const FDaInventoryEntry& Entry, int32 SlotIndex);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> WearMaterials;

	/** Inventory named by SetItem; unset means "find the wearer's". */
	TWeakObjectPtr<UDaInventoryComponent> ExplicitInventory;

	/** Inventory whose OnEntryChanged this component is bound to. */
	TWeakObjectPtr<UDaInventoryComponent> BoundInventory;

	FGuid ResolvedItemID;
	float WearIntensity = 0.f;
	float WearSeed = 0.f;
	float WearGrade = 0.f;
	bool bWearMaterialsBuilt = false;

	FTimerHandle ResolveTimerHandle;
	float ResolveElapsed = 0.f;
};
