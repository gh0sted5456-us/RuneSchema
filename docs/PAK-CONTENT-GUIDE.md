# Create new PAK content for RuneSchema

A PAK is the package that carries new Unreal content into Dragonwilds. Use one
when your mod needs a new mesh, texture, icon, sound, animation, Blueprint,
item, recipe, or station that does not already exist in the game.

RuneSchema does not replace Unreal's packaging tools. It mounts enabled PAK
folders, lets smaller mods connect cooked objects through full paths, applies
requested compatibility edits, reports failures clearly, and safely removes
saved references after a mod is uninstalled.

For a very large content library, bake the item, recipe, station, and unlock
relationships into the PAK itself. RuneSchema should not need thousands of
empty files that merely repeat cooked object paths.

## What to prepare

Before building the PAK, decide:

- the permanent mod folder name;
- the permanent Unreal content root, such as `/MyGoldenArsenal/`;
- which existing game item or station is the closest working reference;
- which assets are genuinely new;
- which parts can be handled by RuneSchema files instead;
- which players and servers need the content in multiplayer.

Changing the content root after release changes every object path. Choose it
once and keep it.

## Use unique content paths

Give the mod its own content root. Good examples are:

```text
/GoldenArsenal/Items/ITEM_GoldenSpear
/GoldenArsenal/UI/T_GoldenSpear
/GoldenArsenal/Buildings/BP_GoldenForge
/GoldenArsenal/Data/DT_GoldenForge
```

Do not cook new files over a native `/Game/Gameplay/...` path. Do not reuse
another mod's root. Unique paths prevent one PAK from silently replacing
another PAK's content.

The full object path normally repeats the final asset name:

```text
/GoldenArsenal/Items/ITEM_GoldenSpear.ITEM_GoldenSpear
```

Copy that exact path from the editor or an asset inspection tool. Folder names,
asset names, punctuation, and letter case must match.

## Start from a working game reference

Find the closest native example before creating anything new. For a spear, use
a working spear item, held actor, and animation family. For a forge, use a
working crafting station with the same style of interaction.

Record:

- the source item's class and item type;
- the mesh, icon, held actor, and animation references;
- the station's building data, actor, interaction, and table row;
- the recipe structure used by that station;
- any required parent Blueprint or component.

Copying the shape of a working feature is safer than building an empty asset
and guessing which values the game requires.

## Build the content in Unreal Editor

Use the editor setup that matches the current Dragonwilds build. A mismatched
editor or an incomplete reference project can package files that the game
cannot read.

For each new asset:

1. Place it under your permanent content root.
2. Use a compatible game class or parent Blueprint.
3. Fill required references with content that will also be included in the PAK
   or already exists in the game.
4. Save and compile every Blueprint.
5. Fix missing-reference and compile warnings before packaging.
6. Test the asset in the editor where possible.

Do not copy editor-only helper files into the release. The game needs cooked
content, not loose `.uasset` files copied from the project folder.

## Prepare saved content identities

Items, recipes, quests, and journal entries can be stored in a character save.
Each new saved entry needs a permanent identity.

For an item, keep these values unique and unchanged after release:

- object path;
- internal name;
- persistence ID.

Helpy's item authoring tool includes **Generate new ID** for a new item copy.
Generate an ID once, record it in the mod's source notes, and never recycle it
for another item.

If the cooked asset already contains its identity fields, RuneSchema verifies
them when it joins the live item list. If RuneSchema creates a copy through an
`assets` file, put the new internal name and persistence ID in that definition.

## Package for Windows

Cook and package the selected content for the Windows game target. The exact
editor buttons depend on the project setup, but the result is one of these:

- a `.pak` file; or
- a `.pak`, `.utoc`, and `.ucas` set; or
- the same set with a matching `.sig` file.

Keep every file produced for that package together. Do not create empty `.utoc`,
`.ucas`, or `.sig` files when the build did not produce them.

For the clearest automatic discovery, keep the package name aligned with the
content root. Content under `/GoldenArsenal/` should normally ship in a package
named `GoldenArsenal` rather than an unrelated name.

