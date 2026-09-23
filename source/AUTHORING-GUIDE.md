# RuneSchema authoring guide

This guide covers the runtime layout and the path from a JSON file to a loaded
game object. See [LOADER-WALKTHROUGHS.md](LOADER-WALKTHROUGHS.md) for every
loader's input and workflow.

## Runtime layout

Install RuneSchema under the UE4SS `Mods` directory:

```text
ue4ss/
└─ Mods/
   └─ RuneSchema/
      ├─ enabled.txt
      ├─ dlls/
      │  ├─ main.dll
      │  └─ mappings/
      │     └─ Mappings.usmap       optional
      ├─ settings/
      │  ├─ settings.jsonc
      │  └─ safesave/
      │     └─ OwnedContentLedger.json  generated
      ├─ plugins/                    optional
      └─ mods/
         ├─ runeschema.txt
         └─ MyMod/
            ├─ ID.txt               optional distribution metadata
            ├─ paks/
            ├─ assets/
            ├─ recipes/
            └─ ...
```

Release ZIPs intentionally omit `RuneSchema/mods`. Create that directory when
installing content mods.

## Create a mod

1. Create `RuneSchema/mods/MyMod`.
2. Add one or more loader folders. Folder names must match the names in
   `settings/settings.jsonc`.
3. Place `.json` or `.jsonc` files directly in each loader folder.
4. Put cooked containers under
   `paks/<PackageName>/<PackageName>.pak|.ucas|.utoc`. All three files are
   required.
5. Add `MyMod : 1` to `mods/runeschema.txt`, or let RuneSchema reconcile the
   order file when that setting is enabled.
6. Install the same content mod and cooked containers on the server and every
   client for multiplayer use.

The mod directory name is the owner namespace. References may use a local ID
inside the same mod or `OtherMod:Id` for an explicit cross-mod reference.

## Ordering

RuneSchema reads mods from top to bottom in `mods/runeschema.txt`. A value of
`0` disables the mod. Files within a loader folder are read in path order, so
numeric prefixes such as `10-Items.jsonc` and `20-Recipes.jsonc` make intent
clear.

The runtime creates loaders in this order:

```text
registry → equipment → effects → niagara → enums → raw → assets → blueprints
→ recipes → npc → vendors → dialogue → quests → events → journal → lore
→ buildings → spawns → courses → strings
```

`effects` and `niagara` definitions are loaded before their consumers. Recipe
references are prepared before game-instance recipe placement. Nameplates are
resolved before player rules are finalized.

Load order does not override validation. A reference still has to resolve to
the expected Unreal type.

## JSON rules

- `.json` and `.jsonc` are accepted.
- JSONC may contain comments.
- Duplicate keys are rejected.
- Use full cooked object paths when two loaded objects can share a short name.
- Use stable IDs and persistence IDs after a public release.
- `$Patch` changes an existing supported definition.
- `$Clone` creates a new supported definition from a compatible source.
- `$Append` adds to supported arrays. It does not deduplicate entries.
- Unknown fields are rejected where a loader has a closed schema.

Not every loader supports every directive. The loader walkthrough marks the
supported operations.

## Cooked assets and `$declaration`

A cooked PAK can contain objects that RuneSchema did not create. Add a
`$declaration` in a supported loader to assign those objects to the mod's
current ownership snapshot. On the next load, RuneSchema compares that small
snapshot with the active definitions and removes the missing mod's known save
references before overwriting the snapshot. It never restores removed state.

Supported declaration areas are:

- `/assets`
- `/buildings`
- `/journal`
- `/lore`
- `/quests`

Example:

```json
{
  "$declaration": {
    "Kind": "Item",
    "Path": "/Game/MyMod/Items/ITEM_MySword.ITEM_MySword",
    "PersistenceID": "stable-id-from-the-cooked-asset"
  }
}
```

Use the kind required by the selected loader. A declaration does not clone,
patch, or load an object; it records ownership after the cooked path resolves.

