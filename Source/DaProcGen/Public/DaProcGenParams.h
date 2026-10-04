// Copyright Dream Awake Solutions LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DaDungeonLayout.h"
#include "Engine/DataAsset.h"

#include "DaProcGenParams.generated.h"

/**
 * Shareable layout tuning: one asset several ProcGen actors (or several maps) can point at.
 *
 * The params are NOT replicated — they are config, and every machine in a session must resolve the
 * same ones or the locally generated dungeons disagree despite the seed matching. Cooked content is
 * identical everywhere, which is what makes an asset reference safe here and a runtime-mutated
 * inline struct not.
 *
 * ADaProcGenActor::GetEffectiveLayoutParams prefers this asset over the actor's inline params, so an
 * actor is tuned either by asset (shared) or in place (one-off), never half of each.
 */
UCLASS(BlueprintType)
class DAPROCGEN_API UDaProcGenParams : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Tuning handed to FDaDungeonLayout::Generate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ProcGen", meta = (ShowOnlyInnerProperties))
	FDaDungeonLayoutParams Layout;
};
