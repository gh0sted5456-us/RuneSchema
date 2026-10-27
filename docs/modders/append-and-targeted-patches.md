# Replacement, $Append and targeted patching

Choose the patch form that matches the loader you are using. RuneSchema supports
several authoring contexts, and each one has its own supported directives. This page distinguishes the forms
used by the [loot](creature-drops.md), [item](cooked-item-patches.md),
[clone](item-clones.md) and [inventory](bigger-inventory.md) tutorials.

## Start with the operation you intend

| Intent | Typical form | Result |
| --- | --- | --- |
| Change a scalar on an existing target | `"MaxStackSize": 100` | Write that supplied field. |
| Preserve unspecified reflected struct fields | A partial nested object | Update supplied supported fields; do not assume arbitrary shared UObjects are privately cloned. |
| Replace an array | `"Resources": [ ... ]` | The supplied list replaces the array; `[]` empties it. |
| Add array entries in classic raw/assets/Blueprint writes | `"$Append": {"Resources": [ ... ]}` at the owning object/row level | Append typed entries, without deduplicating them. |
| Change selected struct-array entries | `"Resources": {"$Patch": [{"$Match": {...}, "$Target": {...}}]}` | Select existing entries, then change supplied entry fields. |
| Change an existing raw row only | `{"$Patch": "Table:Row", "$Target": {...}}` | Do not create a missing row. |
| Change an existing item only | A labeled assets entry containing `$Patch` path and `$Target` fields | Preserve the existing identity; do not create a clone. |
| Create a different compatible ItemData | `$Clone` plus new stable identity | Register a separate runtime item. |

Directives are case-sensitive: **`$Append`, `$Patch`, `$Match`, `$Target`,
`$Index`, `$Clone`**. Lowercase `$append` is not an alias. Native names such as
`MaxSlotCount` and `SpawnedItemData` must also match the reflected contract.

## Two different meanings of $Patch

### A. Existing-record envelope

In `/raw`, `$Patch` is a **string** identifying `table:row`, and `$Target` is
the object of row changes:

```jsonc
{
  "$Patch": "/Game/Gameplay/Items/LootDropTables/DT_LootDropTable.DT_LootDropTable:AbyssalDemon",
  "$Target": {
    "$Append": {"Resources": []}
  }
}
```

This intentionally empty append illustrates placement of the keys; it adds
nothing. Use the [complete drop entry](creature-drops.md#3-write-an-additive-existing-row-only-patch)
for the actual lesson. A misspelled row should fail, not become a new empty row.

In `/assets`, the same existing-record idea goes **inside a labeled entry**;
the `$Patch` string is an object path, not a table/row pair. In `/blueprints`,
an existing-class patch uses the loader's class target. Recipe, quest, building
and other loaders have their own identity rules. Do not copy one envelope into
every loader and expect identical semantics.

### B. Struct-array field patch

At an array-valued reflected field, `$Patch` is an **array of edits**:

```jsonc
{
  "InventoryPages": {
    "$Patch": [
      {
        "$Match": {"SlotType": "EInventorySlotType::InventoryItems"},
        "$Target": {"NumSlots": 120}
      }
    ]
  }
}
```

This fragment belongs under the inspected inventory component/class. It keeps
all other pages and all other fields of the selected page. The loader stages
this array patch on a copy and publishes it only if its edits validate. That
does **not** promise a transaction covering every other file, property or loader
in your mod.

## Match by meaning, not position

Each array edit requires exactly one selector:

- `$Match`: a nonempty object of reflected fields that must identify exactly
  one struct entry. A missing or ambiguous match rejects the array patch.
- `$Index`: a nonnegative, zero-based index that must already exist. It does
  not append and is brittle when earlier mods insert/reorder entries.

Use `$Target` for the nonempty object of fields to change. Do not include both
selectors. Matching supports inspected scalar/enum/reference and nested struct
fields, not arbitrary array predicates. Reference fields in array matches or
targets use object-path strings (or null where the property accepts it), not
FModel export-reference wrappers. New entries need append or their own loader,
not a fabricated out-of-range index.

The [Bigger Inventory example](bigger-inventory.md) uses `SlotType`, so a page
insertion does not redirect an index-based patch onto the wrong page. The
[loot tuning example](creature-drops.md#5-tune-one-existing-drop-instead) selects
the item reference instead of assuming that the drop is element zero.

## The direct DA_ operation form is a separate context

For supported direct `DA_` DataAsset field-path documents in `/assets`, a field
can instead contain `$Set`, `$Merge`, `$Append`, `$AppendUnique` or `$MergeWhere`.
Here `$Append` is **inside the selected field**, not the classic row-level
`$Append` object above.

`$AppendUnique` requires a supported `$Identity` field; it is not a drop-list
operator you can insert into any `/raw` row. `$MergeWhere` uses `Field`, `Values`
and `Value`, not the array-field `$Match` syntax. See the
[`assets` reference](../reference/loaders/assets.md#direct-da_-fields) for the
character-menu example and its limited inference rules.

## Avoid broad compatibility claims

Focused patches reduce unnecessary conflicts; they do not make load order
irrelevant. Two mods setting the same scalar still compete. Two appends can
introduce duplicate drops. Two selectors can become ambiguous after a third
mod adds a matching entry. Preserve unknown/unrelated data, resolve dependencies,
then [test the complete combination](testing-and-compatibility.md).

Do not claim that all unknown properties are rejected atomically by all loaders:
strict patch contracts and legacy reflected writers have different boundaries.
Read the actual rejection and read back the result.
