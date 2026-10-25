# Advanced Authoring & Tools

## Step-by-step workflow

1. Open Helpy and choose the workspace for the feature you are authoring; searching/previewing does not install changes.
2. Select an inspected native record or cached reference, then prepare a focused editable starter.
3. Replace names, identities and paths; check dependencies and export to the relevant loader folder rather than copying a whole unrelated mod.
4. Review the exported JSONC, enable the mod, restart and verify native behavior separately from the preview.

JSONC snippets are authoring mock-ups. Replace example paths, IDs and values with
inspected content; a valid snippet cannot supply missing cooked dependencies.

**Tie-ins:** [Loader walkthroughs](LOADER-WALKTHROUGHS.md), [item authoring](WEAPONS-AND-ITEMS.md), [quests and boards](modders/quests-and-questboards.md) and [cache ownership](CACHE-BUNDLES.md).


RuneSchema provides authoring through Helpy's separate desktop
workspace. It can help you find a loaded record, prepare an editable JSONC
starter, check its references, and export it. Opening the page, searching, and
building a preview do not install a mod or change the game data.

## Open the tools

### Helpy desktop workspace

Open Helpy with **F2**. **Players → Items** handles browsing and delivery;
**Players → Clone / Recipes** and **Tools → Authoring** provide authoring
workspaces. **Diagnostics** combines grouped issues and **Live Log**. UI
preferences use `ui.jsonc`; runtime settings use [`settings.ini`](SETTINGS-INI.md).

The shared item bay defaults to **20 items per page**; columns adapt to the
window and view settings. Change page size below the items, then use
**Previous / Next**. Page changes preserve the cart. The catalogue fetches
bounded windows; it does not start a Full Scan. A partial catalogue says
“more available” rather than reporting a guessed total.

Search runs after a short typing pause or immediately on Enter. **All categories** filters item types. **View** contains Grid/List and optional column/row controls; the lower divider still supports free-height resizing. Narrow windows reduce columns and short windows scroll within the page. Preferences are shared across inventory and editor bays and saved in `ui.jsonc` (`views.itemPageSize`, `itemGridColumns`, `itemGridRows`, `itemGridFitRows`).

Choose an item, then **Add to cart**, or drag it into the compact cart. **Give items** is the only primary delivery action. **Loadouts** holds kit import/export; **More** holds list/player refresh and advanced scan tools. An empty cart does not reserve the height of a populated staging chest. Existing colors, scaling, list-view preferences and delivery confirmation semantics are retained.

**Players → Clone** separates source selection, customization and the resulting
item into connected cards. Inspect the source before creating a clone. Native
property and JSON overrides are in a separate expandable section. The source
item is not changed. Follow the [line-by-line clone guide](modders/item-clones.md)
for the actual runtime path, stable identity and an installable example.

**Recipes** connects inputs, station/timing and outputs. Drag items from the bottom bay into either side, or select an item and use the add buttons. Quantities are editable; a recipe can have multiple outputs. Choose a native station row and, where applicable, a labeled category. Optional processing duration has minimum and maximum bounds in seconds. These cards stack vertically when the available width is limited.

Use **Browse** to search native recipe metadata, or paste an exact cooked `RecipeData` object path and choose **Load**. Saving an override changes only the authored ingredients, outputs and optional timing; it preserves the cooked recipe's identity. Adding a station is optional and does not remove its original station placements. New recipes require a compatible station. **Save** writes a RuneSchema `/recipes` definition and reports its file path; restart to apply it. It does not silently grant inventory items. Station artwork comes from native row metadata or a cooked preview-icon path, not an automatic web download.

Normal catalog queries combine loaded objects with mounted asset-registry metadata. Unloaded AI/resource candidates remain visible but need **Inspect selected definition** before spawning. Inspection loads only the selected class and checks its native ancestry; native NPC-copy restrictions and resource save-exclusion checks still apply. The coverage message identifies when registry metadata is unavailable. Unknown mod directories are not guessed to be resource or AI classes: authored `/spawns` entries and the explicit Full Scan remain additional discovery paths.

Drag an item into the cart, or right-click **Add to cart**, select recipients
and choose **Give items**. Selecting an item does not grant it. Cart editing
pauses during delivery. Matching native inventory confirmations establish success;
confirmed and rejected entries are cleared and recorded in **Delivery results**.
Interrupted or unconfirmed work can remain staged for review, without automatic
retries. Inspect the recipient's inventory before retrying.

In **0.8.0.2**, right-click a confirmed **Last delivery** item to
**Reorder**, or its heading to stage all confirmed items. This rebuilds the
cart only; it does not deliver automatically or replay uncertain grants.

