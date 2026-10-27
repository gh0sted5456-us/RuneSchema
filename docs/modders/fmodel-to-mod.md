# From an FModel export to your first mod

This learning path uses real patterns from the author's Increased Storage - 3,
Bigger Inventory and ResourcesAndLoot mods. It targets the
[0.8.0.2 release](../RELEASE-0.8.0.2.md), which includes the earlier 0.8.0.1b
source fixes. Check the [download page](../CURRENT-RELEASE.md) for the current package.

You will learn to identify the object that owns a value, discard export-only
wrappers, keep only your changes, and place the result in the correct loader.
You do **not** need to recook an existing compatible asset merely to patch a
reflected value. New meshes, classes or Blueprint logic still need cooked content.

## 1. Choose one measurable change

Start with one of these lessons:

| Goal | Walkthrough | Loader |
| --- | --- | --- |
| Increase a chest's slots | [Bigger chests](bigger-chests.md) | `blueprints` |
| Add a creature drop without deleting its other drops | [Creature loot](creature-drops.md) | `raw` |
| Change a cooked item's stack size | [Patch an existing item](cooked-item-patches.md) | `assets` |
| Make a separate resource-item variant | [Item clones](item-clones.md) | `assets` |
| Change selected inventory pages | [Bigger inventory](bigger-inventory.md) | `blueprints` |
| Understand replacement, append and selection | [Patch operators](append-and-targeted-patches.md) | Depends on target |

Use a disposable world and character, especially for capacity or save-bearing
changes. Do not begin by editing every export in a folder.

## 2. Inspect the current installed game

1. Open the archives for the Dragonwilds build you are actually testing. Check the current Game Pass layout separately when working from an older Steam export.
2. Configure FModel's engine/versioning and matching mapping as required for
   that build. RuneSchema's mapping status is described in
   [mappings and writable paths](../SETTINGS-INI.md#game-mappings-and-writable-state).
   A stale mapping can make an export misleading.
3. Find the package, open its properties, and follow references to the object
   or table that actually owns the field. A building piece, its actor, and the
   actor's inventory component are different objects.
4. Save the **Properties (.json)** output somewhere outside your mod's loader
   folders. Keep the untouched original as a comparison file.
5. Record the package/object path, class, component or row, field name, value
   type, game build and platform in your own notes.

FModel's [getting-started guide](https://github.com/4sval/FModel/wiki/Getting-Started)
describes the property view. Its [release notes](https://github.com/4sval/FModel/releases)
distinguish JSON property exports from raw package/model/world exports; exact
menu placement varies by FModel version. Exporting a mesh or raw `.uasset` is
not the same operation as saving properties as JSON.

## 3. Read the export before changing it

Here is a **reconstructed, trimmed FModel-shaped teaching input**. The
owner/component/field names are backed by Increased Storage - 3; the starting
values are illustrative. It is not represented as a verbatim export of your
current game. Real files commonly have more exports, flags and references.

```jsonc title="Reference input — do not install" linenums="1"
--8<-- "docs/examples/Walkthroughs/exports/ash-chest.fmodel.jsonc"
```

| Part | What it tells you | What the mod needs |
| --- | --- | --- |
| Outer `[...]` | The package export is a list of records. | Not automatically the loader's required envelope. |
| `Type` | Which class describes this exported record. | Research information, not a field to paste onto the target. |
| `Name` | The record/component's name. | The component key when following it under its owning Blueprint. |
| `Outer` | A clue to the owning object/default. | Identify the owning Blueprint class; do not paste `Default__` as a new property. |
| `Properties` | Exported property values. | Extract only the reflected fields your mod intends to change. |
| `MaxSlotCount` | The capacity field on this component. | A JSON number, at the component level. |
| `bSupportsSortAndFillStacks` | A separate Boolean behavior flag. | `true`/`false`, not the strings `"true"`/`"false"`. |

Do not delete `Properties` blindly from every export. First work out **who owns
it**. If you flatten a component's fields into the actor, the fields are on the
wrong object even though the JSON parses.

## 4. Turn the relevant fields into a loader document

The installed storage mod expresses the change like this:

```jsonc title="BiggerChests/blueprints/10-ash-chest.jsonc" linenums="1"
--8<-- "docs/examples/Walkthroughs/BiggerChests/blueprints/10-ash-chest.jsonc"
```

The class is the outer key, the inventory component is the next key, and only
the two desired values remain. There is no export `Type`, `Outer`, `Properties`
or copied full component list. The [chest lesson](bigger-chests.md) explains
every line and the other chest sizes.

## 5. Recognize the three common export shapes

| Export evidence | Loader document shape | Do not confuse it with |
| --- | --- | --- |
| Blueprint/component defaults | `class → component → changed fields` in `blueprints` | A new Blueprint class or cooked actor. |
| DataTable with `Rows` | `table → row → changed fields`, or explicit `table:row` patch in `raw` | An ItemData object. |
| Existing ItemData | `object path → changed fields` or an explicit asset patch in `assets` | A DataTable row just because the file exports as JSON. |

The [loot lesson](creature-drops.md) walks a table export into `$Append`.
The [item lesson](cooked-item-patches.md) walks an item export into a focused
patch. [Cloning](item-clones.md) is a separate operation with a new save identity.

## 6. Translate references deliberately

These are different things:

| Example | Meaning |
| --- | --- |
| `.../ITEM_Resources_Wood_Ash.uasset` | Package filename, not the preferred runtime object reference. |
| `/Game/.../ITEM_Resources_Wood_Ash.ITEM_Resources_Wood_Ash` | Complete object path used in the tutorials. |
| `/Game/.../ITEM_Resources_Abyssal_Ashes.0` plus `ObjectName` | FModel export-index reference; the name identifies the actual export. |
| `.../BP_Name.BP_Name_C` | Blueprint-generated class. The `_C` is significant when a class is required. |
| `AbyssalDemon` | A row name inside a particular table, not a package. |
| `PersistenceID` | Saved identity, not a filename or display name. |

RuneSchema accepts several exported reference forms, including supported
`ObjectName`/`ObjectPath` and `AssetPathName` wrappers. In hand-written reference
fields, use the inspected complete object-path string where supported. Do not
blindly replace every `.0`: a package may contain multiple exports or subobjects.
The numeric suffix alone does not tell you the object name.

`/Game/Mods`, `/PluginMount`, a Windows path and the RuneSchema mod-folder name
are not interchangeable. A loose mod folder does not rename an existing asset's
Unreal mount root.

## 7. Install one lesson, not the export directory

```text
RuneSchema/
  mods/
    runeschema.txt
    BiggerChests/
      blueprints/
        10-ash-chest.jsonc
```

Add `BiggerChests : 1` to `mods/runeschema.txt`. UE4SS's separate `Mods/mods.txt`
must already have `RuneSchema : 1`. Restart Dragonwilds completely. Numbered
filenames organize files **inside a loader**; they do not establish one global
execution order across `assets`, `raw`, `recipes` and every other loader.

Open **Helpy → Diagnostics**, inspect the RuneSchema log and search the mod,
target and rejection reason. A parse success is not proof of a reflected field
write, and a field write is not proof that the game consumes it as intended.

## 8. Prove the result and keep the smallest diff

Compare before/after behavior, newly created and existing objects, save/reload,
menu re-entry and a complete restart. Keep unrelated fields unchanged and test
the server/client combinations you plan to support. Use the
[release checklist](testing-and-compatibility.md) before sharing.

Download the [teaching inputs and finished example folders](../examples/Walkthroughs/README.md).
They contain no PAKs or user saves. Keep original game exports for research;
distribute the focused authored change, not a whole game dump.
