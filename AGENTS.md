# Agent Guidelines — GameplayFramework

This repository is the **GameplayFramework** plugin for **Unreal Engine 5.7**: a data-driven, replicated Gameplay Ability System (GAS) framework with two Runtime modules (`GameplayFramework`, `Collectibles`) and no editor module. There is no automated test suite.

**Canonical guide: see [`CLAUDE.md`](./CLAUDE.md).** It is the authoritative agent/contributor reference — module map, conventions, the shared-symlink gotcha, the FastArray inventory, build steps, and what does not exist. This file is only a pointer; do not duplicate that content here.

For human-facing setup and integration instructions, see [`README.md`](./README.md).

Two things to internalize before editing:

- This plugin is a **shared symlink**, not a git submodule. It is its own repo and is symlinked into each consumer's `Plugins/GameplayFramework`; editing it affects every consumer. Commit plugin changes from `C:/Source/GameplayFramework` directly.
- Engine is **UE 5.7**; inventory is the new `FFastArraySerializer` system (the old UObject inventory was deleted); there is no `GameplayFrameworkEditor` module. See `CLAUDE.md` for details.
