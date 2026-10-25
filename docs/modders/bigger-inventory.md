# Bigger inventory: patch pages, then verify the UI

The author's **Bigger Inventory** mod illustrates a change that spans native
inventory data and interface layout. A larger inventory window is not more
storage. A larger storage count is not proof that the window can reach it.
Carry weight is a third, separate setting.

## Inspect the pages before writing a list

The native `InventoryPages` field is an array of structs. Each page has a
`SlotType` and a `NumSlots` in the example. A copied four-element array could
discard another page introduced by the game or a different mod. Instead,
select the intended entries by their enum value and change only `NumSlots`.

Do not assume `InventoryItems` is permanently element zero. Inspect the current
class/default, inherited values and the player controller's component as well
as the UI's inventory-body widgets.

## Read the author's focused rules

```jsonc title="BiggerInventory/blueprints/10-inventory-pages.jsonc" linenums="1"
--8<-- "docs/examples/Walkthroughs/BiggerInventory/blueprints/10-inventory-pages.jsonc"
```

| Part | Why it exists |
| --- | --- |
| `/Game/.../BP_Components_Inventory` | Configure the inspected inventory-component defaults. |
| `InventoryPages.$Patch` | Preserve the array and edit selected existing struct entries. |
| `$Match.SlotType` | Identify the intended page semantically. Enum spelling is part of the native contract. |
| `$Target.NumSlots` | Change only that page's slot count. |
| `MaxSlotCount` | A separate overall capacity field; it is not the UI's row count. |
| `BP_PlayerController → BP_Components_Inventory` | The author's profile also applies the same page rules to this owned component path. Inspect both consumers; do not assume changing a component class alone updates every existing owner. |
| `WBP_Inventory_VerticalNavigation → InventoryBody_*` | The author's page-specific UI row counts. This does not allocate gameplay slots. |

The four changed pages total **304** slots (`120 + 56 + 56 + 72`), while this
installed profile supplies `MaxSlotCount: 312`. The four snippets alone do not
explain the difference. Inspect unmodified/native pages and capacity semantics
before deriving a new preset; do **not** invent an eight-slot explanation or
call 312 the sum of those four values.

Likewise, the author's row counts correspond to an eight-column layout. If
your UI uses another layout, inspect it rather than copying those row counts
as universal constants. Keep any window-size/scrollbar adjustments in a
separate file so you can distinguish a storage failure from a display failure.

## What the selector guarantees

`$Match` must find exactly one matching entry. A missing page or duplicated
enum value causes that array patch to fail instead of silently editing a
different page. Only supplied fields change; other page entries are retained.
The array edit is staged before commit, not a guarantee that every other target
in the file changes as one transaction. See [patch operators](append-and-targeted-patches.md).

## Test each layer separately

1. Use a backed-up/disposable character and fully restart after enabling the mod.
2. Check Diagnostics for each class, component, `SlotType` and reflected field.
3. Verify the actual storage pages, not only the number of visible boxes.
4. Put disposable items in the last new slots on each page.
5. Test mouse/controller navigation, scrolling, drag/drop, sort/fill and transfers.
6. Save, quit completely, restart and verify the items and selected page again.
7. Test a second character and a matching host/client pair if you support co-op.
8. Test rollback only after removing everything from added slots.

Changing `NumSlots` does not change an item's stack limit or a character's
carry weight. The author's separate `players/00-GenericCarryCapacity.jsonc`
is outside this lesson; consult the [`players` contract](../reference/loaders/players.md)
instead of inventing a generic Blueprint carry-weight property.

This is a readable excerpt of an author's configuration, not live certification
of every new Dragonwilds build, screen size or inventory consumer. Capacity
reduction can strand items; prepare the character before disabling the mod.
