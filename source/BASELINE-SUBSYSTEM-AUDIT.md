# RuneSchema subsystem baseline audit

Compared on 2026-09-22:

- RuneSchema 0.6.1E source archive (the closest complete local 0.6.x source baseline)
- Okaetsu/PalSchema `main`
- RuneSchema 0.7.5.1 dual-storefront source

## Save handling

RuneSchema 0.6.1E already performed a synchronous, registry-wide character-save sweep during data-registrar initialization. It enumerated item and recipe registries, parsed every character JSON file, and removed unresolved inventory, loadout, item-progress, and recipe-progress records. That behavior is not a safe simplicity baseline: an incomplete registry can misclassify valid data and the work blocks initialization.

PalSchema does not implement an equivalent startup disk sweep. Its item compatibility work is attached to the relevant live save-validation paths. This keeps unrelated subsystems out of save maintenance, although those native hooks are game-specific and signature-sensitive.

The corrected RuneSchema 0.7.5.1 build uses an offline, explicit cleanup workflow:

- no character-save work during front-end/core initialization;
- no live-registry enumeration in the automatic cleanup path;
- character saves are never read or rewritten automatically during startup or world entry;
- only records carrying explicit RuneSchema ownership metadata can be removed;
- inventory, personal inventory, loadout, item/recipe progress, unowned quests, malformed native rows, duplicate native rows, and third-party records are preserved;
- the Save Cleaner UI previews an owner-only export while leaving the source save untouched.

The supported ownership formats cover RuneSchema quests/dialogue, owned quest locations, and journal/lore entries. Items and recipes do not become eligible merely because their IDs fail to resolve.

The slow-loading 0.7.5.1 regression also included a compatibility wrapper that replaced Steam's native object lookup with repeated full UObject array scans. Steam now uses the validated native signatures from the fast 0.7.0/0.7.5 builds. When a native signature is unavailable, RuneSchema uses UE4SS's tested object hash tables once ready, and scans only as a final fallback. The GamePass UE4SS package supplies its own internal hash-table discovery, but the attached archive has only a `StaticConstructObject.lua` override; it does not supply a RuneSchema `GetObjectsOfClass` signature.

## Loader lifecycle

PalSchema's useful baseline is a small lifecycle contract: a loader declares when it can initialize, initializes once, and ignores folders it does not own. RuneSchema retains that contract while adding per-loader and per-mod failure isolation. The isolation is justified; it prevents one optional system or malformed mod section from disabling the framework.

Complexity that should remain centralized rather than duplicated in plugins:

- UE4SS/native capability resolution;
- lifecycle/readiness gating;
- deterministic mod order;
- data-table and registry bridges;
- networking/replication facilities;
- tagged degradation reporting.

Complexity that should not be placed in the startup critical path:

- save-file enumeration and JSON serialization;
- diagnostics and discovery scans;
- optional UI registry snapshots;
- broad validation of content not owned by the active loader.

## Follow-up audit rule

Every subsystem should be judged against this boundary: resolve only the capability it owns, load only its own folder, fail only its own section, and never mutate unrelated state based solely on temporary non-resolution. New cross-loader passes need a measured startup cost and a concrete dependency reason.
