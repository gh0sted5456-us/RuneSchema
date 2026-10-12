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

The ID above is a placeholder; inspect your cooked asset and keep its shipped ID stable. Cooked entries can use the native player-unlock path without RuneSchema's transient-save adapter.

## Native discovery conditions

`UnlockCondition` is a reflected native struct. RuneSchema accepts it for a journal entry and validates the required cooked path for the four trigger types confirmed in vanilla assets:

| `UnlockType` | Required field | Meaning |
| --- | --- | --- |
| `ItemFirstPickup` | `UnlockingItemData` | First pickup of the exact item |
| `RecipeUnlocked` | `UnlockingRecipe` | Discovery of the exact crafting recipe |
| `Interaction` | `UnlockingInteractionActor` | Interaction with the exact actor class |
| `AIKill` | `AIData` | Kill of the exact AI data type |

Use full cooked object paths, including the object name. For example:

```json
{
  "/MyMod/Journal/JOURNAL_Pigment.JOURNAL_Pigment": {
    "Type": "World",
    "Unlock": false,
    "UnlockCondition": {
      "UnlockType": "EJournalEntryUnlockType::ItemFirstPickup",
      "UnlockingItemData": "/MyMod/Items/ITEM_Pigment.ITEM_Pigment"
    },
    "AddTo": {"Section": "World", "Category": "Material Sources"}
  }
}
```

Native assets also expose `PlayerStart`, `RegionUnlocked`, `Manual`, and `NeverUnlock`. `RegionUnlocked` has no confirmed donor in the current asset audit, so its target fields are not yet prescribed here. Conditions are data, not proof that an event will be indexed or fired for a late-registered mod entry. Test a fresh event, save/re-entry, and multiplayer authority before relying on a condition. `Manual` requires a successful native player unlock; it is not an automatic station-placement or NPC-talk trigger. When you provide `UnlockCondition`, RuneSchema no longer requests an immediate unlock by default; explicit `Unlock: true` still overrides that default.

Recipe availability is separate from journal discovery. To make a crafting recipe earnable, set that `/recipes` definition's `Unlock` to `false` and provide a verified game progression rule that grants the recipe. A `RecipeUnlocked` journal condition can then reveal its page. A visible journal page alone does not grant the crafting recipe.

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
