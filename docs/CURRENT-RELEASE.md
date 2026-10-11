# RuneSchema 0.7.7.9m

RuneSchema loads enabled mod folders, mounts their cooked content, and connects
supported definitions to Dragonwilds' live systems. The current package uses
one DLL for Steam/GOG and Game Pass. The `m` release is the main channel; `e`
is the experimental channel.

[Download 0.7.7.9m](https://github.com/gh0sted5456-us/RuneSchema/releases/download/0.7.7.9m/RuneSchema-0.7.7.9m.zip)
· [Experimental 0.7.7.9e](https://github.com/gh0sted5456-us/RuneSchema/releases/download/0.7.7.9e/RuneSchema-0.7.7.9e.zip)
· [Installation guide](INSTALLATION.md)

## What changed

- Custom cooked wearable items can join the game's item enumeration and native
  appearance cache. When equipment changes clear a previously built cache
  entry, RuneSchema can ask the game to rebuild it again. The game still applies
  the item's authored material and hide rules; RuneSchema does not create them.
- A cooked registry declaration can name wearable mesh-data dependencies in
  `WearableMeshData`. This checks that those assets resolve; it is not a
  substitute for the item's male/female mesh-data references.
- The large-recipe warning is an advisory setting. It is enabled by default
  above 50,000 authored recipes, and its threshold can be changed in
  `settings.jsonc`. It never caps or skips recipe loading.
- A mod marked `0` in `runeschema.txt` contributes no RuneSchema definitions,
  PAKs, LogicMods, or Lua companion on the next launch. Enabled mods retain
  their listed order.

The package includes RuneSchema, Helpy, the shared Registry Bridge, and
RSNetworking. These are one download; optional plugins can be disabled when no
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
