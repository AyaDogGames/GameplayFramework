# GameplayFramework

The gameplay framework is an Unreal Engine 5.7 C++ plugin that simplifies creating a fully replicated data-driven Gameplay Ability System (GAS) based game rapidly. Classes can be overridden in C++ or Blueprints but often will expect Data Tables or Data Assets to set them up.

> Agents: see `CLAUDE.md` for the canonical contributor/agent guide (module map, conventions, the shared-symlink gotcha, build steps). This README is the human-facing setup guide.

## Overview

The plugin utilizes the Gameplay Ability System (GAS) and while originally influenced by the Epic Games Lyra example; it is different. It combines several concepts such as data driven ability set creation and provides an optional attribute component that is responsible for handling vital attributes like Health and Death of an actor, but any attributes can be created in game as long as they are vended using the parent gameplay tags provided in the plugin. For example, one could use `Attributes.Stats.Primary` and `Attributes.Stats.Secondary` and subclass `DaBaseAttributeSet` to create a set of Attributes for their specific game. The base class would need a parent `SetIdentifierTag` (in this case `Attributes.Stats`), and would need to associate all Attributes with gameplay tags in the constructor like so: `TagsToAttributes.Add(YourGameplayTags::AttributesStatsPrimaryStrength, GetStrengthAttribute);`

UI Base classes that handle and vend attribute updates are also provided. A `WidgetController` subclass is associated with an attribute set tag and looks for all attributes in the set and provides updates whenever their values change using multicast delegate methods.

It provides core base character classes and player state set up with an ability system component and an attribute component, and subclasses for players and NPCs. Characters are granted attribute sets, gameplay effects, and gameplay abilities set up with AbilitySets defined as DataAssets.

For player stats or attributes (strength, intelligence, armor, etc), define a subclass of `DaBaseAttributeSet`, define each attribute using the parent GameplayTag `Attributes.Stats`, and set up event handlers in blueprints. `Attributes.Stats.Primary` are meant to be set directly while those with `Attributes.Stats.Secondary` are derived from Primary (through a gameplay effect).

The UI is based on an event-driven MVC/MVVM design pattern with AttributeSet subclasses (model), WidgetControllers listening and broadcasting changes to UserWidgets (controller->view), all driven via GameplayTags. UI is built on **CommonUI** and the **ModelViewViewModel** plugin.

The game using this plugin is responsible for creating its own `DaBaseAttributeSet` subclass for things like Stats and a `DaAttributeInfo` DataAsset to define associated UI (images and text for each widget type) and connects attributes to widgets via their gameplay tag.

