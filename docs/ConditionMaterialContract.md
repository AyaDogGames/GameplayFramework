# Condition material contract

How an item's **Condition** (see `FDaConditionConfig`) becomes something a player can see.

The framework does not ship wear materials — wear looks completely different in a grimy dungeon
crawler and in a glitch-corruption roguelike, and that choice belongs to the game. What the
framework ships is a **parameter contract** plus the component that pushes it
(`UDaConditionComponent`, `Source/GameplayFramework/{Public,Private}/Equipment/DaConditionComponent.*`).
Author any material you like; implement the parameters you care about and the wear drives itself.

## The parameters

All three are **scalar** material parameters. Names are exact and case-sensitive.

| Parameter | Range | Value pushed | Meaning |
| --- | --- | --- | --- |
| `Da_Wear_Intensity` | 0 → 1 | `1 - Condition / Cap` (clamped) | 0 = factory fresh, 1 = one use from broken. The parameter to drive if you only implement one. |
| `Da_Wear_Seed` | 0 → 1 | low 16 bits of `UDaInventoryComponent::GetItemSeed(ItemID)`, divided by 65535 | Per-instance variation. Constant for the life of the item instance and across save/load and drop→re-pickup, because it is derived from the entry GUID. Two swords of the same type wear in different places. |
| `Da_Wear_Grade` | 0 → 1 | `Grade / 10` | Provenance, not wear. Grade never changes. Use it for a quality tint or to bias how much wear shows. |

`Cap` is `FDaConditionConfig::GetConditionCap(Grade)` = `CapBase + CapPerGrade * Grade` — so
intensity is relative to what *this* instance's condition ceiling is, not to a global maximum.

**A material implementing none of the three is left completely alone** (the component checks
before it creates anything), so the component is safe to put on an actor whose materials know
nothing about wear. Implementing a subset is fine and normal: intensity-only is the cheap version.

Items whose definition has `bUsesCondition = false` are pushed `Da_Wear_Intensity = 0` — they
never wear — but they still receive seed and grade, so a rack of identical items is not identical.

## Wiring a visual actor

Add `UDaConditionComponent` to the item's visual actor: the equipment actor in the definition's
`ActorsToSpawn`, the pickup actor, a shop display. It creates a
`UMaterialInstanceDynamic` per contract-implementing material slot on the owner's mesh components
and pushes the three values.

Which item a visual represents:

- **Equipment actors: nothing to configure.** The component asks the wearer's
  `UDaEquipmentManagerComponent` (via `FindItemIDForSpawnedActor`) which entry spawned its owner.
  It finds the wearer through the owner's attach parent, and — on the frame the actor spawns,
  before it is attached — through its `Owner`.
- **Visuals nobody equips** (loot rack, repair-shop preview, a pickup you want to look battered
  before it is picked up): call `SetItem(Inventory, ItemID)`. There is no wearer to find an
  inventory through, so name both halves. `SetItemID(ItemID)` alone works when the visual *is*
  hanging off the pawn holding the item.

Resolution is retried on a timer (`ResolveRetryInterval` / `ResolveRetryTimeout`) because an
equipment actor spawns before its equipment entry is published, and on a client the pawn, the
equipment list and the PlayerState's inventory all arrive in separate bunches. A component that
never resolves logs a warning when it gives up rather than sitting silently pristine.

After that, the component refreshes itself from the inventory's `OnEntryChanged` broadcast, which
fires on the authority *and* from the FastArray's client replication callbacks — so a client sees
its own sword darken as it swings, with no RPCs of its own. The component replicates nothing: both
ends derive the same numbers from the same replicated entry.

## Reading back what was pushed

`GetWearIntensity()` / `GetWearSeed()` / `GetWearGrade()` return the last pushed values, and
`GetWearMaterials()` hands back the MIDs themselves (so a test can read
`GetScalarParameterValue("Da_Wear_Intensity")` off the real material rather than trusting a
mirror). `GetResolvedItemID()` says which item the component decided it belongs to.

## Worked example

GlitchShaper's pilot sword: `Tools/editor/create_wear_material.py` authors
`/Game/GlitchShaper/Materials/M_Wear` (base colour lerped toward a dark oxidised tone and
roughness lifted by `Da_Wear_Intensity`, blotched by `Da_Wear_Seed`, brightness biased by
`Da_Wear_Grade`), assigns it to `BP_SwordEquipActor`'s mesh and adds `UDaConditionComponent` to
that Blueprint. `Tools/smoke/ue_condition_visual_smoke.py` asserts the pushed values.
