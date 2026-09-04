# RuneSchema 0.6.1E changelog

This document summarizes the user-facing progression from the original 0.6.0 release to the 0.6.1E extended build. The implementation is based on the frozen 0.6.2 Experimental.2 source tree while retaining the requested 0.6.1E product version.

## 0.6.0

- Established the original RuneSchema loader and its core data-driven mod folders.
- Supported editing existing game content through the original asset, recipe, raw-table, spawn, string, building, blueprint, course, journal, and enum loaders.

## Stable baseline updates

- Preserved the stable loader behavior and local UE4SS build integration from the frozen 0.6.2 Experimental.2 baseline.
- Added clean packaged defaults for `config/config.json`, `mods/mods.txt`, and deterministic mod ordering.
- Added compatibility reporting and optional authoring/schema tooling without enabling experimental runtime scaling by default.

## 0.6.1E extended authoring

- Added deterministic `$Patch` support for recursive asset edits in declared mod order.
- Added `$Clone` for making genuinely new item assets from an existing `ItemData` object while assigning a new `InternalName` and canonical `PersistenceID`.
- Added `$Create` for explicitly authored runtime items.
- Added runtime alias resolution so recipes, raw-table rows, and nested properties can refer to newly cloned items.
- Expanded `/assets` reflected-property support, including names, flavor text, icons, meshes, item level and `PowerLevel`, consumable packs, scale where the native target exposes it, and the exact cooked `bSoftDeleted` field.
- Added `/players` rules for all players (`*`) and stable connection slots (`*1`, `*2`, and onward), including scale, health, stamina, movement, carry weight, resistances, attack/defense families, generic attributes, and appearance selections.
- Added named `/spawns` through per-instance `DisplayName`, plus `PowerLevel`, health and damage multipliers, additional loot rows, and reflected character/component property edits for class-specific curves and combat fields.
- Preserved the demonstrated recipe placement contract: `Table`, `Row`, and `Category`, including crafting-station and vendor table placement.
- Made recipe placement safe during early GameInstance startup by storing the rooted recipe soft path without manually constructing an unsafe weak object pointer.
- Added complete demonstration mods for ten baseline capes, four player profiles, and a named spawn showcase.
- Added the loader attribution: RuneSchema created by Snorkles; extended features by Jonesing4Space; PalSchema lineage credited separately to Okaetsu.

## Compatibility notes

- Identity changes are deliberately restricted: patching cannot mutate an existing item's identity, while clones must provide a unique identity.
- `IdentityOverride` is retained only for the narrowly required item identity-map registration/read-back path.
- Unrelated later-version quest, event, NPC, dialogue, replacement-scheduler, and global-registry work is excluded.
- Runtime game testing is still required for persistence, UI refresh, every player attribute family, and game-version-specific reflected fields.
