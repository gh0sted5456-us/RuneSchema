# Journal loader

**Folder:** `RuneSchema/mods/<ModName>/journal/` (`/lore/` accepts Lore entries only).

The loader creates a RuneSchema-owned journal asset or registers an existing cooked one, then places it in a native subcategory. Registration, placement, and **player discovery** are separate steps. A successful loader summary does not prove that a player has unlocked an entry.

## Create an entry

Use a stable, mod-specific key. `Type` selects the native asset class: `Lore`, `People`, `Place`, `Treasure`, `World`, or `Recipe`. `AddTo` selects its journal location.

```json
{
  "my_mod:field_notes": {
    "Type": "Lore",
    "DisplayName": "Field Notes",
    "PageDescriptions": [{"Description": "A first page."}],
    "Unlock": false,
    "AddTo": {"Section": "Knowledge", "Category": "Lore Scraps"}
  }
}
```

`Unlock: false` is appropriate when a gameplay event should discover the entry later. `Unlock: true` requests an immediate native player unlock, but the game can reject that request. Check the log and in-game journal; placement alone is not discovery. A generic building-placement or NPC-dialogue unlock trigger is not yet verified.

## Register a cooked entry

Use its full cooked object path as the JSON key. The asset retains its native class, identity, pages, icon, and `UnlockCondition`. Optional `PersistenceID` and `InternalName` fields assert the cooked identity; RuneSchema will not replace it. Do not create a second runtime entry for the same page.

```json
{
  "/MyMod/Journal/JOURNAL_FieldNotes.JOURNAL_FieldNotes": {
    "Type": "Lore",
    "PersistenceID": "actual-cooked-persistence-id",
    "Unlock": false,
    "AddTo": {"Section": "Knowledge", "Category": "Lore Scraps"}
  }
}
```

The ID above is a placeholder; inspect your cooked asset and keep its shipped ID stable. Cooked entries can use the native player-unlock path without RuneSchema's transient-save adapter. Native `UnlockCondition` has types `PlayerStart`, `ItemFirstPickup`, `RecipeUnlocked`, `Interaction`, `AIKill`, `RegionUnlocked`, `Manual`, and `NeverUnlock`. These are asset fields, not a guarantee that a mod event invokes them. `Manual` in particular still requires a successful authoritative runtime unlock.

## Choose a section

`AddTo` accepts these friendly native routes:

| Section | Categories |
| --- | --- |
| World | Flora, Fauna, Material Sources, Factions |
| Recipes | Tools, Weapons, Armor, Ammo & Runes, Trinkets, Masterworks, Food & Potions, Basic Materials, Processed Materials, Special Materials, Quest Items, Capes |
| Knowledge | Tutorials, Lore Scraps, People, Places |

For a loaded native subcategory outside this list, use `"AddTo": {"SubCategory": "/Game/.../ExactCategory.ExactCategory"}`. `AddTo.Key` optionally chooses the key within the category. `AddTo.Group` may select a group or create one where the native category supports it:

```json
"AddTo": {
  "Section": "Knowledge",
  "Category": "People",
  "Group": {"Id": "MyMod_Craftsmen", "DisplayName": "Craftsmen", "CreateIfMissing": true}
}
```

Use a native type appropriate for the destination. A recipe-journal entry also requires a resolvable `RecipeData`, an `ItemData` matching one of that recipe's outputs, and a valid crafting `StationTableRowHandle`. `UnlockOnAcquire: true` is supported for an entry with `ItemData`; when `Unlock` is omitted it suppresses the default immediate unlock.

Keep keys and persistence IDs stable after release so saved discovery state survives updates. Safe Clean removes orphaned RuneSchema identities when supplying content is removed; it cannot turn a rejected native unlock into a successful one.

[← All loaders](../../LOADER-WALKTHROUGHS.md) · [Authoring guide](../../AUTHORING-GUIDE.md)
