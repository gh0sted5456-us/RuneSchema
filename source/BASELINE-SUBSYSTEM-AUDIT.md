# Subsystem baseline

Compared on **2026-09-22**:

- RuneSchema 0.6.1E source archive
- Okaetsu/PalSchema `main`
- RuneSchema 0.7.5.1 dual-storefront source

This page records design boundaries. It is not an authoring specification.

## Save handling

### RuneSchema 0.6.1E

0.6.1E performed a synchronous save sweep during data-registrar startup.

It:

- enumerated item and recipe registries;
- parsed every character JSON file;
- removed unresolved inventory, loadout, progress, and recipe records.

That approach is unsafe when the registry is incomplete. It also blocks startup.

### PalSchema

PalSchema does not perform the same startup disk sweep. Compatibility work is
attached to the live save-validation paths instead.

The trade-off is native-hook maintenance: those paths are game-build specific.

### RuneSchema 0.7.5.1

0.7.5.1 moved cleanup out of the startup path.

Rules:

- no character-save work during front-end initialization;
- no automatic character-save rewrite during startup or world entry;
- remove only identities with explicit RuneSchema ownership;
- preserve vanilla, third-party, malformed, duplicate, and unresolved records
  when ownership is not proven;
- preview cleanup before modifying a source save.

Supported ownership includes RuneSchema quests, dialogue state, owned quest
locations, and journal/lore entries. An unresolved item or recipe ID is not
enough to make it removable.

## Object lookup

A slow 0.7.5.1 compatibility path repeatedly scanned the full UObject array.

Current rule:

- Steam/GOG uses validated native signatures when available;
- UE4SS object hash tables are the next fallback;
- a full object scan is last resort;
- Game Pass uses its own UE4SS object-discovery path.

The Game Pass package includes a `StaticConstructObject.lua` override. It does
not provide a RuneSchema `GetObjectsOfClass` signature.

## Loader lifecycle

Keep the lifecycle small:

1. Check readiness.
2. Initialize once.
3. Read only the loader's own folder.
4. Isolate failure to that loader, mod, or section.

Core should own shared infrastructure:

- native capability resolution;
- readiness gating;
- deterministic mod order;
- DataTable and registry bridges;
- networking/replication;
- degradation reporting.

Keep these out of the startup critical path:

- save-file enumeration;
- discovery scans;
- optional UI snapshots;
- broad validation of unrelated content.

## Baseline rule

A subsystem should resolve only the capability it owns, load only its own data,
and fail only its own section.

Do not mutate unrelated state because an object is temporarily unresolved.
Cross-loader passes need a measured startup cost and a real dependency.
