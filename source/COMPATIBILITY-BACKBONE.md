# Compatibility

RuneSchema uses one runtime with separate storefront lanes.

## Startup

1. **Detect storefront.**
   - `WinGDK`, Windows package identity, or the Xbox
     `UE4SS_Signatures/StaticConstructObject.lua` file selects Game Pass.
   - `Win64`, Steam, or GOG selects the desktop lane.

2. **Select native behavior.**
   - Steam/GOG may use validated embedded signatures.
   - Game Pass disables Steam-only scans and prefers UE4SS metadata, reflection,
     and its own validated WinGDK contracts.

3. **Select mappings.**
   - Preferred: `Mods/RuneSchema/dlls/mappings`
   - Compatibility paths: UE4SS root, `ue4ss/mappings`,
     `Mods/RuneSchema/mappings`, and `Mods/RuneSchema/shared`
   - Exact `Mappings.usmap` names win. Otherwise the newest map is selected.

4. **Start core services.**

5. **Load optional plugins.**
   - Semantic-version differences are notices.
   - ABI incompatibility can block one native DLL, not the whole runtime.
   - Complete plugin PAK triplets can still mount independently.

## Game Pass DataTable serialization

For `UDataTable::Serialize(FArchive&)`, Game Pass resolves the live vtable slot
from UE4SS's versioned `UObject` layout metadata.

RuneSchema checks that the target points to executable memory. If Unreal is not
ready during preload, resolution is retried later.

The installed `UE4SS_Signatures` folder remains owned by UE4SS and is reported
at startup.

## Mapping service

`runeschema.mapping` parses the selected uncompressed UE4SS map only when a
plugin or diagnostic asks for a type query.

Parsing is bounded by file size, count, time, and cache limits.

Server/client mapping fingerprints are diagnostic only. A mismatch does not
reject a connection. Live reflection remains authoritative for mutation.

## Native bindings

`main.dll` owns native resolution.

It records provenance and exposes only allow-listed RuneSchema bindings.

- Steam/GOG may use a validated AOB.
- Game Pass prefers UE4SS metadata or reflection when a Steam pattern is not
  portable.
- Missing bindings disable only the dependent hook or feature.

Native plugins can check `RuneSchemaHostApi::StructSize` and use the append-only
`ResolveBinding` host function.

The `runeschema.bindings` service reports capability and source without
exposing raw addresses through JSON.

## Journal hierarchy

Journal placement is storefront-specific.

- Steam/GOG uses its validated insert, category-dispatch, and builder contracts.
- Game Pass uses its validated hierarchy insert, three category entry points,
  and builder-layout witness.

Patterns must resolve uniquely in executable memory. If validation fails,
hierarchy placement is disabled for that lane; unrelated journal data and
loaders continue.

## Loader rules

- Each loader owns only its named folder.
- Failures are isolated by mod and section.
- Effects and Niagara load before their consumers.
- `/raw` uses explicit ownership, dependency order, preconditions, and
  transactional commits.
- `/registry` is the multiplayer authority/presentation bridge. It does not
  replace the other loaders.
- Live reflection validates objects and fields before mutation.
- Core and vanilla behavior do not depend on Helpy or Networking.

## Plugin behavior

`BuiltForRuneSchema` and semantic versions are informational.

RuneSchema attempts best-effort startup. A native ABI mismatch skips that DLL
because it cannot be called safely. Other components continue.

Helpy uses an instant-open model:

- embedded first page;
- bounded pagination;
- no automatic full scan;
- push updates;
- lazy icons;
- manual refresh/full scan.
