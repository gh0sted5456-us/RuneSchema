# Bigger chests, line by line

This is a focused Blueprint/component-default edit based on the author's
**Increased Storage - 3** mod. It does not create a new chest type, a building
clone, or an inventory item. Existing chest classes retain their game identity.

Start with [FModel to mod](fmodel-to-mod.md) if export wrappers are unfamiliar.

## Find the actual inventory component

Inspect the chest's Blueprint/defaults and its referenced components. The field
we want belongs to `BP_Components_WorldItemInventory`, not to the building's
ItemData or a crafting recipe. The installed ash-chest rule targets
`BP_BaseBuilding_Chest_Small_C`.

The [teaching export](../examples/Walkthroughs/exports/ash-chest.fmodel.jsonc)
shows an export record with `Type`, `Name`, `Outer` and `Properties`. Preserve
that as a reference file. The `24` before-value in that fixture is illustrative;
read your current game rather than assuming it is the vanilla ash-chest size.

## Create the smallest working file

Create `RuneSchema/mods/BiggerChests/blueprints/10-ash-chest.jsonc`:

```jsonc linenums="1"
--8<-- "docs/examples/Walkthroughs/BiggerChests/blueprints/10-ash-chest.jsonc"
```

| Line | Explanation |
| --- | --- |
| 1 | Open the loader document object. |
| 2 | Select the existing ash-chest Blueprint-generated class by its verified short class name. |
| 3 | Follow its inventory component. These fields do not go directly on the chest. |
| 4 | Set capacity to 56 slots; do not write `"56"` as a string. |
| 5 | Enable the separate native sort/fill-stacks flag. Omit this field if you only intend a capacity change. |
| 6–8 | Close the component, class and document objects. |

The short class name above is the form in the author's working configuration.
When more than one package defines that name, inspect and use the exact
supported `/Game/.../BP_Name.BP_Name_C` target rather than choosing an arbitrary
first match. Do not invent a full path from the human-readable chest name.

## Expand deliberately to other chest types

The other installed rules use separate classes:

| Container | Class in the author's file | Configured slots |
| --- | --- | ---: |
| Crate | `BP_BaseBuilding_Crate_C` | 40 |
| Ash/small chest | `BP_BaseBuilding_Chest_Small_C` | 56 |
| Oak chest | `BP_BaseBuilding_Chest_C` | 72 |
| Iron chest | `BP_BaseBuilding_Chest_Iron_C` | 88 |

These are **mod values**, not a table of vanilla defaults. The companion file
changes only the other three chest classes:

```jsonc title="blueprints/20-other-chests.jsonc" linenums="1"
--8<-- "docs/examples/Walkthroughs/BiggerChests/blueprints/20-other-chests.jsonc"
```

A file may contain multiple class keys. Separate files can be easier to disable
while diagnosing a particular container. Do not target a shared inventory
component class globally unless you intend to affect every consumer of it.

## What about lumber and ore storage?

The author's lumber and ore rules additionally provide `AllowedItemFilterQuery`.
Its `TagDictionary` and `QueryTokenStream` are an encoded native query, not a
list of friendly labels. For a **capacity-only** mod, omit the whole query so
the original item restrictions remain unchanged.

If you intentionally change allowed contents, inspect the complete query and
validate it in the game. Editing only `UserDescription` changes a description,
not the predicate. Copying a different storage object's query can change what
the container accepts. Do not publish an arbitrary token stream as a universal
"allow everything" recipe.

## Enable and test

1. Keep `RuneSchema : 1` in UE4SS's `Mods/mods.txt`.
2. Add `BiggerChests : 1` in `RuneSchema/mods/runeschema.txt`.
3. Close and restart Dragonwilds; do not rely on a live default edit.
4. Check **Helpy → Diagnostics** for the mod and missing class/component/field
   messages. Confirm you edited the active installation, not another store's copy.
5. Place a fresh test chest, inspect its capacity and use its final slots.
6. Separately check a chest already present in the backed-up world. Existing
   serialized instance state need not behave like a newly constructed default.
7. Test sorting, quick transfer, save/reload and full game restart.
8. For multiplayer, use matching gameplay files on the authority and relevant
   clients; test with two players using the same chest.

## Updates and safe removal

**Empty extra slots before reducing capacity or removing this mod.** Do not
assume hidden overflow slots will be recovered automatically. Preserve a world
backup before testing any capacity reduction. Increasing a number is not proof
that the UI, serialization and network paths all support that number.

If another mod changes the same field, use a clearly named compatibility mod
after both providers and document the chosen capacity. A full copied export
makes that conflict harder to see; this two-field patch makes it explicit.

Next: [patch selected inventory pages](bigger-inventory.md), or
[learn replacement/append/targeted patching](append-and-targeted-patches.md).
