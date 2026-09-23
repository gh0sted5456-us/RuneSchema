# RuneSchema 0.7.5.13

- Adds a provider-based native-binding subsystem with source provenance and executable-memory validation.
- Exposes validated, allow-listed bindings to future native plugins through the append-only `ResolveBinding` host function and `runeschema.bindings` discovery service.
- Resolves `UDataTable::Serialize(FArchive&)` through UE4SS versioned vtable metadata on Game Pass, while retaining the validated embedded AOB on Steam/GOG.
- Retries the Game Pass vtable provider after Unreal readiness when the DataTable class was unavailable during preload.
- Treats a missing early DataTable hook as feature-scoped degradation; GameInstance core startup continues.
- Creates the optional `mods` directory before generating `runeschema.txt` and reports the exact path when it is not writable.
- Restores the compact 0.6-style direct character-JSON cleanup, constrained to RuneSchema ownership: the previous active identity snapshot is compared with the current active definitions, missing identities are removed before character deserialization, and the snapshot is overwritten with the current set.
- Cleanup covers inventory, personal inventory, direct and indexed loadout entries, item/recipe progress, and owned quest/journal state. Reinstalling a removed mod is a fresh install; deleted save records are never restored.
- Preserves vanilla, unknown third-party, and current active identities. Every changed character JSON receives a sibling `runeschema-before-clean` backup and an atomic, read-back-verified replacement.
- Ships an empty `mods` directory without `runeschema.txt` or example content, so a drag-and-drop update cannot overwrite an existing mod list.

- Adds a strict, capability-scoped registry patch layer behind `/raw` without removing the legacy format.
- Adds owned DataTable row creation, deterministic identities, dependency ordering, transaction preparation, reflected layout checks, and unambiguous full-path/short-name resolution.
- Adds the first character-customization adapter for deterministic hair-zone and hair-preset registration from cooked mod assets.
- Rejects duplicate JSON keys, oversized definitions, unknown fields, destructive operations, ownership violations, ambiguous targets, and changed reflected layouts with isolated diagnostics.
- Adds formal authoring schemas, examples, and a release-gated registry patch contract.
- Keeps plug-in versions independent from the RuneSchema core version.
- Hardens universal storefront detection with WinGDK/Win64 layout, Windows package identity, and the Game Pass `UE4SS_Signatures` fallback; the selected evidence is printed in standard logs.
- Adds optional `.usmap` discovery, one-time fingerprinting, server/client identity diagnostics, and a bounded lazy query/cache service while retaining live reflection as the authority.
- Makes plug-ins explicitly optional. Missing or mismatched native DLLs, versions, and dependencies are isolated; RuneSchema core, other plug-ins, and complete plug-in PAK containers continue.
- Adds a plugin-free Core runtime ZIP alongside the Universal runtime as a release proof of the optional-plugin contract.
- Preserves Helpy's instant-open embedded catalogue, bounded pagination, lazy icon work, push-driven updates, and manual full scan; no external menu framework is required.
- Adds an authoring guide, walkthroughs for all 22 loader folders, and a complete API 1 reference covering every export, host function, result code, lifecycle phase, and core service.
- Shortens runtime diagnostics while retaining loader, mod, and service scope tags.
