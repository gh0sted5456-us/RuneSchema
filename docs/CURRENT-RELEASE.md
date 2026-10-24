# RuneSchema 0.8.0.1

The current release is **0.8.0.1**, published October 24, 2026.
[Download 0.8.0.1](https://github.com/gh0sted5456-us/RuneSchema/releases/download/0.8.0.1/RuneSchema-0.8.0.1.zip)
· [Package, defaults and verification](RELEASE-0.8.0.1.md)
· [Implemented and removed changes, line by line](CHANGELOG-0.8.0.1.md).
The release source is published on `main` and `experimental`, and the
`0.8.0.1` tag identifies the source used for the rebuilt DLLs. The cleaned ZIP
is locally built and verified, not a hosted CI artifact.

RuneSchema loads enabled mod folders, mounts their cooked content, and connects
supported definitions to Dragonwilds' live systems. The main release now uses
the plain version/tag `0.8.0.1`; `m`/`e` suffixes describe historical releases.
The package uses one core DLL for Steam/Epic, Game Pass and servers, plus Helpy.

[Previous release: 0.8.0.0](RELEASE-0.8.0.0.md)
· [Historical changelog](CHANGELOG-0.8.0.0.md)
· [Installation guide](INSTALLATION.md)

## What changed

This release consolidates the [implemented and retired changes](CHANGELOG-0.8.0.1.md):
the themed Helpy desktop workspace, source-aware item/icon caches,
Diagnostics and bug reporting, platform-separated AppData, mapping reuse,
native registration, backup-first cleanup and reduced observer/startup/UI work.
Both DLLs are compressed. The stock UE4SS host is not replaced.
The release notes distinguish automated verification from outstanding live
placement, progression and multiplayer acceptance tests.

Earlier foundation improvements retained in this release:

- Custom cooked wearable items can join the game's item enumeration and native
  appearance cache. When equipment changes clear a previously built cache
  entry, RuneSchema can ask the game to rebuild it again. The game still applies
  the item's authored material and hide rules; RuneSchema does not create them.
- A cooked registry declaration can name wearable mesh-data dependencies in
  `WearableMeshData`. This checks that those assets resolve; it is not a
  substitute for the item's male/female mesh-data references.
- The large-recipe warning is an advisory setting. It is enabled by default
  above 50,000 authored recipes, and its threshold can be changed in
  `settings.ini` (`[diagnostics]`; legacy JSONC remains supported). It never caps or skips recipe loading.
- A mod marked `0` in `runeschema.txt` contributes no RuneSchema definitions,
  PAKs, LogicMods, or Lua companion on the next launch. Enabled mods retain
  their listed order.

The package includes RuneSchema, Helpy, the shared Registry Bridge, and
RSNetworking, plus the shared QuestItems helper. These are one download; optional plugins can be disabled when no
installed mod requires them. [Choose optional bridges](OPTIONAL-BRIDGES.md).

## Find the right authoring path

| Goal | Start here |
| --- | --- |
| Edit existing game data with JSON/JSONC | [First mod](AUTHORING-GUIDE.md) and [loader folder guide](LOADER-WALKTHROUGHS.md) |
| Add a new cooked item, mesh, station, or other asset | [PAK content guide](PAK-CONTENT-GUIDE.md) |
| Make a cooked item discoverable in a live registry | [Cooked registry declarations](COOKED-PAK-REGISTRY-MANIFEST.md) |
| Add physical, ranged, or magic attacks | [Weapons and attacks](WEAPONS-AND-ITEMS.md) |
| Add crafting or processing | [Stations and recipes](STATIONS-AND-RECIPES.md) |
| Add custom armor and appearance | [Custom armor](CUSTOM-ARMOR.md) |

## Save and multiplayer boundaries

PAK mounting, object resolution, live registration, and gameplay connection
are different steps. A mounted file is not proof that its item or attack is
usable. Check each layer in the startup log and in the game. The server remains
authoritative for multiplayer gameplay; participating clients need the cooked
content they must use or display. [Platform and multiplayer guide](COMPATIBILITY.md).

RuneSchema's cleanup uses applicable live registries after enabled content has
loaded. It does not infer that a saved ID is orphaned merely because it came
from a mod. Back up important characters before changing a gameplay mod list.
[Save safety and removal](SAFE-SAVE-AND-LEDGER.md).

## What RuneSchema does not do

RuneSchema cannot fix a PAK with missing cooked assets, wrong paths, or broken
Blueprints. It cannot invent an armor material, the missing half of an attack
pair, or multiplayer behavior for a mod that has not authored it. A registry
declaration names content and validates supported live lanes; it does not make
every asset inside a PAK active automatically.
