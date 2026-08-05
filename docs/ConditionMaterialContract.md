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
| `Da_Wear_Intensity` | 0 → 1 | `clamp(1 - Condition / Cap, 0, 1)` | 0 = factory fresh, 1 = one use from broken. The parameter to drive if you only implement one. |
| `Da_Wear_Seed` | 0 → 1 | low 16 bits of `UDaInventoryComponent::GetItemSeed(ItemID)`, divided by 65535 | Per-instance variation. Constant for the life of the item instance and across save/load and drop→re-pickup, because it is derived from the entry GUID. Two swords of the same type wear in different places. ONE exception: dropping part of a STACK leaves the original entry (and its ItemID) behind in the inventory, so what hits the ground is a new instance and is minted a fresh ItemID — and therefore a fresh seed — when picked up. Whole-stack drops keep the identity. |
| `Da_Wear_Grade` | 0 → 1 | `clamp(Grade / 10, 0, 1)` | Provenance, not wear. Grade never changes. Use it for a quality tint or to bias how much wear shows. |

`Cap` is `FDaConditionConfig::GetConditionCap(Grade)` = `CapBase + CapPerGrade * clamp(Grade, 0, 10)`
— so intensity is relative to what *this* instance's condition ceiling is, not to a global maximum.

Both clamps are load-bearing, not defensive habit:

- **Intensity** is clamped because Condition can legitimately exceed the cap for a moment (a grade
  written down after a condition fill, content seeded above its own cap), and an unclamped
  `1 - Condition/Cap` would go negative and hand the material a value outside the contract.
- **A cap of 0 reads as PRISTINE, not as broken.** `Cap <= 0` means "nothing to be a percentage of",
  so the component pushes `Da_Wear_Intensity = 0` rather than dividing by zero or inventing a ruined
  look for what is really a misconfigured definition. (`UDaItemDefinition::IsDataValid` errors on
  exactly that configuration, so it should not survive to runtime.)
- **Grade** is clamped for the same reason the cap clamps it: grades outside 0–10 are content bugs,
  and the contract promises the material a value in [0,1]. Note the representable grade range is
  really 1–10 — grade is a `StatTags` count and 0 is how the tag's absence is stored — so a
  `Da_Wear_Grade` of exactly 0 means "no grade recorded", not "grade zero".

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
- **Visuals nobody equips** (loot rack, repair-shop preview): call `SetItem(Inventory, ItemID)`.
  There is no wearer to find an inventory through, so name both halves. `SetItemID(ItemID)` alone
  works when the visual *is* hanging off the pawn holding the item.
- **Visuals whose item is in NO inventory** — above all a DROPPED pickup: call
  `SetExplicitWear(Intensity, Seed, Grade)`. The moment an item hits the ground its entry has left
  the inventory, so there is nothing left for any lookup to find; the caller pushes the numbers it
  already has and the resolve retry stops instead of spending its budget and then warning. This is
  what `ADaItemActor` does with its drop snapshot (the only place those values still exist), which is
  why a sword dropped at 10% condition looks it. A hand-placed pickup takes the same path with the
  pristine reading. Note the Grade argument here is the CONTRACT value (`Grade / 10`), not 0–10.

Resolution is retried on a timer (`ResolveRetryInterval` / `ResolveRetryTimeout`) because an
equipment actor spawns before its equipment entry is published, and on a client the pawn, the
equipment list and the PlayerState's inventory all arrive in separate bunches. The retry also stays
alive while the item HAS resolved but the owner has not reported a single material slot yet, so an
actor whose mesh is assigned after `BeginPlay` still gets scanned. A component that gives up logs a
warning — naming which half failed — rather than sitting silently pristine; `SetExplicitWear` is the
way to say "there is nothing to look for" and stop it.

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

The worked example is **not** part of the plugin: it lives in the GlitchShaper consumer, at
`C:/Source/GlitchShaper/Tools/editor/create_wear_material.py`, and the paths below are that project's.
Nothing in the plugin depends on it — a different game implements the same three parameters however
its art direction wants.

That script authors `/Game/GlitchShaper/Materials/M_Wear`, which drives four channels off one mask
(`Da_Wear_Intensity`, blotched by a noise field the `Da_Wear_Seed` offsets, sampled in OBJECT-LOCAL
space so the pattern belongs to the mesh rather than sliding across it as the item moves):

| Channel | Pristine → worn |
| --- | --- |
| Base colour | clean metal → dark oxide, **and then dimmed by the same mask** (`Lerp(1.0, 0.2, mask)`) — the colour blend alone does not read as worn, because a fifth of a bright albedo is still pale |
| Roughness | 0.15 → 0.98 (polished → matte) |
| Metallic | 1.00 → 0.02 (metal → dead oxide) |
| Grade tint | `Da_Wear_Grade` biases how bright the CLEAN metal is (`Lerp(0.55, 1.2)`) — provenance, applied before any wear |

It assigns that material to `BP_SwordEquipActor` (the equipped visual) and to
`BP_ItemActor_Sword` (the ground pickup), adds `UDaConditionComponent` to both, and points
`DA_Item_Sword`'s `PickupActorClass` at the pickup Blueprint so a dropped sword actually spawns the
wear-capable actor. `Tools/smoke/ue_condition_visual_smoke.py` asserts the equipped values and
`Tools/smoke/ue_rf2_wear_drop_smoke.py` the dropped ones.
