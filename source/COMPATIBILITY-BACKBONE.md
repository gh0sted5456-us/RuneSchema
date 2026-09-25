# RuneSchema compatibility backbone

## Startup flow

1. Detect the storefront from the executable layout. `WinGDK`, Windows package identity, and finally the Xbox `UE4SS_Signatures/StaticConstructObject.lua` file identify Game Pass. `Win64`, Steam, or GOG identify the desktop build.
2. Game Pass disables RuneSchema's Steam-only native scans and uses UE4SS's public reflection/object APIs. For `UDataTable::Serialize(FArchive&)`, RuneSchema resolves the live DataTable vtable slot from UE4SS's versioned `UObject` layout metadata, validates that the target is executable memory, and retries once Unreal is ready if the class was unavailable during preload. The installed `UE4SS_Signatures` folder remains UE4SS's responsibility and is reported visibly at startup.
3. Prefer `.usmap` data from `Mods/RuneSchema/dlls/mappings`, with compatibility reads from the UE4SS root, `ue4ss/mappings`, `Mods/RuneSchema/mappings`, and `Mods/RuneSchema/shared`. Exact `Mappings.usmap` names take priority; otherwise the newest map is selected. Startup fingerprints it but does not parse it.
4. Load RuneSchema core services, then discover optional plugins. Plugin semantic-version differences are notices, not load gates.
5. Mount complete plugin PAK triplets independently from optional plugin DLL startup. A missing DLL, incompatible ABI, missing dependency, or failed initialization is isolated to that native component.

The `runeschema.mapping` service parses the selected uncompressed UE4SS map
only when a plugin or diagnostic explicitly requests a type query. Parsing is
bounded by file, count, time, and cache limits. Server/client fingerprints are
compared for diagnostics only; a mismatch does not reject a connection. Live
reflection remains authoritative for every mutation.

## Native bindings

Native binding resolution is owned by `main.dll`, not by individual plugins.
The provider records provenance for every successful resolution and exposes
only RuneSchema's allow-listed bindings. Steam/GOG may use a validated embedded
AOB; Game Pass prefers live UE4SS metadata and reflection where a Steam address
pattern is not portable. Missing bindings disable only their dependent hook or
feature and do not abort core startup.

Native plugins can check `RuneSchemaHostApi::StructSize` and then call the
append-only `ResolveBinding` host function. The `runeschema.bindings` service
provides discovery and source/status reporting without exposing raw addresses
through JSON.

Journal hierarchy placement follows the same split. Steam/GOG uses its
validated insert, category-dispatch, and builder contracts. Game Pass uses its
own validated hierarchy insert, three independent category entry points, and
builder-layout witness. The executable-pattern resolver requires a unique match,
checks relative targets and executable sections, and disables only hierarchy
placement when the active storefront contract does not validate.

## Loader ownership and conflict rules

- Every loader owns only its named folder and isolates failures by mod and section.
- Effects and Niagara definitions run before their consumers.
- `/raw` registry patches use explicit ownership, dependency ordering, target preconditions, and transactional table commits.
- `/registry` is the multiplayer capability/presentation bridge. It does not replace `/raw`, recipes, equipment, NPC, building, or spawn definitions.
- Runtime reflection validates objects and properties before mutation. A `.usmap` accelerates compatibility analysis and cache invalidation but does not authorize an unsafe write.
- RuneSchema core, local loaders, and vanilla systems remain operational without Helpy or Networking.

## Plugin compatibility

`BuiltForRuneSchema` and manifest/DLL semantic versions are informational. RuneSchema attempts best-effort startup. A native ABI version mismatch cannot be called safely, so that DLL is skipped; its complete PAK containers remain eligible for mounting and all other components continue.

Helpy follows an instant-open model: an embedded first page, bounded pagination, no automatic full scan, push-based updates, lazy icons, and manual refresh/full scan. This mirrors the useful performance patterns found in current UE4SS menu projects without making another menu framework a dependency.
