# Create a separate item with $Clone

Use `$Clone` when you want a **different item** that reuses compatible existing
content. Use [an item patch](cooked-item-patches.md) when you want to change the
original item instead. Capitalization is exact: `$Clone`, not `$clone` or
`$CloneFrom`.

A RuneSchema item clone is a runtime ItemData copy, not a newly cooked Unreal
package. It cannot manufacture a mesh, texture, animation, class or Blueprint
behavior that does not already exist. Building clones use their own
[`buildings` contract](../reference/loaders/buildings.md), not this ItemData format.

## 1. Choose a compatible donor

Inspect a working native or mounted, registered ItemData of the kind you need.
This lesson uses the ash-wood item referenced by the author's ResourcesAndLoot
mod. A resource is a straightforward way to learn identity and inheritance
without implying that a weapon can acquire a new attack family by renaming it.

Inherited fields still matter. An equipment item may point to shared stat rows,
wearable data, recipes or a held actor. Copying the ItemData does not necessarily
deep-clone those dependencies. Changing a shared referenced row can still alter
the original donor and other consumers.

## 2. Create a stable mod and item identity

Use the folder `CopyWood`. Generate **one** new canonical 22-character
PersistenceID in Helpy's authoring workflow and keep it unchanged after release.
Do not copy the tutorial's demonstration ID into several published mods.

An offline way to generate the same URL-safe, unpadded 16-byte identity form is:

```powershell
[Convert]::ToBase64String([Guid]::NewGuid().ToByteArray()).TrimEnd('=').Replace('+','-').Replace('/','_')
```

Run it once when creating the item—not at startup or on every update. A display
name is not a persistence identity. Do not reuse the donor's ID or internal name.

## 3. Write the clone file

Create `RuneSchema/mods/CopyWood/assets/10-ash-variant.jsonc`:

```jsonc linenums="1"
--8<-- "docs/examples/Walkthroughs/CopyWood/assets/10-ash-variant.jsonc"
```

| Line | Meaning |
| --- | --- |
| 1 | Replace the demo identity before publishing. The comment is valid JSONC. |
| 2 | Open the assets document. |
| 3 | Name the clone using the runtime path derived from mod folder `CopyWood` and internal name `copywood_ash`. |
| 4 | `$Clone` identifies the compatible existing ItemData donor. It is not a filename on your disk. |
| 5 | A new canonical PersistenceID is the clone's permanent saved identity. |
| 6 | A unique, stable InternalName is separate from the friendly display name. |
| 7 | `Name` changes the player-facing name; it does not rename the donor. |
| 8 | Only this clone's item-owned stack limit is overridden. Other inherited fields remain. |
| 9–10 | Close the clone and document. |

The runtime constructs the clone at
`/Game/RuneSchema/<sanitized-mod-folder>/Items/<sanitized-InternalName>.<sanitized-InternalName>`.
Use a simple stable folder/internal name so the written target and constructed
path agree. Confirm the **actual registered path** in Diagnostics/Helpy before
using it in other files. Renaming the mod folder can change this path.

## 4. Make the new item obtainable

Enable `CopyWood : 1` and restart. Search Helpy for **Workshop Ash Wood**, inspect
its path/identity, then explicitly give it to a disposable test character.
Selecting or inspecting an item does not deliver it.

A clone does not automatically receive a crafting recipe, vendor offer or
loot entry. To distribute it through normal gameplay, reference its actual
runtime path from [a recipe](../STATIONS-AND-RECIPES.md),
[a vendor](../reference/loaders/vendors.md), or the
[loot example](creature-drops.md). Put dependencies in the appropriate loaders;
do not paste a recipe envelope into an item-property object.

## 5. Verify that it is genuinely separate

Test the donor and clone side by side:

1. They should retain different PersistenceIDs and internal names.
2. The donor's stack limit/name should not have been changed by this file.
3. The clone should keep its intended appearance and native behavior.
4. Transfer it through inventory/storage, save, restart and load again.
5. Test crafting/loot/vendor consumers and a matching client/server mod set.

For weapon or armor clones, also test attacks, repair, equip/unequip and any
shared stat-row dependencies. A rendered icon is not combat or repair validation.

## Updates and removal

Keep the folder, InternalName, PersistenceID and downstream references stable.
Changing an ID to avoid a conflict is not a safe migration for an already
released item. Resolve duplicate ownership with the other author instead.

When the mod is truly absent, supported orphan cleanup can remove its unresolved
saved items after a verified backup. Reinstalling the mod does not automatically
restore removed possessions. Read [save safety](../SAFE-SAVE-AND-LEDGER.md) and
test removal only on a backup—not on a player's only save.