## `/raw` and `/registry`

These folders solve different problems:

- `/raw` creates or patches supported DataTable rows. It is data authoring.
- `/registry` describes authority actions and client presentation for content
  that both sides already have. It is a multiplayer bridge.

Do not place DataTable patches in `/registry`, and do not use `/raw` as a
replacement for spell or presentation synchronization.

## Storefront and mappings

The same RuneSchema DLL supports both storefront layouts. Startup detection
uses the executable path, Windows package identity, and the Game Pass
`UE4SS_Signatures` directory. Steam/GOG native scans are not used on a detected
Game Pass installation.

USMAP data is optional. RuneSchema looks for `Mappings.usmap`, then other
`.usmap` files (newest first), in:

```text
ue4ss/Mods/RuneSchema/dlls/mappings/Mappings.usmap
ue4ss/Mappings.usmap
ue4ss/mappings/Mappings.usmap
ue4ss/Mods/RuneSchema/mappings/Mappings.usmap
ue4ss/Mods/RuneSchema/shared/Mappings.usmap
```

The first path is canonical. The remaining paths are compatibility reads for
existing installations.

Startup reads the selected file once to compute a stable fingerprint. It does
not parse the type table. A plugin or diagnostic tool can call
`runeschema.mapping` to describe or search types; that first query performs a
bounded lazy parse and stores a small fingerprint-keyed cache. See
`API-REFERENCE.md` for every request.

The server and client exchange their optional mapping fingerprints. A
difference is reported but is not a connection or loader gate. Mapping data is
advisory: live reflection is always checked before RuneSchema writes to an
object. Use an uncompressed map produced by UE4SS DumpUSMAP after game updates,
with Blueprint-generated types included when possible.

## Plugins

Plugins are optional. RuneSchema loads manifests from `RuneSchema/plugins`,
then uses `plugins/plugins.txt` for order and enablement.

A plugin can provide:

- a native DLL under `dll/`;
- cooked containers under `paks/<PackageName>/`;
- both; or
- neither while it is being developed.

Manifest and semantic-version differences are reported but do not block a
load attempt. An ABI mismatch cannot be called and disables that DLL. The
plugin's complete PAK containers can still mount. A missing dependency does
not disable RuneSchema core.

Set a plugin to `0` in `plugins.txt` to disable it. This also overrides the
legacy `Required` manifest field.

## Multiplayer checklist

- Install the same PAKs on server and clients.
- Keep mod folder names and IDs identical.
- Put gameplay decisions on the authority side.
- Use `/registry` for supported action and presentation synchronization.
- Use replicated or multicast cooked game paths when a client must see an
  animation, Niagara system, spell presentation, or world-state change.
- Treat client-only menus as request senders. The server validates inventory
  grants, temporary spawns, and world changes.

Without the Networking content plugin, local loaders still run. Features that
depend on its cooked bridge objects cannot synchronize through that bridge.

## Failure isolation and log tags

RuneSchema reports the failed scope:

```text
[DISABLED][LOADER:name]       loader initialization failed
[DEGRADED][LOADER:name]       part of the loader failed
[DEGRADED][LOADER:name][MOD]  one mod section failed
[PERF][LOADER:name]           a load or finalization step exceeded one second
```

One rejected file does not authorize RuneSchema to accept invalid data from
another file. Fix the first error for the named loader and mod, then restart
unless that loader explicitly supports auto-reload.

## Test sequence

1. Start with one mod and one definition.
2. Confirm the storefront line and loader announcement in `UE4SS.log`.
3. Confirm every cooked path resolves.
4. Test creation, save, reload, and removal in single player.
5. Test a listen server with one client.
6. Test a dedicated server if the feature is intended for one.
7. Remove or disable the mod and verify only its declared or RuneSchema-owned
   state is cleaned.

Do not use a production save for first-pass loader testing.