Open the finished package in your normal inspection tool and confirm that the
internal paths match the paths used by your RuneSchema files.

## Install the PAK inside the mod

Recommended layout:

```text
RuneSchema/
  mods/
    GoldenArsenal/
      assets/
        10-items.jsonc
      recipes/
        20-recipes.jsonc
      buildings/
        30-forge.jsonc
      paks/
        GoldenArsenal/
          GoldenArsenal.pak
          GoldenArsenal.utoc
          GoldenArsenal.ucas
```

Keep separate packages in separate subfolders. Do not install the same package
twice in different locations.

Enable the mod in `RuneSchema/mods/runeschema.txt`:

```text
GoldenArsenal : 1
```

A `0` prevents RuneSchema from mounting or loading that mod folder.

## Reference cooked content from a smaller mod

A direct reference is useful for an ordinary-sized mod because it documents the
required object and guarantees that the setup asks for the exact path.

An `assets` file may reference an existing cooked item without changing it:

```json
{
  "/GoldenArsenal/Items/ITEM_GoldenSpear.ITEM_GoldenSpear": {}
}
```

Use this when the PAK already contains the correct item values and the mod needs
RuneSchema to load or patch that particular object. Add fields inside the braces
only when a value must be adjusted at startup.

You can also reference the item directly from a recipe. RuneSchema checks that
the path resolves to a real item before accepting the recipe.

## What registration means

Dragonwilds keeps live lists for items, recipes, quests, journal entries, magic,
and equipment effects. Mounting a PAK makes its cooked files available. Content
still needs the native relationships that cause the game to load and use it.

RuneSchema verifies the live entries it handles or observes:

- the permanent ID is present;
- the ID does not point to two different objects;
- the internal name points to the same object;
- the entry can be sent correctly in multiplayer;
- the entry remains available for the rest of the game launch.

An unused cooked data asset does not become gameplay content merely because it
exists in a container. Reference it from the PAK's cooked gameplay structure or
from a focused RuneSchema definition.

## Use the log to prove each step

Treat these as separate checks:

1. **Mounted:** RuneSchema added the mod's PAK folder.
2. **Found:** the game could see the cooked asset path.
3. **Loaded:** RuneSchema opened the asset successfully.
4. **Registered:** the matching live game list accepted it.
5. **Connected:** a recipe, station, quest, or vendor points to it.

A mounted PAK is not proof that an item registered. A visible icon is not proof
that a saved identity is healthy. Read the complete startup summary.

## Large content packs

Large packs should use unique paths and permanent identities, but their main
catalogue should travel through the native PAK route. Bake these relationships
into the cooked content:

- items and their identity fields;
- recipes and their item references;
- station or recipe-collection membership;
- normal unlock relationships;
- any required combat, magic, equipment, or presentation references.

Use RuneSchema only for the integration that cannot sensibly be baked into the
PAK. For example, a currency pack may use a small `/raw` file to add consumable
packs to an existing enemy loot DataTable.

If RuneSchema receives more than 1,000 recipe definitions, it prints one
advisory warning with the count and continues loading without limits.

## Test before release

1. Test the PAK with no RuneSchema recipe and confirm the mount message.
2. Confirm the PAK's cooked station and recipe relationships work on their own.
3. Add one focused RuneSchema integration, if the mod actually needs one.
4. Craft or obtain the item.
5. Save, return to the menu, and re-enter the world.
6. Restart the game and load the character again.
7. Test a second client if the content affects multiplayer.
8. Back up the save, disable the mod, restart, and confirm that only missing
   identities are removed.

## Update or remove the mod safely

An update should keep released object paths, internal names, and persistence IDs.
Add new content with new identities. Do not assign an old identity to a replacement
object just because the original item was removed.

For removal:

1. close the game;
2. set the mod to `0` or remove its folder;
3. restart the game so RuneSchema can build a complete live content list;
4. load a backed-up test character first;
5. read the cleanup warning and confirm that only the removed mod's missing
   identities were pruned.

RuneSchema cannot restore an item after it has been removed from the save.