PowerLevel badges request the native game texture and display the item's actual native value, with a text fallback. Masterwork labels use native flags/tags; detailed tooltips retain the native `MasterworkType` identifier when supplied by the item. Item names and high PowerLevel do not imply masterwork quality.

Use **Export kit** to save the cart as JSON or commented JSONC. **Import kit**
lists the active profile's `loadouts` directory; use the displayed path and
**Refresh kits** after copying a kit. **Load into cart** replaces the staged cart,
not inventory. Missing content is not installed by importing a kit. The old
game-local `runtime/Helpy/kits` path is not the normal profile location.

Exports use a versioned, data-only `RuneSchemaHelpyKit` format: a schema version, optional name and up to 64 unique cooked item paths with quantities 1..10000. Imports are limited to 128 KiB and do not contain scripts, automatic delivery, target-player selection or authority overrides. Existing exports require explicit replacement confirmation and retain a backup. The in-window kit library does not open a Windows file dialog.

```jsonc
{
  "SchemaVersion": 1,
  "Type": "RuneSchemaHelpyKit",
  "Name": "Starter sword",
  "Items": [
    {"Item": "/Game/Gameplay/Character/Player/Equipment/Held/Sword/ITEM_Sword_Bronze.ITEM_Sword_Bronze", "Count": 1}
  ]
}
```

Delivery uses the authoritative inventory grant and confirmation path.
**Equip on receipt** requests supported native equipment after a grant; it
still requires a compatible available slot and verified native result. It does
not make an incompatible item equip. Advanced mode's **Inspect native equip /
unlock contracts** exports read-only metadata without invoking those functions.

### RuneSchema authoring workflow

Open **Tools → Authoring** in Helpy; the old engine-embedded
settings page is no longer the authoring entry point. If authoring is disabled,
review its runtime setting in **Settings** and follow the displayed restart or
session-activation requirements. Engine-backed actions still need a ready
runtime and their own safety checks.

Use the workflow selector for content/schema authoring, presets, traces,
profiles, inspection and results. Offline save cleanup remains a separate,
guarded workflow rather than an automatic step in creating a mod.

## Create an editable starter

1. Choose **Content & schemas** in the workflow selector.
2. Choose a **Content type**, or use **All Loaders** for the complete loader list.
3. Choose the loader and **Format**. **Basic starter** is the default. Use
   **$Patch** to change supported existing content, **$Clone** to create a
   supported copy, or **Fields reference** for research only. Unsupported
   formats are disabled for that loader.
4. Enter search text and select a result. The page says whether it searches
   installed authored files or loaded game names and paths. An inactive mod's
   authored file can appear in search results; that does not activate the mod.
5. Select the fields you need and choose **Build preview**. For a new item
   clone, set the mod folder and new name and generate a permanent persistence
   ID once. Keep that ID unchanged after release.
6. Review the draft. Use **Check changes and dependencies** to check the
   proposed changes and loaded references. Resolve reported problems before
   using **Export JSONC**.
7. Put the exported file in the matching folder of your own mod, then restart
   and test it on a backed-up character.

**Export editable reference JSONC** and **Fields reference** are research
outputs, not installable mods. The structural schema for the selected loader
can be exported from **Structural schema**. Under **All Loaders**, **Export
structural loader schemas** provides the full set.

Items, recipes, journal entries, quests, NPCs, spawns, and events have focused
content choices. Player and nameplate pages include their own guided controls;
the Items page also points to **Players → Clone** for item authoring.

## Know what each stage is for

| Stage | Use it to |
| --- | --- |
| Mod Authoring | Find records and export loader definitions. |
| Presets | Run supported diagnostic presets. |
| Traces | Narrow a live behavior or event path. |
| Inspector | Examine a target when a path or field is uncertain. |
| Results | Review completed tool output. |
| Save Cleanup | Perform a separate offline save operation only when you intend to. |

Authoring tools cannot make an incorrectly cooked PAK valid, and a preview is
not proof that a mod works in a world. Follow the [first-mod guide](AUTHORING-GUIDE.md)
and test first entry, return to menu, re-entry, and a full restart.

For the manual workflow, use [FModel export to mod](modders/fmodel-to-mod.md),
[chest capacity](modders/bigger-chests.md), [creature drops](modders/creature-drops.md)
and [the testing checklist](modders/testing-and-compatibility.md).
Live inventory deletion/scrapping is not part of this workflow. Character-copy
tools remain separate reviewed offline edits; see [profiles](HELPY-PROFILES.md).
