# Add to a creature's drop list without replacing it

This example is based on the author's **ResourcesAndLoot** mod. Its
`DT_LootDropTable` edits use `$Append` under each creature's row. We will add
one resource-drop entry, then show a separate way to tune an existing entry.

This is a `raw` DataTable change. It is not a new creature, a spawn definition,
or an instruction to copy all of an exported loot table.

## 1. Trace the creature to the table and row it uses

In FModel, inspect the creature and follow its loot reference. The example uses:

- table: `/Game/Gameplay/Items/LootDropTables/DT_LootDropTable.DT_LootDropTable`;
- row: `AbyssalDemon`;
- array field: `Resources`;
- item: `/Game/Gameplay/Items/Resources/Animal/ITEM_Resources_Abyssal_Ashes.ITEM_Resources_Abyssal_Ashes`.

Those names are present in the author's installed files and the runtime's loot
lookup. They are not a promise that every creature uses this table, or that
every game build keeps the same rows. Variants such as `AbyssalDemon_Child` are
separate rows; changing one does not automatically change the other.

## 2. Understand the relevant export

This trimmed, reconstructed export-shaped fixture keeps the relevant table/row
shape and the drop fields from the author's file. It is **not** a complete or
verbatim vanilla drop table:

```jsonc title="Research input — do not install" linenums="1"
--8<-- "docs/examples/Walkthroughs/exports/loot-table.fmodel.jsonc"
```

The outer export list and `Type`/`Name` identify the exported table. `Rows`
contains row names; `AbyssalDemon.Resources` contains drop structs. For a mod,
replace those export wrappers with the actual loader's table/row target.

The exported `ObjectPath` ends in `.0`. Combined with `ObjectName`, it refers
to the named item export. Our hand-written patch uses the inspected complete
object-path string instead. Do not infer item identity from a display label.

## 3. Write an additive, existing-row-only patch

Create `MoreAbyssalDrops/raw/10-add-drop.jsonc`:

```jsonc linenums="1"
--8<-- "docs/examples/Walkthroughs/MoreAbyssalDrops/raw/10-add-drop.jsonc"
```

| Lines | Meaning |
| --- | --- |
| 2 | `$Patch` names one existing row as `full-table-object-path:RowName`. It must resolve; it is not a request to create a row if the name is misspelled. |
| 3 | `$Target` contains only the changes for that existing row. |
| 4–5 | `$Append.Resources` adds struct entries to the current array, preserving its existing entries. Capitalization matters. |
| 7 | `SpawnedItemData` references an existing, mounted and valid item. A missing item's name cannot manufacture it. |
| 8–9 | Minimum and maximum amounts belong to this added drop entry. Keep minimum no larger than maximum. |
| 10 | `DropChance` is the native field/value used in the source mod. Its unit must be established for this row type; it is not interchangeable with another struct's `ProbabilityOfDrop`. |
| 11 | The native per-player visibility option is deliberately false in this example. |
| 12 | The item is not automatically inserted into inventory by this option. |
| 13 | The native damage-participation restriction is deliberately false here. |
| Remaining lines | Close the new entry, append array and patch envelope. No other table rows are supplied. |

The source mod uses `5.625` for this field. This guide preserves that value
rather than claiming a measured drop percentage. Do not translate it to
`0.05625`, or promise a rate to players, without verifying the game's native
semantics and actual rolls. Resource destruction/consumable drop structs in
the same mod use different names such as `ProbabilityOfDrop` and `MinToDrop`.
Copying those field names into this row would target the wrong layout.

## 4. Know what append does—and does not do

`$Append` does **not** deduplicate by item path. If the row already contains the
same item, you may add another independent drop entry. Loading two mods that
append the same entry can do the same. Inspect the original and other enabled
mods before choosing append.

Do not replace the wrapper above with:

```jsonc
// Replacement example only: this discards other Resources entries.
{
  "Resources": []
}
```

A normal array assignment replaces that array. Supplying a shortened copied
list does not mean "only change the entries shown." An empty array means an
empty array, not "leave it alone."

## 5. Tune one existing drop instead

Use a separate, later compatibility mod when the entry already exists once:

```jsonc title="TuneAbyssalDrops/raw/10-tune-existing-drop.jsonc" linenums="1"
--8<-- "docs/examples/Walkthroughs/TuneAbyssalDrops/raw/10-tune-existing-drop.jsonc"
```

The outer `$Patch` still selects the table row. The inner `Resources.$Patch`
selects one struct-array member with `$Match.SpawnedItemData`. Its `$Target`
changes only that entry's amount fields, leaving its chance, flags and the
other drops unchanged. A missing or ambiguous match rejects that array patch;
it is not a fuzzy search or an add-if-missing operation.

For array matching, object references must use the supported path-string form,
not the whole FModel `ObjectName`/`ObjectPath` wrapper. If several entries use
the same item, add other stable, reflected scalar fields to the selector only
when they identify the intended entry unambiguously. Avoid `$Index` unless you
control the exact list and understand how earlier mods can reorder it.

Choose **add** or **tune** deliberately. If you test both example mods, place
`TuneAbyssalDrops` after `MoreAbyssalDrops` and ensure there is exactly one
matching entry. Do not treat the pair as an automatically compatible loot preset.

## 6. Verify a real drop, not just a loader summary

1. Enable the chosen folder in `mods/runeschema.txt` and fully restart.
2. Inspect Diagnostics for the exact table, row and property failures.
3. Confirm the creature variant actually consumes the row you edited.
4. Test the normal kill/loot path in a disposable world. A manually granted
   item proves availability, not that the loot system rolled it.
5. Check that original drops remain and that no duplicate entry was added.
6. Use enough trials to evaluate probability; one kill cannot establish a rate.
7. Test host/client ownership, visibility and inventory options separately.

The [`raw` reference](../reference/loaders/raw.md) covers full paths and row
creation. [Patch operators](append-and-targeted-patches.md) explains the two
meanings of `$Patch` used above and the different DataAsset operation form.
