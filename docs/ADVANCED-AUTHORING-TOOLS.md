# Advanced Authoring & Tools

RuneSchema gives the in-game authoring page one full-width, top-to-bottom
workflow. It can help you find a loaded record, prepare an editable JSONC
starter, check its references, and export it. Opening the page, searching, and
building a preview do not install a mod or change the game data.

## Open the tools

### Helpy 7.8 alpha

Helpy's external F2 window has a compact **Simple / Advanced** switch in its header. Simple mode keeps everyday item browsing and delivery together; Advanced exposes Transmutate, Recipes, building tools, cleanup, full scans and detailed appearance controls. The preference is stored in `ui.jsonc` under `views.advancedMode`.

The shared item bay defaults to **5 columns × 4 rows**. Choose **Layout** above the bay to change the column/row counts, or drag its lower divider for a free-height pane. The grid fits the available window and uses fewer columns on narrow windows; scroll to browse the remaining items. This changes the viewport, not the catalog's item limit. These preferences are shared by the inventory and editor bays and saved in `ui.jsonc` (`views.itemGridColumns`, `itemGridRows`, `itemGridFitRows`). Existing colors, scaling and list-view preferences are retained.

**Transmutate** separates source selection, customization and the resulting item into connected cards, with the item bay below. Inspect the selected source before creating a clone. Native property and JSON overrides are in a separate expandable section. The source item is not changed.

**Recipes** connects inputs, station/timing and outputs. Drag items from the bottom bay into either side, or select an item and use the add buttons. Quantities are editable; a recipe can have multiple outputs. Choose a native station row and, where applicable, a labeled category. Optional processing duration has minimum and maximum bounds in seconds. These cards stack vertically when the available width is limited.

Use **Browse** to search native recipe metadata, or paste an exact cooked `RecipeData` object path and choose **Load**. Saving an override changes only the authored ingredients, outputs and optional timing; it preserves the cooked recipe's identity. Adding a station is optional and does not remove its original station placements. New recipes require a compatible station. **Save** writes a RuneSchema `/recipes` definition and reports its file path; restart to apply it. It does not silently grant inventory items. Station artwork comes from native row metadata or a cooked preview-icon path, not an automatic web download.

Normal catalog queries combine loaded objects with mounted asset-registry metadata. Unloaded AI/resource candidates remain visible but need **Inspect selected definition** before spawning. Inspection loads only the selected class and checks its native ancestry; native NPC-copy restrictions and resource save-exclusion checks still apply. The coverage message identifies when registry metadata is unavailable. Unknown mod directories are not guessed to be resource or AI classes: authored `/spawns` entries and the explicit Full Scan remain additional discovery paths.

The item inventory is above the cart. Drag an item down, or right-click **Add to cart**, then choose a player and **Deliver**. Selecting an item does not grant it. Cart editing pauses during delivery; entries leave the cart only after matching inventory-delta confirmations. Failed or unconfirmed entries stay staged for inspection, without automatic retries. Advanced mode also offers one-each and stack presets.

PowerLevel badges request the native game texture and display the item's actual native value, with a text fallback. Masterwork labels use native flags/tags; detailed tooltips retain the native `MasterworkType` identifier when supplied by the item. Item names and high PowerLevel do not imply masterwork quality.

Use **Export kit** to save the cart as JSON or commented JSONC. **Import kit** lists kits in the displayed `runtime/Helpy/kits` folder under RuneSchema's per-profile Saved state. Copy shared kits into that folder and choose **Refresh kits**. Selecting a file previews its name/count; **Load into cart** replaces the staged cart, not your inventory. Loaded-item metadata is refreshed on the game thread without mounting missing content. Unknown or unavailable items are checked again at delivery.

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

Delivery uses the existing authoritative inventory grant and confirmation path. **Auto-equip** is disabled until an inventory-instance equip command and its native slot/result contract are mapped; importing a kit cannot enable it. Advanced mode's **Inspect native equip / unlock contracts** exports read-only function metadata without invoking those functions or editing saves.

### RuneSchema authoring workflow

Open RuneSchema Settings, then **Authoring & Tools**. If the tools are off,
enable **Mod authoring tools** under **General > Runtime** and restart. When
the game is ready, choose **Activate Tools for this session**. Activation is
temporary; it does not become a permanent setting.

The **Workflow stage** menu contains Mod Authoring, Presets, Traces, Inspector,
Results, and Save Cleanup. **Previous stage** and **Next stage** move through
that order. Recent results are at the bottom of the page. Save Cleanup is last
because it is a separate offline operation, not part of creating a mod.

## Create an editable starter

1. Choose **Mod Authoring** in Workflow stage.
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
the Items page also points to Helpy's Transmutate workspace for in-world item authoring.

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
