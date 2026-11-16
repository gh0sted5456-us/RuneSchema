---
channel: experimental
promote_to: COOKED-TABLE-EXTENSIONS.md
---

!!! info "Experimental · 0.8.1.0"
    This guide describes the experimental branch. For the current public package, see
    [stable downloads](../CURRENT-RELEASE.md). [Experimental overview](index.md).

# Extend a vanilla table from a cooked PAK

This development path lets a PAK supply new rows without shipping a replacement
copy of a vanilla DataTable. The candidate has parser/schema checks and compiles;
its native import and each table's gameplay consumers still need an isolated
cooked fixture before release.

Mounting a different package does not merge its rows into a vanilla table.
Shipping the vanilla package path can replace that asset, including other mods'
changes. Use an explicit source table instead.

## Cook the extra rows

1. Find the destination table in FModel. Record its exact object path, row names
   and row struct. If it is a CompositeDataTable, identify the plain parent table
   that actually supplies the rows.
2. Create a **plain DataTable with the exact same row struct** in your own mod.
   Include only your additional rows. Give their names a mod-specific prefix.
3. Cook that table and everything its rows reference into your PAK.
4. Declare the source and destination below. The paths are illustrative: replace
   them with the full paths from your own assets.

```json
{
  "SchemaVersion": 1,
  "Entries": [],
  "LoaderDefinitions": {
    "DataTables": {
      "/Example/Tables/DT_Extra.DT_Extra": {
        "Target": "/Game/Tables/DT_Native.DT_Native"
      }
    }
  }
}
```

Put this document in the owning cooked registry declaration, or ship it as
`RuneSchema/mods/Example/registry/10-tables.jsonc` without recooking an existing
PAK. This route currently requires the source under `/Example/` or
`/Game/Mods/Example/` for owner `Example`; the destination must be an exact
`/Game/...Asset.Asset` path. Keep the `raw` loader enabled and restart after
changing declarations.

## Import first, edit with JSON afterward

RuneSchema resolves declared tables once during the existing table-loader
finalization and on relevant table serialization. It copies missing rows through
Unreal's native table API, then runs the usual loose JSON edits. Existing target
rows win a name collision; the source table stays unchanged. There is no
`Replace`, `Unlock` or implicit overwrite option in this declaration.

For example, if your cooked source adds row `Example_Bonus`, a subsequent
`raw/20-bonus.jsonc` may contain:

```json
{
  "$Patch": "/Game/Tables/DT_Native.DT_Native:Example_Bonus",
  "$Target": {
    "YourActualField": 2
  }
}
```

Use an actual reflected field from that row. Follow the
[raw-table reference](reference/loaders/raw.md) for the supported `$Patch`
envelope and `$append` syntax; registering a row does not create its gameplay
relationships or guarantee that a consumer has refreshed an earlier cache.

## Checks and limits

- Source and target must be plain DataTables with the same native row struct.
  Directly mutating a composite is refused; select its editable leaf instead.
- A source has at most 4,096 rows. Inline staging has a 16 MiB budget, and every
  reflected field must fit its row struct. This is not a total bound on nested
  allocations inside trusted cooked data.
- Rows are staged before import, checked after copying, and rows added by a
  failed import are removed. Independent declarations and ordinary JSON edits
  continue after a rejection.
- A live source/target pair is not repeatedly imported. Removing a mod does not
  remove its rows from an already-running session; restart to rebuild content.
- Test native lookups, composite refresh, the actual consuming menu/action,
  save/reload and client/server behavior. Tables consumed before RuneSchema's
  loading boundary need a dedicated adapter; do not promise universal merging.

See [registry declarations](COOKED-PAK-REGISTRY-MANIFEST.md) and
[targeted patches](../modders/append-and-targeted-patches.md).
