# CLAUDE.md — GameplayFramework plugin (canonical agent guide)

This is the canonical guide for AI agents and contributors working in this repository. `AGENTS.md` is a short pointer to this file; `README.md` is the human-facing setup guide. When this file disagrees with prose elsewhere, re-open the cited source and trust the code.

## What this is

`GameplayFramework` is an **Unreal Engine 5.8** C++ plugin for building fully replicated, **data-driven Gameplay Ability System (GAS)** games. Almost everything is wired up through GameplayTags, Data Assets, and Data Tables; core classes are meant to be subclassed in C++ or Blueprints. Prefix conventions: `Da` (GameplayFramework module), `CE` (Collectibles module); `U`=UObject/component, `A`=Actor, `F`=struct, `I`=interface.

## Critical: this is a SHARED plugin (one checkout in a `SharedPlugins/` container)

- This plugin is its **own git repo** (`https://github.com/AyaDogGames/GameplayFramework.git`,
  formerly `ModularGameplayFramework` — GitHub redirects the old URL), checked out at
  `C:/Source/SharedPlugins/GameplayFramework`.
- Consumer projects (GlitchShaper, CollectorsEdition, Glitchwalker) do **not** vendor, submodule,
  or symlink it. Each consumer's `.uproject` declares
  `"AdditionalPluginDirectories": [ "../SharedPlugins" ]` — the engine scans the *children* of
  that container for `.uplugin` files. There is **no `Plugins/GameplayFramework`** inside any
  consumer repo; each consumer's `setup.sh` clones this repo into the container if missing.
- **There is exactly one copy on disk — editing it affects every consumer.** Commit plugin
  changes in this repo, on this repo's branches; after any plugin change, rebuild all consumer
  projects before committing on their side.

## Modules (both Runtime — there is NO editor module)

From `GameplayFramework.uplugin`:

- **`GameplayFramework`** — `Type: Runtime`, `LoadingPhase: Default`. The main module. Startup (`Private/GameplayFramework.cpp`) just declares the `DA_GameplayFramework` log category and logs a load message — **no asset-type or factory registration**.
- **`Collectibles`** — `Type: Runtime`, `LoadingPhase: PostDefault`. Depends on the `GameplayFramework` module. Startup logs "Collectibles submodule Loaded." (a verbatim UE-*module* log string — not a git submodule).

There is **NO `GameplayFrameworkEditor` module** and no editor target of any kind. `GameplayFrameworkEditorToolingPlan.md` is an **aspirational design proposal**; its `UDaProjectSetupWizard` / `EGameType` / factories exist only in that markdown, not in code.

### Plugin dependencies (`.uplugin`)
`GameplayAbilities`, `EnhancedInput`, `CommonUI`, `ModelViewViewModel` (all enabled).

### Build dependencies
- `GameplayFramework.Build.cs` — Public: `Core`, `CoreUObject`, `Engine`, `InputCore`, `GameplayAbilities`, `EnhancedInput`, `GameplayTasks`, `DeveloperSettings`, `GameplayTags`, `AIModule`, `CommonUI`, `CommonInput`, `ModelViewViewModel`. Private: `Slate`, `SlateCore`, `UMG`, `Niagara`, `NavigationSystem`, `NetCore`.
- `Collectibles.Build.cs` — Public: `Core`, `GameplayFramework`. Private: `CoreUObject`, `Engine`, `Slate`, `SlateCore`, `GameplayAbilities`, `GameplayTags`, `CommonUI`, `UMG`, `ModelViewViewModel`.

## Source map (`Source/`)

`Public/` holds headers; `Private/` mirrors it with implementations.

