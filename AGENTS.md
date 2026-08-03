# Agent Guidelines — GameplayFramework

This repository is the **GameplayFramework** plugin for **Unreal Engine 5.8**: a data-driven, replicated Gameplay Ability System (GAS) framework with two Runtime modules (`GameplayFramework`, `Collectibles`) and no editor module. There is no automated test suite.

**Canonical guide: see [`CLAUDE.md`](./CLAUDE.md).** It is the authoritative agent/contributor reference — module map, conventions, the shared-checkout gotcha, the FastArray inventory, build steps, and what does not exist. This file is only a pointer; do not duplicate that content here.

For human-facing setup and integration instructions, see [`README.md`](./README.md).

Two things to internalize before editing:

- This plugin is **one shared checkout** at `C:/Source/SharedPlugins/GameplayFramework` (not a submodule, not a symlink). Consumers discover it via `"AdditionalPluginDirectories": ["../SharedPlugins"]` in their `.uproject`; editing it affects every consumer. Commit plugin changes in this repo directly.
- Engine is **UE 5.8**; inventory is the `FFastArraySerializer` system with a thin view-model UI adapter layer (the old UObject inventory model was deleted); there is no `GameplayFrameworkEditor` module. See `CLAUDE.md` for details.
