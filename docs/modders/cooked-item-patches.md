# Update a cooked item without recooking it

Use this when the item already exists and you want a small reflected-data
change, not a new item. The provider PAK stays unchanged. RuneSchema applies
the authored override to the loaded object; it does not rewrite the PAK on disk.

For a mod-owned cooked item, install/enable its provider first and use its real
mount root. A loose folder named `MyMod` does not move an item from `/OtherPak/`.

## 1. Inspect the item, not its icon or mesh

Find the ItemData object in FModel and inspect the fields it actually owns.
Follow referenced row handles when the desired value lives in a DataTable.
For example, a weapon's damage may be in a referenced stat row rather than on
the item. A property export's name does not tell you every consumer's behavior.

This reconstructed teaching input shows the relevant export shape. The
starting stack count is illustrative, and the internal name is deliberately
marked as not for copying:

```jsonc title="Research input — not a loader file" linenums="1"
--8<-- "docs/examples/Walkthroughs/exports/ash-wood.fmodel.jsonc"
```

The real object path used below is the ash-wood reference also used by the
author's ResourcesAndLoot mod. Confirm it and `MaxStackSize` on your build.

## 2. Keep only the change

Create `RuneSchema/mods/PatchWood/assets/10-stack-size.jsonc`:

```jsonc linenums="1"
--8<-- "docs/examples/Walkthroughs/PatchWood/assets/10-stack-size.jsonc"
```

| Line | Explanation |
| --- | --- |
| 1 | The asset loader document is an object. |
| 2 | A readable label for this patch entry. This label does not create or rename an asset. |
| 3 | `$Patch` gives the exact existing item object path. |
| 4 | `$Target` contains the fields being changed. |
| 5 | `MaxStackSize` is a number on that item type. The example chooses 100. |
| 6–8 | Close the changes, labeled patch and document. |

We intentionally omitted the export's `Type`, `Name`, `Properties` and
`InternalName`. Most importantly, **do not generate or replace the existing
item's PersistenceID**. The explicit item-patch envelope protects
`PersistenceID` and `InternalName` from being changed. If you need a different
item identity, use [a clone](item-clones.md) instead.

## 3. Understand the shorter legacy form

For supported ordinary asset writes, the full object path can also be the key:

```jsonc
{
  "/Game/Gameplay/Items/Resources/Wood/ITEM_Resources_Wood_Ash.ITEM_Resources_Wood_Ash": {
    "MaxStackSize": 100
  }
}
```

Use one form, not both. The explicit `$Patch` form communicates "change existing
content" clearly. Neither form makes an unsupported field writable, changes a
mesh's topology, repairs broken cooked Blueprint logic, or guarantees an
already-created instance will immediately refresh its cached state.

## 4. Patch a mod-owned cooked item

The same procedure applies to an existing ItemData from another mod:

1. Find its **actual** `/Mount/Folder/Item.Item` path in the cooked package.
2. Confirm the provider is enabled and the item has completed registration.
3. Change the patch's `$Patch` path and use fields of that native item class.
4. Put the override mod after the provider in `runeschema.txt`.
5. Document the dependency and whether an update changes the expected fields.

This is different from [enrolling an older cooked PAK](first-cooked-item.md):
registration makes an existing identity available to its supported native
system; a field patch modifies data. Some mods need both, but an empty
registration declaration is not a stats override.

## 5. Test consequences and removal

Enable `PatchWood : 1`, restart, inspect Diagnostics, and obtain the **original**
item. Confirm its identity is unchanged and test new stacking, transfers,
crafting inputs, save/reload and multiplayer. Existing oversized stacks may not
be automatically split when a later version lowers the limit.

Before reducing stack limits or removing a stack-size mod, use a backup and
make affected stacks fit within the restored limit. RuneSchema's orphan cleanup
is not an automatic stack-splitting migration. If another mod patches the same
field, resolve the conflict explicitly rather than shipping a full export.

Next: [create a separate item](item-clones.md) or
[patch only one array member](append-and-targeted-patches.md).
