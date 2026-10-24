# RuneSchema folder guide

Each folder inside a RuneSchema mod has one job. Keeping those jobs separate
makes a mod easier to test, update, and remove.

## Choose the first loader for your goal

| Goal | Start with | Add another folder only when needed |
| --- | --- | --- |
| Resolve or adjust an item that already exists in a PAK | `assets` with its full cooked object path | `recipes` to craft it; `journal` for a discovery page. |
| Add a new mesh, icon, sound, animation, or Blueprint class | `paks` for the cooked files | `assets`, `blueprints`, or a cooked registry declaration to connect them. |
| Change a row in an existing DataTable, including a mod-owned cooked table | `raw` | `paks` if the table itself is new cooked content. |
| Add a building or station | `buildings` plus its cooked actor/data in `paks` | `recipes` for station output; `quests` for progression. |
| Put a recipe into a crafting or processing station | `recipes` | `assets` only if the item also needs loading or adjustment. |
| Add a quest-driven character | `quests`, `npc`, and `dialogue` | `events`, `spawns`, `journal`, or `lore` for the specific feature. |
| Offer craftable quest contracts on a Quest Board | `quests`, `assets`, and `recipes` | Follow the [quest-to-board walkthrough](modders/quests-and-questboards.md); install the separate board provider. |
| Change supported interface text, font, or color | `blueprints` or `strings` | `paks` only for new cooked fonts or widget classes. |
| Make a large cooked catalogue discoverable | A mod-owned cooked registry asset inside `paks` | Ordinary loaders only for focused patches or missing relationships. |
| Register existing cooked items without a cooked registry declaration | Loose `registry/*.jsonc` with `NativeRegistries.Items` | Keep the cooked item provider mounted; add recipe/station/discovery tie-ins as needed. |

A loader file cannot cook a missing Unreal asset. A PAK mount cannot, by
itself, put every packaged object into the live gameplay list. For new content,
follow [PAK mounting](PAK-CONTENT-GUIDE.md) and the
[live registry guide](COOKED-PAK-REGISTRY-MANIFEST.md) together.

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
| `registry` | Supported native-registry references, presentation and action declarations | Yes |
| `spawns` | Enemies, resources, props, and placed actors | Yes |
| `strings` | Supported text replacements | Usually |
| `vendors` | Shops and offers | Yes |
| `paks` | Cooked Unreal files used by the other folders | Always |
| `logicmods` | Cooked packages whose `ModActor` should start through RuneSchema's Lua helper on clients | Always |
| `ue4ss` | Optional Lua companion at `ue4ss/scripts/main.lua` | Always |

The `ue4ss` folder is not a normal JSON loader. RuneSchema's Lua bootstrap
starts its `scripts/main.lua` only when that RuneSchema mod is enabled. See the
[UE4SS Lua companion guide](UE4SS-LUA-COMPANIONS.md) for the complete layout,
logging, safety, and multiplayer rules.

`logicmods` is also optional. Unlike `paks`, it opts a cooked package into
Blueprint actor startup. See [Blueprint and Lua companion folders](PSEUDO-LOADERS.md)
for both folder layouts and their enable rules.

## How the folders work together

### NPC removal is safe

Actors created through `npc` are transient world actors. RuneSchema recreates
them from the enabled mod each time a world loads; it does not store the actor
itself in the character or world save. Quest, dialogue, and other gameplay
progress still use their normal game-owned save systems.

When an NPC mod is disabled or removed, its actor therefore does not return as
an invisible collision shell. RuneSchema also recognizes the private identity
used by older RuneSchema NPC builds and retires those legacy saved shells. It
does not apply this migration to native game NPCs or actors owned by other
systems.

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

## The registry folder declares references, not missing assets

The `registry` folder supports native-registry references as well as presentation
and action declarations. A loose declaration can announce exact cooked ItemData
paths when the PAK has no cooked registry declaration. It does not replace
ItemData construction or automatically scan every asset in a PAK.

For an ordinary-sized mod, make a cooked item available by using its exact path
in `assets` or in a recipe. For a large catalogue, bake normal item, recipe,
station, and unlock relationships into the PAK rather than creating thousands
of empty loader files.

Custom combat, magic, and equipment data can require different authoritative
game registries. A mod can ship a loose registry declaration or cook its own
declaration asset for the supported lanes. Start with the
[loose item-registration walkthrough](reference/loaders/registry.md#register-a-cooked-item-without-cooking-a-registry-declaration)
and consult the [current lanes and design limits](COOKED-PAK-REGISTRY-MANIFEST.md)
before relying on a lane for a new feature. In particular, attack lanes have
their own fallback and item-owned collection limits; an accepted path is not
proof that a global attack registry was installed.

## Open the detailed loader reference

- Items and game data: [assets](reference/loaders/assets.md),
  [raw tables](reference/loaders/raw.md), [recipes](reference/loaders/recipes.md),
  [buildings](reference/loaders/buildings.md), and
  [equipment](reference/loaders/equipment.md).
- Story and world: [quests](reference/loaders/quests.md),
  [dialogue](reference/loaders/dialogue.md), [events](reference/loaders/events.md),
  [journal](reference/loaders/journal.md), [lore](reference/loaders/lore.md),
  [NPCs](reference/loaders/npc.md), [spawns](reference/loaders/spawns.md), and
  [vendors](reference/loaders/vendors.md).
- Presentation and rules: [blueprints](reference/loaders/blueprints.md),
  [strings](reference/loaders/strings.md), [nameplates](reference/loaders/nameplates.md),
  [players](reference/loaders/players.md), [effects](reference/loaders/effects.md),
  [niagara](reference/loaders/niagara.md), [courses](reference/loaders/courses.md),
  [enums](reference/loaders/enums.md), and [registry](reference/loaders/registry.md).

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
