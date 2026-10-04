# RuneSchema folder guide

Each folder inside a RuneSchema mod has one job. Keeping those jobs separate
makes a mod easier to test, update, and remove.

## Content folders

| Folder | Typical content | Restart recommended |
| --- | --- | --- |
| `assets` | Items, item copies, icons, names, durability, and item references | Yes for new content |
| `blueprints` | Existing Blueprint values, components, menus, fonts, and colors | Yes |
| `buildings` | Buildable pieces, placement rules, costs, and station settings | Yes |
| `courses` | Supported course records | Yes |
| `dialogue` | Conversations and choices | Yes |
| `effects` | Supported effects | Yes |
| `enums` | Supported named value additions | Yes |
| `equipment` | Special supported equipment behavior | Yes |
| `events` | Encounters and event stages | Yes |
| `journal` | Journal entries and recipe pages | Yes |
| `lore` | Books and lore records | Yes |
| `nameplates` | Supported names shown in the world | Yes |
| `niagara` | Supported visual-effect settings | Yes |
| `npc` | Non-player characters and world interactions | Yes |
| `players` | Player profiles and supported appearance rules | Yes |
| `quests` | Quest definitions and objectives | Yes |
| `raw` | Rows in existing game tables | Yes |
| `recipes` | Crafting, processing, dismantling, and station placement | Yes |
| `registry` | Supported presentation and action declarations | Yes |
| `spawns` | Enemies, resources, props, and placed actors | Yes |
| `strings` | Supported text replacements | Usually |
| `vendors` | Shops and offers | Yes |
| `paks` | Cooked Unreal files used by the other folders | Always |

## How the folders work together

A new weapon normally uses:

1. `paks` for the mesh, icon, Blueprint, and cooked item;
2. `assets` to load the item path and apply safe adjustments;
3. `recipes` to make the weapon obtainable;
4. `journal` if it should appear in recipe discovery;
5. `equipment` only when it needs a supported special behavior.

A new station normally uses:

1. `paks` for the station actor, building data, and optional station table;
2. `buildings` for cost, placement, stability, and interaction settings;
3. `recipes` to place recipes into the station;
4. `strings` or `blueprints` only when the station needs additional text or interface changes.

## The registry folder is not an item list

The `registry` folder supports presentation and action declarations used by
specific RuneSchema features. It does not replace the game's item or recipe
lists and should not be filled with every PAK asset.

For an ordinary-sized mod, make a cooked item available by using its exact path
in `assets` or in a recipe. For a large catalogue, bake normal item, recipe,
station, and unlock relationships into the PAK rather than creating thousands
of empty loader files.

Custom combat, magic, and equipment data can require different authoritative
game registries. The planned generic solution is one cooked manifest per PAK,
with atomic validation and multiplayer order checks. See the
[cooked PAK registry manifest design](COOKED-PAK-REGISTRY-MANIFEST.md). It is a
design target; the guide identifies which lanes are currently verified and
which still require live reflection.

## File order inside a folder

Use names such as `10-items.jsonc`, `20-patches.jsonc`, and
`90-compatibility.jsonc`. Put the base definition first and later adjustments
after it.

The order of mod folders comes from `runeschema.txt`. The order of PAK content
that replaces the same internal file is not a safe compatibility method. Use
unique paths instead of asking load order to choose between two cooked copies.

## When to use a later compatibility mod

Create a separate compatibility folder when two independent mods need a small
bridge. Put it after both mods in `runeschema.txt` and keep it limited to the
required changes. This lets either base mod update without hiding which files
belong to the bridge.