Some Gameplay effects are required by default (and are available in the plugin's content folder) as they are responsible for important things like handling death or setting an attribute set's default values.

There is a GAS-based projectile system, an item pickup system to create abilities on the fly, and a data-driven Enhanced Input system that can be set up using GAS abilities or directly in a player character or controller.

Replicated Actors can be continuously spawned on the server via the game mode by setting a Spawner subclass in its blueprint, which spawns actors defined via data asset. There is an item spawner (`UDaPickupItemSpawnManager`) and an AI character spawner (`UDaAISpawnManager`), both subclasses of `UDaActorSpawnManager`, which can be activated via the game mode.

The AI characters can run around, find a `TargetActor` using pawn sensing and try to attack it (via gameplay ability), and run away and heal (via gameplay ability) if damaged.

### Inventory (FFastArraySerializer)

Inventory is a replicated, server-authoritative system built on Unreal's `FFastArraySerializer` for efficient delta replication:

- `UDaInventoryComponent : UActorComponent` — attach to any actor. Server-authoritative, replicates a `FDaInventoryList`, a `MaxSlots` (default 20), and `InventoryTags`. BlueprintCallable API includes `AddItem(FPrimaryAssetId, StackCount=1, SlotHint=-1)`, `RemoveItem(SlotIndex, Count=0)`, `MoveItem(From, To)`, `GetAllEntries`, `GetEntryAtSlot`, `SaveInventory`/`LoadInventory`, and the static `GetInventoryFromActor`. It broadcasts `OnEntryAdded` / `OnEntryRemoved` / `OnEntryChanged` delegates for decoupled UI. `UDaMasterInventory` is a Blueprintable subclass variant.
- `FDaInventoryEntry : FFastArraySerializerItem` — the replicated per-item instance (`ItemID`, `ItemDefinitionID`, `SlotIndex`, `StackCount`, `MaxStackCount`, `Tags`, `AbilitySetID`); auto-stacks when `MaxStackCount > 1`.
- `UDaItemDefinition : UPrimaryDataAsset` — designer-authored item template (display name/description, icon, mesh, tags, max stack count, an `AbilitySet` to grant, equip-slot tags). Its `GetPrimaryAssetId()` returns the `"ItemDefinition"` primary asset type.
- `IDaInventoryItemInterface` — implement on world actors to make them addable to an inventory; exposes `GetItemDefinitionID()`, `GetStackCount()`, and `AddToInventory(InstigatorPawn, bDestroyActor)`. Implemented by `ADaItemActor` and the Collectibles `ACECollectibleActorBase` / `UCECollectibleProxy`.

Because item definitions resolve via the Asset Manager, consumers must add `ItemDefinition` to `PrimaryAssetTypesToScan` in their project's `DefaultGame.ini` (the plugin ships no `Config/`). See **Setup** step 10.

## Setup

1. Clone the plugin to its own location (e.g. `C:/Source/GameplayFramework`). This plugin is its **own git repository** (`AyaDogGames/ModularGameplayFramework`). In the consumer projects in this workspace it is **not** added as a git submodule — instead each consumer's `setup.sh` creates `Plugins/GameplayFramework` as a **symlink/junction** to the one shared checkout (see **Shared plugin (symlink) model** below). For a standalone project you may instead copy/clone the plugin directly into `YourProject/Plugins/GameplayFramework`.
2. Enable the plugin in the project editor Plugins panel, or in C++ add `"GameplayFramework"` (and `"Collectibles"` if needed) to the public/private dependency module names in your project's `Build.cs`.
3. Create subclasses in either C++ or Blueprints for the core classes: `DaGameStateBase`, `DaGameModeBase`, `DaCharacter` (or `DaPlayerCharacter_ThirdPerson`), `DaPlayerState`, `DaPlayerController`, `DaHUD`.
4. Set your game mode subclass to use those, and set it as the map's game mode or default game mode in settings.
5. Create a `DaAbilitySet` Data Asset where you assign GameplayAbilities to Input Tags, set up any non-default Attribute Sets (`DaPlayerState` and the AI character both create a default Character set for Health and Mana and a combat set to handle damage and healing), and GameplayEffects. Create a `DaPawnData` Data Asset, set the AbilitySet you created on it, then make a DataTable of `FPlayerCharacterInfoRow` and set the pawn data asset there.
6. Set the DataTable on the PlayerState as well as a GameplayEffect to reset Health. There is one pre-made in the plugin's Content folder that can be used for this.
7. In Project Settings -> Input, set the Default Input Component Class to `DaInputComponent`.
8. Create your Enhanced Input InputActions and Mapping Contexts, then create a `DaInputConfig` DataAsset and map InputTags to InputActions as needed. Provide this asset to your `DaPlayerController` subclass to dynamically connect input to GameplayAbilities defined in the character ability set.
9. HUD: Create an OverlayWidgetController and Overlay UserWidget class and set them on the HUD's properties.
10. In Project Settings -> Game -> Asset Manager, add entries to the **Primary Asset Types to Scan** array for the data-asset types your game uses. The plugin defines exactly one primary asset type string in code: `ItemDefinition` (via `UDaItemDefinition::GetPrimaryAssetId()`). If you use the inventory system you must add:
    - PrimaryAssetType: `ItemDefinition`, AssetBaseClass: `DaItemDefinition`, the directory where these data assets live, rules set to always cook.

    For the other plugin DataAsset classes you load through the Asset Manager (e.g. `DaAbilitySet`, `DaPawnData`, AI pawn data) add similar entries (AssetBaseClass + scan directory, always cook). The PrimaryAssetType *name* for those is whatever you choose in your project config — the plugin does not register or mandate one.
11. The Developer Settings tab (`DaSaveGameSettings`) can change the SaveGame slot name.

## Shared plugin (symlink) model

Within this workspace the plugin is shared by multiple consumer projects via a symlink rather than a copy or a submodule:

- The plugin lives in its own repo at `C:/Source/GameplayFramework` (`AyaDogGames/ModularGameplayFramework`).
- Each consumer's `setup.sh` runs `ln -s "$SHARED_GAMEPLAY_FRAMEWORK" "$REPO/Plugins/GameplayFramework"` (default source `/c/Source/GameplayFramework`, overridable via the `SHARED_GAMEPLAY_FRAMEWORK` env var). On Windows this is a directory symlink/junction.
- **Editing the plugin from any consumer's `Plugins/GameplayFramework` path edits the one shared repo and affects every consumer.** Always commit plugin changes from `C:/Source/GameplayFramework` directly.
- Consumer repos `.gitignore` the `Plugins/GameplayFramework` path (and ship an empty `.gitmodules`), so the plugin is an untracked symlinked checkout there — **not** a git submodule.

## Building the Plugin

After adding the plugin to your Unreal Engine 5.7 project:

1. Run Unreal's **GenerateProjectFiles** script for your project so the modules are detected by your IDE.
2. Open the generated project files (for example the `.sln` on Windows) and build the project. The `GameplayFramework` and `Collectibles` modules compile with the rest of your project.

The repository does not contain automated tests.

## Configuration

There are many ways to configure a project built on the framework: UI, AI, camera modes, gameplay abilities, effects, attribute sets, inventory, etc. Most configuration is driven through Data Assets and Data Tables wired up by gameplay tags, plus the Asset Manager entries from **Setup** step 10. The Developer Settings tab exposes save-game options.

## Debug

### CVars

Typing `da.` in UE's editor console reveals the framework's debug CVars (toggles and visualizers exposed by the plugin's subsystems).
