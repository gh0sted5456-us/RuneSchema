# Create your first RuneSchema mod

## Step-by-step workflow

1. Create and enable one stable mod folder under `RuneSchema/mods`; add only the loader folders your feature uses.
2. Follow [FModel export to mod](modders/fmodel-to-mod.md), then make the small [Bigger Chests patch](modders/bigger-chests.md). Inspect paths and fields in your game version before copying them.
3. Connect the result to its consumer: an item needs a recipe/vendor, a quest needs acceptance, and a station needs its real actor row handle.
4. Restart, inspect targeted diagnostics, then verify the feature and saved progress before distributing it.

The [worked tutorials](modders/index.md#learn-with-real-mod-patterns) distinguish
real mod patterns, reconstructed FModel excerpts and placeholders. A valid JSONC
file cannot supply a missing cooked dependency.

**Tie-ins:** [All loaders](LOADER-WALKTHROUGHS.md), [quests and boards](modders/quests-and-questboards.md), [cache bundles](CACHE-BUNDLES.md) and [diagnostic codes](ERROR-CODES.md).


RuneSchema lets a mod connect content to Dragonwilds without replacing large
parts of the game. A small mod can change an existing value with one file. A
larger mod can combine new PAK content with items, recipes, stations, quests,
vendors, and interface changes.

This guide explains the shared foundation. The focused guides cover
[PAK content](PAK-CONTENT-GUIDE.md), [weapons and items](WEAPONS-AND-ITEMS.md),
and [stations and recipes](STATIONS-AND-RECIPES.md). For a complete quest-to-board
flow, follow [Build a quest and put it on a Quest Board](modders/quests-and-questboards.md).

If you prefer a guided starting point, the in-game
[Advanced Authoring & Tools workflow](ADVANCED-AUTHORING-TOOLS.md) can search
records, export a JSONC starter, and check its dependencies before installation.

## Create the mod folder

Give the mod one clear, permanent folder name beneath `RuneSchema/mods`:

```text
RuneSchema/
  mods/
    MyDragonwildsMod/
```

Use letters, numbers, dashes, or underscores. Avoid renaming the folder after
release because other mods may use it when referring to your content.

Add the folder to `RuneSchema/mods/runeschema.txt`:

```text
MyDragonwildsMod : 1
```

The position of this line is the mod's load position. Put a compatibility mod
after the mods it changes.

## Add only the folders you need

```text
MyDragonwildsMod/
  assets/
  blueprints/
  buildings/
  dialogue/
  equipment/
  events/
  journal/
  lore/
  npc/
  quests/
  raw/
  recipes/
  registry/
  spawns/
  strings/
  ue4ss/
  vendors/
  paks/
  logicmods/
```

You do not need empty folders. RuneSchema reads each folder for a different
kind of work.

| Folder | Use it for |
| --- | --- |
| `assets` | Load, copy, or adjust items and other supported data assets. |
| `blueprints` | Adjust an existing Blueprint, component, menu, font, or color. |
| `buildings` | Add or adjust building and station definitions. |
| `recipes` | Create recipes and place them in crafting or processing stations. |
| `raw` | Add or adjust rows in an existing game table. |
| `quests`, `dialogue`, `events` | Build connected story and encounter content. |
| `journal`, `lore` | Add readable and discoverable entries. |
| `npc`, `spawns`, `vendors` | Add world actors, enemies, and shops. |
| `equipment` | Add supported special behavior to equipment. |
| `registry` | Declare supported cooked native-data families, presentation/action content or explicit registry patches. Each family has its own contract; this is not an arbitrary registry writer. |
| `paks` | Store packaged Unreal content such as meshes, icons, sounds, items, and stations. |
| `logicmods` | Opt in cooked packages that provide a `ModActor` to Blueprint startup. |
| `ue4ss` | Lua-only UE4SS add-ons, with scripts/configs kept intact; uses unmodified UE4SS. Native/hybrid packages are skipped. |

[Open the complete folder guide](LOADER-WALKTHROUGHS.md).

For cooked Blueprint logic, put the package under `logicmods/<PackageName>/`
and include a default `ModActor.ModActor_C`. Packages under `paks` still mount,
but they do not automatically start a ModActor. See
[Cooked PAK Content](PAK-CONTENT-GUIDE.md#start-a-cooked-blueprint-modactor).

[Compare the two optional UE4SS-backed folders](PSEUDO-LOADERS.md) before
adding Blueprint startup or a Lua companion.

## Include complete Lua UE4SS packages

Use this only when cooked Blueprints and RuneSchema's data loaders cannot
express the behavior. Keep the feature inside the same mod folder:

```text
MyDragonwildsMod/
  ue4ss/
    MyLuaPackage/
      Scripts/main.lua
      Scripts/my_helpers.lua
      config.txt
    AnotherLuaPackage/
      Scripts/main.lua
      settings.ini
```

Keep each Lua package intact, including configs, helpers and metadata.
RuneSchema uses its Lua bootstrap on unmodified UE4SS; it does not
provide a managed/custom host. Nested native or hybrid DLL packages are
rejected and must instead be installed directly through stock UE4SS.
Package names must be unique. Package-local Lua context is not an independent
UE4SS VM/ModRef or a security sandbox.
The legacy direct `ue4ss/scripts/main.lua` layout still works; do not mix it
with child packages in one container.

RuneSchema starts companions once, in the enabled order recorded in
`RuneSchema/mods/runeschema.txt`. A row set to `0` is not started. A failure in
one companion is logged with that mod's name and does not stop later mods.
Disabling RuneSchema itself in UE4SS `Mods/mods.txt` disables this bootstrap and
every nested companion.

Lua companions are not sent over the network. Install the same mod version on
the server or host and every client that needs its local behavior. Keep game
authority on the server and use client Lua only for presentation or input.

[Open the full UE4SS Lua companion guide](UE4SS-LUA-COMPANIONS.md) for a
complete example, startup messages, troubleshooting, and safety guidance.

## Use numbered filenames

Numbered names make file order within a loader easy to understand. They do not
reorder the runtime's loader phases:

```text
assets/10-items.jsonc
assets/20-item-overrides.jsonc
raw/10-item-stats.jsonc
recipes/10-recipes.jsonc
```

Use `.jsonc` when you want comments in the file. Keep comments short and do not
leave old disabled copies beside the live file.

## Understand the three names an item can have

An item commonly has three different identifiers:

- The object path tells the game where the item lives, such as
  `/MyMod/Items/ITEM_GoldenSpear.ITEM_GoldenSpear`.
- The internal name is the short name used by game systems.
- The persistence ID is the permanent save identity.

Do not reuse a persistence ID or internal name for a different item. Once an
item has been released, keep all three values stable. Changing them can make an
existing saved item appear missing.

## Choose between copying and cooking

For an existing item, start with [a field patch](modders/cooked-item-patches.md).
For a new compatible variant, follow [the `$Clone` walkthrough](modders/item-clones.md).
For arrays, read [replacement, `$Append` and matched edits](modders/append-and-targeted-patches.md)
before copying an export's entire list.

Use a RuneSchema copy when the new item can reuse an existing mesh, icon,
animation family, and general behavior. This is the quickest route for an item
variant.

Create a PAK when the mod needs a new mesh, texture, icon, sound, animation,
Blueprint, station actor, or fully cooked data asset. RuneSchema then connects
that packaged content to the rest of the game.

## Build one layer at a time

The safest order is:

1. Load one item or building successfully.
2. Confirm its object path in the log.
3. Confirm its permanent identity is accepted.
4. Add one recipe that uses it.
5. Place the recipe in one station.
6. Test crafting and saving.
7. Add journal, quest, vendor, or special behavior later.

This order makes it clear which layer failed. Building the entire mod before
the first test makes a simple path mistake much harder to find.

## Test the full player journey

For each release candidate:

1. Start the game from a full restart.
2. Wait at the main menu for the RuneSchema summary.
3. Enter a test world.
4. Obtain or craft the new content.
5. Save and return to the main menu.
6. Re-enter the same world.
7. Restart the game and load the character again.
8. Test with a second player if the mod changes gameplay.
9. Disable the mod only on a backup character and confirm safe cleanup.

## Journal/lore workspace (candidate)

Helpy's Journal workspace separates native Journal/Lore, Author, and optional
Wiki reference articles. Native browsing is read-only: it never grants discovery,
marks an entry read, or changes a character save. Cached entries not observed in
the current runtime are explicitly labeled as references.

The Author tab can start with a new entry or copy a native entry's text/art into
a new draft. Choose a unique mod-prefixed ID, entry type, title, pages, cooked
image path and native subcategory path. Export writes under Helpy's
`exports/journal-drafts`; it does not install or overwrite a mod. Review the
draft against `docs/schemas/journal.schema.json` or `lore.schema.json`, then put
it in the appropriate folder of your own mod. Lore-loader entries must use the
Lore type. Discovery defaults to `Unlock: false`; placement and discovery are
separate contracts. Test references, placement, persistence and unlock conditions
in a disposable world before sharing.

Helpy maintains `cache/journal/catalog.json` and an observed, unmodified
`cache/journal/vanilla.json`. An optional supplied baseline may be placed at
`Mods/RuneSchema/assets/journal/vanilla.json`, using the version-1 native cache
format. Loading and writing are asynchronous. A completed loaded-object scan is
not a certified full-game export, and the candidate does not yet ship a verified
complete vanilla baseline. Game artwork/text remains Jagex content; do not
relicense it as RuneSchema MIT or mistake Wiki text licensing for game-media
licensing.

## Prepare a mod for sharing

Ship the whole mod folder, including its RuneSchema files and PAK containers.
For a CurseForge or Nexus Mods listing, use the
[mod-host and getting-started page](MOD-HOSTS-AND-GETTING-STARTED.md) to explain
the framework dependency, expected file types, and install path.
Include a short README with:

- the RuneSchema version used for testing;
- the supported Dragonwilds store versions;
- required mods;
- the recommended position in `runeschema.txt`;
- whether every multiplayer participant needs the mod;
- safe update and removal instructions;
- the object paths and permanent IDs that other authors may reference.
