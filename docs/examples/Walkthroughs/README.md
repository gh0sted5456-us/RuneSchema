# Worked tutorial files

These files accompany the [FModel-to-mod learning path](../../modders/fmodel-to-mod.md).
They are examples, not an additional shipped gameplay mod.

Use the file links in the [example library](../../EXAMPLES.md#worked-line-by-line-tutorials)
to save individual JSONC files. Preserve the shown mod/loader layout; downloading
this README alone does not install the examples.

| Folder | Purpose |
| --- | --- |
| `exports` | Reconstructed, trimmed FModel-shaped teaching inputs. Not complete or byte-for-byte game exports. Illustrative starting values are not promises about your game version. |
| `BiggerChests` | Focused chest defaults derived from the author's installed Increased Storage - 3 files. |
| `MoreAbyssalDrops` | One additive loot row, derived from the author's ResourcesAndLoot mod. |
| `TuneAbyssalDrops` | An alternative/later patch of exactly one existing drop. It requires that drop to exist exactly once. |
| `PatchWood` | Change an existing cooked item's stack limit without creating an item. |
| `CopyWood` | Create a resource-item variant. Generate your own PersistenceID before installing. |
| `BiggerInventory` | Selected page/default/UI rules from the author's Bigger Inventory configuration; not a universal inventory-size preset. |

Install only the lesson you are testing under `RuneSchema/mods/<folder>/`, enable
that folder in `RuneSchema/mods/runeschema.txt`, and restart the game. Do not copy
`exports` into a loader folder. An example is not proof that the corresponding
asset, row or field still exists in a later Dragonwilds build.

The tutorials deliberately keep unrelated fields and inherited defaults out of
patches. They explain when a normal array value replaces an array, when
case-sensitive `$Append` adds entries, and when `$Patch` selects an existing
record. Never install several alternative lessons unknowingly against the same
target. No saves, account data, PAKs, textures or executable files are included.
