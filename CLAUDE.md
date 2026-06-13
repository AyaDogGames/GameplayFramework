# CLAUDE.md — GameplayFramework plugin (canonical agent guide)

This is the canonical guide for AI agents and contributors working in this repository. `AGENTS.md` is a short pointer to this file; `README.md` is the human-facing setup guide. When this file disagrees with prose elsewhere, re-open the cited source and trust the code.

## What this is

`GameplayFramework` is an **Unreal Engine 5.7** C++ plugin for building fully replicated, **data-driven Gameplay Ability System (GAS)** games. Almost everything is wired up through GameplayTags, Data Assets, and Data Tables; core classes are meant to be subclassed in C++ or Blueprints. Prefix conventions: `Da` (GameplayFramework module), `CE` (Collectibles module); `U`=UObject/component, `A`=Actor, `F`=struct, `I`=interface.

## Critical: this is a SHARED plugin (symlink, not a submodule)

- This plugin is its **own git repo** (`AyaDogGames/ModularGameplayFramework`), checked out at `C:/Source/GameplayFramework`.
- Consumer projects (GlitchShaper, CollectorsEdition, Glitchwalker) do **not** vendor or submodule it. Their `setup.sh` creates `Plugins/GameplayFramework` as a **symlink/junction** to this one shared checkout. Each consumer `.gitignore`s that path and ships an empty `.gitmodules`.
- **Editing any file under a consumer's `Plugins/GameplayFramework` edits this one shared repo and affects every consumer.** Always commit plugin changes from `C:/Source/GameplayFramework` directly — never through a consumer's `Plugins/GameplayFramework` path.

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
- **`Inventory/`** — the **new `FFastArraySerializer` system** (five files in each of Public/Private): `UDaInventoryComponent` (server-authoritative; replicates a `FDaInventoryList`, `MaxSlots=20`, `InventoryTags`; `AddItem`/`RemoveItem`/`MoveItem`/`Save`/`Load`, static `GetInventoryFromActor`; `OnEntryAdded`/`Removed`/`Changed` delegates; `Server_AddItem`/`RemoveItem`/`MoveItem` RPCs + `Internal_AddItem`), `UDaMasterInventory` (empty Blueprintable subclass), `FDaInventoryEntry : FFastArraySerializerItem`, `FDaInventoryList : FFastArraySerializer`, `UDaItemDefinition : UPrimaryDataAsset` (`GetPrimaryAssetId()` -> type `"ItemDefinition"`), `IDaInventoryItemInterface` (`GetItemDefinitionID`/`GetStackCount`/`AddToInventory`). The old UObject-based inventory (item base classes, factory, equipment component, blueprint library, UI widget/controller) was **deleted** — do not document it.
- **`UI/`** — CommonUI + MVVM. Widget controllers `UDaWidgetController` -> `UDaOverlayWidgetController` -> `UDaOverlayWidgetController_Vitals`, plus `UDaStatMenuWidgetController`. CommonUI widgets `UDaActivatableWidget`, `UDaUserWidgetBase`, `UDaOverlayWidgetBase`, `UDaGameMenuBase`, `UDaPanelWidget`, `UDaPrimaryGameLayout`, `UDaTaggedWidget`. UUserWidget-based `UDaWorldUserWidget`, `UDaDamageWidget`, `UDaMessageWidget`. `ADaHUD`, `UDaCommonUIExtensions`; data assets `UDaUILevelData`, `UDaWidgetMessageData`. The only `UMVVMViewModelBase` in the plugin is the Collectibles `UCECollectibleViewModel` — there is **no** inventory ViewModel.
- **Characters / game framework (`Public/` root)** — `ADaCharacterBase` (+ `IDaCharacterInterface`), `ADaCharacter`, `ADaPlayerCharacter_ThirdPerson`; `ADaPlayerState` (defines `FPlayerCharacterInfoRow : FTableRowBase`), `ADaPlayerController`, `ADaPlayerController_TopDown`; `ADaGameModeBase`, `ADaGameStateBase`, `UDaGameInstanceBase`; `UDaPawnData`; `UDaAttributeComponent` (vitals/Death); `UDaCommonGameViewportClient`.
- **Input** — `UDaInputComponent : UEnhancedInputComponent` (set as the project's Default Input Component Class); `UDaInputConfig : UDataAsset` (Input tag -> InputAction map).
- **Interaction / inspection** — `IDaInteractableInterface` + `UDaInteractionComponent`; `IDaInspectableInterface` + `UDaInspectableComponent` + `ADaInspectableItem`.
- **Save** — `UDaSaveGame : USaveGame`, `UDaSaveGameSubsystem : UGameInstanceSubsystem`, `UDaSaveGameSettings : UDeveloperSettings`, `IDaSaveInterface`.
- **Spawning** — `UDaActorSpawnManager` (base) with `UDaAISpawnManager` and `UDaPickupItemSpawnManager` (data-asset-driven, server, activated via game mode).
- **Quest / state / checkpoint** — `UDaQuestComponent`, `UDaQuest`/`UDaQuestWithResult`, `FQuestInProgress`; state machine `UDaState`/`UDaBranch`/`UDaInputAtom`, `FStateMachineResult`; `ADaCheckpoint : APlayerStart`.
- **Items / pickups / projectiles** — `ADaItemActor` (implements `IDaInteractableInterface`, `IAbilitySystemInterface`, `IDaInventoryItemInterface`), `ADaItemChest`, `ADaEffectActor`; `ADaPickupItem`, `ADaPickup_Ability`; `ADaProjectile`, `ADaProjectile_Magic`, `ADaTeleportProjectile`; `UDaRenderUtilLibrary`.
- **Collectibles module (`Source/Collectibles/`, prefix `CE`)** — `ACECollectibleActorBase` (bridges into inventory via `IDaInventoryItemInterface`), `UCECollectibleData`, `ICECollectibleItemInterface`, `UCECollectibleProxy`, `ACECollectibleSpawnLocation`, `UCECollectibleViewModel : UMVVMViewModelBase`, `UCEItemCoinAttributeSet : UDaBaseAttributeSet`, `UDaAppraisalWidget : UDaPanelWidget`, plus `DataObjects/` (`UCEEditionTemplateData`, `UCEWearTemplateData`).

## Gameplay tags (native)

Declared with `DA_DECLARE_GAMEPLAY_TAG_EXTERN` / `CE_DECLARE_GAMEPLAY_TAG_EXTERN` and defined with `UE_DEFINE_GAMEPLAY_TAG[_COMMENT]`.

- **Core** (`CoreGameplayTags.{h,cpp}`, namespace `CoreGameplayTags`): roots include `Input`, `Character`, `AbilitySet`, `Attributes`, `Inventory`, `InventoryItem`, `Message`, `UI`, `Status`, `Action`, `Pickup`, `Level`, `Ability`, `Event`, `Gameplay`, `Cheat`.
- **Collectibles** (`CollectiblesGameplayTags.{h,cpp}`, namespace `CollectiblesGameplayTags`): `Collectibles.*` plus extensions of the core trees (`Inventory.Type.Collectibles`, `Attributes.Stats.Collectible*`).

## Conventions

- **Data-driven GAS.** New attributes: subclass `UDaBaseAttributeSet`, set a parent `SetIdentifierTag`, and register attributes in the constructor via `TagsToAttributes.Add(Tag, GetXAttribute)`. `Attributes.Stats.Primary` are set directly; `.Secondary` are derived via gameplay effects.
- **Tag-driven UI (MVC/MVVM).** AttributeSet (model) -> WidgetController (listens/broadcasts) -> UserWidget (view), wired by gameplay tag. Built on CommonUI + ModelViewViewModel.
- **Replication.** Inventory and attributes are server-authoritative; inventory uses `FFastArraySerializer` delta replication. Mutate inventory via the component's BlueprintCallable API (which routes through `Server_*` RPCs), not by editing entries directly.
- **Asset Manager.** `UDaItemDefinition::GetPrimaryAssetId()` returns the `"ItemDefinition"` type, but the plugin does **not** register it at startup and ships no `Config/`. Consumers must add `ItemDefinition` (and any other primary data-asset types they use) to `PrimaryAssetTypesToScan` in their own `DefaultGame.ini`.

## Build / run

1. The plugin compiles as part of a consuming UE 5.7 project — there is no standalone build.
2. Run the project's **GenerateProjectFiles** so both modules are detected, then build the `.sln` (Windows) / project from your IDE or the editor.
3. Engine reference for API checks: `C:/Source/UnrealEngine` (UE 5.7.4, release).

## No automated tests

There is no test module, test target, or CI test runner in this repository. Do not claim or invent one.

## Pointers

- `README.md` — human setup guide.
- `GameplayFrameworkEditorToolingPlan.md` — **aspirational** editor-tooling proposal (not implemented).
- `specs/2026-03-11-inventory-refactor.md` — the FastArray inventory plan. The shipped code (above) is authoritative where it differs (e.g. RPC names are `Server_AddItem`/`RemoveItem`/`MoveItem`; there is no `Server_RequestStackItems`, no `IDaInventoryChangeListener`, no `UDaInventoryViewModel`).
