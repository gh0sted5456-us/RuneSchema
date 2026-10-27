# RuneSchema downloads and source updates

The published download and maintained source use version **0.8.0.2**.
The release download was refreshed on October 27 with the maintenance changes listed below.
[Read the changelog](CHANGELOG-0.8.0.2.md). This release includes the earlier
0.8.0.1b source-only fixes as well as the current cache improvements.

## Published download: 0.8.0.2

The current release is **0.8.0.2**, originally published October 25 and refreshed October 27, 2026.
[Download 0.8.0.2](https://github.com/gh0sted5456-us/RuneSchema/releases/download/0.8.0.2/RuneSchema-0.8.0.2.zip)
· [Package, defaults and verification](RELEASE-0.8.0.2.md)
· [Changes since 0.8.0.1](CHANGELOG-0.8.0.2.md).
The release source is published on `main` and `experimental`, and the
`0.8.0.2` tag now identifies the refreshed source for this maintenance package. The cleaned ZIP
is locally built and verified, not a hosted CI artifact.

RuneSchema loads enabled mod folders, mounts their cooked content, and connects
supported definitions to Dragonwilds' live systems. The main release now uses
the plain version/tag `0.8.0.2`; `m`/`e` suffixes describe historical releases.
The package uses one core DLL for Steam/Epic, Game Pass and servers, plus Helpy.

[Previous release: 0.8.0.1](RELEASE-0.8.0.1.md)
· [Historical changelog](CHANGELOG-0.8.0.0.md)
· [Installation guide](INSTALLATION.md)

## October 27 maintenance refresh

`main` and `experimental` include additional cache discovery/persistence fixes,
cart selection and presets, map-cache export, bounded teleport ground probing,
a configurable Helpy shortcut, diagnostic improvements and guarded server startup
corrections. Read the [complete changelog since 0.8.0.1](CHANGELOG-0.8.0.2.md).

The refreshed download retains version 0.8.0.2 and replaces the October 25 ZIP
under the same URL. Its verification JSON and checksums identify the new bytes;
the release tag was moved to the matching current source. Experimental registry-ID
alignment remains disabled by default. The maintenance DLL pair passed 248 core
and 109 Helpy tests; see the changelog for remaining live-game checks.

## What changed

This release includes [registry/save/cart fixes](CHANGELOG-0.8.0.1b.md),
the independent GitHub cache catalogue, owner-scoped mod caches, an optional
[offline PAK cache builder](modders/offline-helpy-cache.md), current native
item/journal cache data and expanded authoring walkthroughs.
Both DLLs are compressed. The stock UE4SS host is not replaced.
The release notes state the scope of automated verification.

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

## Using mods with saves and multiplayer {#save-and-multiplayer-boundaries}

Follow a new item from PAK mounting through object lookup and registration to
its in-game behavior. The startup log helps you check each step and find where
to make an adjustment. The server remains
authoritative for multiplayer gameplay; participating clients need the cooked
content they must use or display. [Platform and multiplayer guide](COMPATIBILITY.md).

RuneSchema's cleanup uses applicable live registries after enabled content has
loaded. It does not infer that a saved ID is orphaned merely because it came
from a mod. Back up important characters before changing a gameplay mod list.
[Save safety and removal](SAFE-SAVE-AND-LEDGER.md).

## What your mod supplies {#what-runeschema-does-not-do}

RuneSchema cannot fix a PAK with missing cooked assets, wrong paths, or broken
Blueprints. It cannot invent an armor material, the missing half of an attack
pair, or multiplayer behavior for a mod that has not authored it. A registry
declaration names content and validates supported live lanes; it does not make
every asset inside a PAK active automatically.