- **`AbilitySystem/`** — `UDaAbilitySystemComponent`, `UDaAbilitySystemLibrary`, `UDaAbilitySet : UPrimaryDataAsset`. `Abilities/`: `UDaGameplayAbilityBase`, `UDaGameplayAbility_BaseTriggeredInputAction`, `UDaGameplayAbility_Death`, `UDaGameplayAbility_Projectile`, and the look/move input abilities `UUDaGameplayAbility_LookAction` / `UUDaGameplayAbility_MoveAction` (the literal **double-`U`** prefix is real — in both type name and filename; do not "fix" it). `Effects/`: `UDaGameplayEffect_DealDamage`, `UDaGameplayEffect_HealToMax`. `Modifiers/`: `UDaMMC_Health`. `Tasks/`: `UDaAbilityTask_TargetDataUnderCursor`.
- **`AbilitySystem/Attributes/`** — `UDaBaseAttributeSet` (carries a `SetIdentifierTag`; subclasses map tags->attributes via `TagsToAttributes` in their constructor); `UDaCharacterAttributeSet` (Health/Mana), `UDaCombatAttributeSet` (damage/healing meta), `UDaDynamicAttributeSet`; `UDaExecution_HealWithMana`; data assets `UDaAttributeInfo`, `UDaAttributeSetDataAsset`.
- **`AI/`** — `ADaAICharacter : ADaCharacterBase`; `ADAAICharacter_NPC : ACharacter, IAbilitySystemInterface` (dialog NPC, **not** a `ADaCharacterBase`); `ADaAIController`; BT services `UDaBTService_CheckAttackRange`, `UDaBTService_CheckLowHealth`; BT tasks `UDaBTTask_DoAbilityToSelf`, `UDaBTTask_DoAbilityToTargetActor`; `UDaNPCDialogData`.
- **`Inventory/`** — the **`FFastArraySerializer` system** (nine files in each of Public/Private): `UDaInventoryComponent` (server-authoritative; replicates a `FDaInventoryList`, `MaxSlots=20`, `InventoryTags`; `AddItem`/`RemoveItem`/`MoveItem`/`UseItem`/`DropItem`/`SaveInventory`/`LoadInventory`, static `GetInventoryFromActor`; `OnEntryAdded`/`Removed`/`Changed` + `OnItemUsed`/`OnItemDropped` delegates; `Server_*` RPCs + `Internal_AddItem`; delegates broadcast on authority AND from FastArray client callbacks — mutate only via the BlueprintCallable API). `UseItem` sends an `Action.UseItem` gameplay event to the owner's ASC (OptionalObject=definition, EventMagnitude=slot) and consumes when `bConsumeOnUse`; `DropItem` spawns the definition's `PickupActorClass` (default `ADaItemActor`) in front of the pawn. `FDaInventoryEntry : FFastArraySerializerItem`, `FDaInventoryList : FFastArraySerializer`, `UDaItemDefinition : UPrimaryDataAsset` (`GetPrimaryAssetId()` -> type `"ItemDefinition"`; fields: DisplayName/Description/Icon/DisplayMesh/ItemTags/MaxStackCount/`AbilitySetToGrant`/`bConsumeOnUse`/`PickupActorClass`/`EquipSlotTags`), `IDaInventoryItemInterface` (`GetItemDefinitionID`/`GetStackCount`/`AddToInventory`), `UDaMasterInventory` (empty Blueprintable subclass). **UI adapter layer** (also under `Inventory/`, recreated as thin view-models over the FastArray — NOT replicated): `UDaInventoryItemBase` (`CreateFromEntry`), `UDaStackableInventoryItem`, `UDaInventoryUIWidget` (`IUserObjectListEntry` list-entry widget), `UDaInventoryWidgetController` (binds entry delegates, rebuilds view-model array, `UseItem`/`DropItem` pass-throughs); `UDaInventoryComponent::GetItems()` is the BlueprintPure BP-compat bridge returning those view-models — prefer the widget controller for new work. The old UObject-based inventory factory and blueprint library were **deleted and not recreated** (the equipment component was later rebuilt on the same FastArray pattern — see **`Equipment/`** below). Entries also carry **per-instance identity**: `StatTags` (`TArray<FDaTagStack>`, tag->count, mirrored into a per-entry accelerator map) reachable through `SetItemStat`/`AddItemStat` (delta form; the client sends the delta and the server does the read-modify-write via `Server_AddItemStat`)/`GetItemStat` plus `FindEntryByItemID(FGuid)` — the identity lookup the equipment side and the stat API both go through — a deterministic `GetItemSeed(ItemID)` derived from the entry GUID, and the replicated+saved **loadout** (`TArray<FDaLoadoutEntry>` of slot tag -> ItemID) behind `SetLoadoutSlot`/`GetLoadoutItemID`/`GetLoadout` — the loadout lives here, on the PlayerState's inventory, so it outlives any pawn. `ADaItemActor::Interact` adds the actor's `ItemDefinitionID` to the interactor's inventory when valid (no-op with a log warning otherwise).
- **`Equipment/`** — applied-equipment mirror of the inventory FastArray, on the **pawn**: `UDaEquipmentManagerComponent : UActorComponent` (server-authoritative, replicates a `FDaEquipmentList`; `EquipItem(FGuid, FGameplayTag SlotTag)`/`UnequipSlot(SlotTag)` route through `Server_*` RPCs to `Internal_*`, `ApplyLoadout()` re-equips everything the owner's inventory loadout names, `GetEquippedItemID`/`IsItemEquipped`/`GetItemIDForAbility`, static `GetEquipmentFromActor`, `OnEquipped`/`OnUnequipped` delegates). Equipping validates against the definition's `EquipSlotTags`, spawns and attaches the definition's `ActorsToSpawn` (class + socket + relative transform), and grants its `AbilitySetToGrant` (or the entry's `AbilitySetID` override) to the owner's ASC; unequipping destroys the actors and takes the grants back. `FDaAppliedEquipmentEntry : FFastArraySerializerItem` (ItemID/ItemDefinitionID/SlotTag/SpawnedActors + `UPROPERTY(NotReplicated) GrantedHandles`), `FDaEquipmentList : FFastArraySerializer` (client callbacks re-broadcast the delegates only — cosmetic actors are spawned on the authority and replicate to clients, so equipment actor Blueprints must have **Replicates** set). The loadout itself is NOT here: it is on the inventory component (above), which is why it survives respawn and save/load. Coherence and lifetime rules the component enforces on the authority: it binds the resolved inventory's `OnEntryRemoved` (in `BeginPlay`, retried from `ApplyLoadout` because the PlayerState arrives with possession) and unequips any slot holding an item that left the inventory, while the inventory clears loadout assignments naming that item — so a dropped or consumed item cannot stay equipped or assigned; `UnequipAll()` drains every slot and is called from `ADaCharacter::UnPossessed` (with `EndPlay` as a backstop) because the ASC lives on the PlayerState and a corpse that kept its grants would double-grant the respawned pawn's re-applied loadout; `OnUnequipped` broadcasts BEFORE teardown so host listeners see live actors and grants, matching client `PreReplicatedRemove` timing, and `OnEquipmentChanged` fires from `PostReplicatedChange` so clients hear about `SpawnedActors` references that only resolve in a later delta. Hotbar activation is `UDaGameplayAbility_QuickSlot` (file under `AbilitySystem/Abilities/`): grant one per button with InputTag `Input.Item1..4` and `QuickSlotTag` = `Equip.Slot.Item1..4`; on activation it Uses consumables and toggles equip/unequip for equippables.
- **`UI/`** — CommonUI + MVVM. Widget controllers `UDaWidgetController` -> `UDaOverlayWidgetController` -> `UDaOverlayWidgetController_Vitals`, plus `UDaStatMenuWidgetController`. CommonUI widgets `UDaActivatableWidget`, `UDaUserWidgetBase`, `UDaOverlayWidgetBase`, `UDaGameMenuBase`, `UDaPanelWidget`, `UDaPrimaryGameLayout`, `UDaTaggedWidget`. UUserWidget-based `UDaWorldUserWidget`, `UDaDamageWidget`, `UDaMessageWidget`. `ADaHUD`, `UDaCommonUIExtensions`; data assets `UDaUILevelData`, `UDaWidgetMessageData`. The only `UMVVMViewModelBase` in the plugin is the Collectibles `UCECollectibleViewModel` — there is **no** inventory ViewModel.
- **Characters / game framework (`Public/` root)** — `ADaCharacterBase` (+ `IDaCharacterInterface`), `ADaCharacter`, `ADaPlayerCharacter_ThirdPerson`; `ADaPlayerState` (defines `FPlayerCharacterInfoRow : FTableRowBase`), `ADaPlayerController`, `ADaPlayerController_TopDown`; `ADaGameModeBase`, `ADaGameStateBase`, `UDaGameInstanceBase`; `UDaPawnData`; `UDaAttributeComponent` (vitals/Death); `UDaCommonGameViewportClient`.
- **Input** — `UDaInputComponent : UEnhancedInputComponent` (set as the project's Default Input Component Class); `UDaInputConfig : UDataAsset` (Input tag -> InputAction map).
- **Interaction / inspection** — `IDaInteractableInterface` + `UDaInteractionComponent`; `IDaInspectableInterface` + `UDaInspectableComponent` + `ADaInspectableItem`.
- **Save** — `UDaSaveGame : USaveGame` (`FPlayerSaveData` carries `SavedInventory` + `SavedLoadout`), `UDaSaveGameSubsystem : UGameInstanceSubsystem`, `UDaSaveGameSettings : UDeveloperSettings`, `IDaSaveInterface`. BlueprintCallable surface for driving a round trip without a respawn: `LoadSaveGame(SlotName, SlotIndex)`, `WriteSaveGame` (null-guarded — it is callable before any save exists), and `ReloadPlayerState(APlayerState*)`, which re-applies the loaded data to a live `ADaPlayerState` and early-returns on a client (save data is applied on the authority only). `UDaGameInstanceBase::LoadSlotName`/`LoadSlotIndex` are BlueprintReadWrite because `SaveGameToSlot` needs a non-empty slot name.
- **Spawning** — `UDaActorSpawnManager` (base) with `UDaAISpawnManager` and `UDaPickupItemSpawnManager` (data-asset-driven, server, activated via game mode).
- **Quest / state / checkpoint** — `UDaQuestComponent`, `UDaQuest`/`UDaQuestWithResult`, `FQuestInProgress`; state machine `UDaState`/`UDaBranch`/`UDaInputAtom`, `FStateMachineResult`; `ADaCheckpoint : APlayerStart`.
- **Items / pickups / projectiles** — `ADaItemActor` (implements `IDaInteractableInterface`, `IAbilitySystemInterface`, `IDaInventoryItemInterface`), `ADaItemChest`, `ADaEffectActor`; `ADaPickupItem`, `ADaPickup_Ability`; `ADaProjectile`, `ADaProjectile_Magic`, `ADaTeleportProjectile`; `UDaRenderUtilLibrary`.
- **Collectibles module (`Source/Collectibles/`, prefix `CE`)** — `ACECollectibleActorBase` (bridges into inventory via `IDaInventoryItemInterface`), `UCECollectibleData`, `ICECollectibleItemInterface`, `UCECollectibleProxy`, `ACECollectibleSpawnLocation`, `UCECollectibleViewModel : UMVVMViewModelBase`, `UCEItemCoinAttributeSet : UDaBaseAttributeSet`, `UDaAppraisalWidget : UDaPanelWidget`, plus `DataObjects/` (`UCEEditionTemplateData`, `UCEWearTemplateData`).

## Gameplay tags (native)

Declared with `DA_DECLARE_GAMEPLAY_TAG_EXTERN` / `CE_DECLARE_GAMEPLAY_TAG_EXTERN` and defined with `UE_DEFINE_GAMEPLAY_TAG[_COMMENT]`.

- **Core** (`CoreGameplayTags.{h,cpp}`, namespace `CoreGameplayTags`): roots include `Input`, `Character`, `AbilitySet`, `Attributes`, `Inventory`, `InventoryItem`, `Message`, `UI`, `Status`, `Action`, `Pickup`, `Level`, `Ability`, `Event`, `Gameplay`, `Cheat`, plus the item-identity/equipment roots `Item` (`Item.Stat.Durability`, `Item.Stat.Grade`) and `Equip` (`Equip.Slot.WeaponMain`, `.WeaponOff`, `.Skill1`, `.Skill2`, `.Item1`..`.Item4` — the hotbar slots).
- **Collectibles** (`CollectiblesGameplayTags.{h,cpp}`, namespace `CollectiblesGameplayTags`): `Collectibles.*` plus extensions of the core trees (`Inventory.Type.Collectibles`, `Attributes.Stats.Collectible*`).

## Conventions

- **Data-driven GAS.** New attributes: subclass `UDaBaseAttributeSet`, set a parent `SetIdentifierTag`, and register attributes in the constructor via `TagsToAttributes.Add(Tag, GetXAttribute)`. `Attributes.Stats.Primary` are set directly; `.Secondary` are derived via gameplay effects.
- **Tag-driven UI (MVC/MVVM).** AttributeSet (model) -> WidgetController (listens/broadcasts) -> UserWidget (view), wired by gameplay tag. Built on CommonUI + ModelViewViewModel.
- **Replication.** Inventory and attributes are server-authoritative; inventory uses `FFastArraySerializer` delta replication. Mutate inventory via the component's BlueprintCallable API (which routes through `Server_*` RPCs), not by editing entries directly.
- **Asset Manager.** `UDaItemDefinition::GetPrimaryAssetId()` returns the `"ItemDefinition"` type, but the plugin does **not** register it at startup and ships no `Config/`. Consumers must add `ItemDefinition` (and any other primary data-asset types they use) to `PrimaryAssetTypesToScan` in their own `DefaultGame.ini`.

## Build / run

1. The plugin compiles as part of a consuming UE 5.8 project — there is no standalone build.
2. Run the project's **GenerateProjectFiles** so both modules are detected, then build the `.sln` (Windows) / project from your IDE or the editor.
3. Engine reference for API checks: `C:/Source/UnrealEngine` (UE 5.8.0 source, branch UE5; includes the Lyra sample at `Samples/Games/Lyra`).

## No automated tests

There is no test module, test target, or CI test runner in this repository. Do not claim or invent one.

## Pointers

- `README.md` — human setup guide.
- `GameplayFrameworkEditorToolingPlan.md` — **aspirational** editor-tooling proposal (not implemented).
- `specs/2026-03-11-inventory-refactor.md` — the FastArray inventory plan. The shipped code (above) is authoritative where it differs (e.g. RPC names are `Server_AddItem`/`RemoveItem`/`MoveItem`; there is no `Server_RequestStackItems`, no `IDaInventoryChangeListener`, no `UDaInventoryViewModel`), and the item-identity/equipment spec said equipment would be slot filtering inside the inventory component, while what shipped is a separate pawn-side `UDaEquipmentManagerComponent` with its own FastArray).
