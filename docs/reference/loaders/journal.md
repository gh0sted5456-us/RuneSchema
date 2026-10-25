# Journal loader

## Step-by-step workflow

Examples with placeholder paths/IDs are templates. Use inspected values and the
[shared testing checklist](../../modders/testing-and-compatibility.md) before sharing.

1. Choose the native journal Type and its World, Recipes or Knowledge placement; separate registration from player discovery.
2. Create `journal/10-camp-notes.jsonc` using a stable mod-specific key, or reference one exact cooked entry.
3. Replace text, art and AddTo values in the mock-up. Choose Unlock or a supported discovery condition/trigger deliberately.
4. Restart and confirm placement, unlock, read state and reload independently. A cached preview is not the player's unlock receipt.

**Tie-ins:** [Lore](lore.md), [NPC JournalEntries](npc.md), [recipes](recipes.md) and [cache bundles](../../CACHE-BUNDLES.md).


**Folder:** `RuneSchema/mods/<ModName>/journal/` (`/lore/` accepts Lore entries only).

The loader creates a RuneSchema-owned journal asset or registers an existing cooked one, then places it in a native subcategory. Registration, placement, and **player discovery** are separate steps. A successful loader summary does not prove that a player has unlocked an entry.

## Create an entry

Use a stable, mod-specific key. `Type` selects the native asset class: `Lore`, `People`, `Place`, `Treasure`, `World`, or `Recipe`. `AddTo` selects its journal location.

```jsonc
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

`Unlock: false` is appropriate when a gameplay event should discover the entry later. `Unlock: true` requests a native player unlock once the owning player's journal component is ready. The loader retries automatic delivery on player inventory activity and RuneSchema NPC interaction, without treating placement as an unlock receipt. A rejected native unlock remains an error; RuneSchema does not force-write the player's unlocked-entry array.

## Discover an entry when talking to an NPC

In RuneSchema 0.8.0.0, an `/npc` definition can add `JournalEntries` alongside its normal shop or dialogue:

```jsonc
"JournalEntries": [
  "field_notes",
  "OtherMod:craftsman",
  "/MyMod/Journal/JOURNAL_FieldNotes.JOURNAL_FieldNotes"
]
```

References can identify a local `/journal` or `/lore` definition, a `Mod:Id`, or a full cooked journal object path. The list accepts 1..32 unique references. RuneSchema asks the native journal component belonging to the interacting player to unlock each registered entry. An already-unlocked entry stays unlocked and is not marked unread again. Unlock failure does not stop the NPC's ordinary dialogue or shop. `NoInteract: true` suppresses this binding.

Use `Unlock: false` on an entry that should be discovered only through this interaction. Its native type and `AddTo` still determine the journal section; `JournalEntries` does not move entries or replace their identities. A cooked Blueprint may instead invoke the game's native `JournalComponent.UnlockJournalEntry` with that same registered entry and owning player component. The mod owns that gameplay trigger; RuneSchema handles mounting, registration and any JSONC override. Building placement does not implicitly unlock journals.

## Register a cooked entry

A PAK can announce entries without duplicate loose pointer files through
`NativeRegistries.Journals` / `NativeRegistries.Lore`, or carry the ordinary
loader body in `LoaderDefinitions.Journals` / `LoaderDefinitions.Lore`.
See [cooked PAK declarations](../../COOKED-PAK-REGISTRY-MANIFEST.md).
Declaration-only entries default to `Unlock: false`; native conditions and
explicit gameplay triggers remain responsible for player discovery. Loose
definitions for the same cooked path still override the in-PAK metadata.

Use its full cooked object path as the JSON key. The asset retains its native class, identity, pages, icon, and `UnlockCondition`. Optional `PersistenceID` and `InternalName` fields assert the cooked identity; RuneSchema will not replace it. Do not create a second runtime entry for the same page.

```jsonc
{
  "/MyMod/Journal/JOURNAL_FieldNotes.JOURNAL_FieldNotes": {
    "Type": "Lore",
    "PersistenceID": "actual-cooked-persistence-id",
    "Unlock": false,
    "AddTo": {"Section": "Knowledge", "Category": "Lore Scraps"}
  }
}
```

The ID above is a placeholder; inspect your cooked asset and keep its shipped ID stable. Cooked entries can use the native player-unlock path without RuneSchema's generated-entry persistence adapter.

Unlocked entries, including automatically granted generated entries, retain
their native character progress between sessions. Already unlocked entries
are not re-marked unread. Safe Clean removes unresolved orphan entries when
their content is disabled or removed, rather than clearing active entries
on every save.

## Native discovery conditions

`UnlockCondition` is a reflected native struct. RuneSchema accepts it for a journal entry and validates the required cooked path for the four trigger types confirmed in vanilla assets:

| `UnlockType` | Required field | Meaning |
| --- | --- | --- |
| `ItemFirstPickup` | `UnlockingItemData` | First pickup of the exact item |
| `RecipeUnlocked` | `UnlockingRecipe` | Discovery of the exact crafting recipe |
| `Interaction` | `UnlockingInteractionActor` | Interaction with the exact actor class |
| `AIKill` | `AIData` | Kill of the exact AI data type |

Use full cooked object paths, including the object name. For example:

```jsonc
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

`UnlockType` accepts the eight native values `PlayerStart`, `ItemFirstPickup`, `RecipeUnlocked`, `Interaction`, `AIKill`, `RegionUnlocked`, `Manual`, and `NeverUnlock`, either bare or prefixed with `EJournalEntryUnlockType::`. Unknown values and other enum prefixes are rejected. The four targeted conditions above require bounded cooked references; those references are also checked against the live reflected fields when applied. `RegionUnlocked` remains native-only here: no synthetic target field or substitute progression rule is invented.

Conditions are data, not proof that an event will be indexed or fired for a late-registered mod entry. Registration and discovery are separate contracts. Item-acquisition delivery only includes entries that completed registration/placement and uses live object identity rather than stale raw pointers. Native unlock requests still require the owning player's queried state to confirm success. `Manual` requires an explicit trigger such as `JournalEntries` on a RuneSchema NPC or the mod's own cooked gameplay event. When you provide `UnlockCondition`, RuneSchema no longer requests an immediate unlock by default; explicit `Unlock: true` overrides that default. `NeverUnlock` has no automatic discovery behavior.

Recipe availability is separate from journal discovery. To make a crafting recipe earnable, set that `/recipes` definition's `Unlock` to `false` and provide a verified game progression rule that grants the recipe. A `RecipeUnlocked` journal condition can then reveal its page. A visible journal page alone does not grant the crafting recipe.

## Choose a section

`AddTo` accepts these friendly native routes:

| Section | Categories |
| --- | --- |
| World | Flora, Fauna, Material Sources, Factions |
| Recipes | Tools, Weapons, Armor, Ammo & Runes, Trinkets, Masterworks, Food & Potions, Basic Materials, Processed Materials, Special Materials, Quest Items, Capes |
| Knowledge | Tutorials, Lore Scraps, People, Places |

For a loaded native subcategory outside this list, use `"AddTo": {"SubCategory": "/Game/.../ExactCategory.ExactCategory"}`. `AddTo.Key` optionally chooses the key within an ungrouped category. Grouped categories use the entry asset's native object name consistently in their list, detail lookup, and hierarchy index; an authored `Key` alias does not replace that native key or change the entry's persistence identity. `AddTo.Group` may select a group or create one where the native category supports it:

```jsonc
"AddTo": {
  "Section": "Knowledge",
  "Category": "People",
  "Group": {"Id": "MyMod_Craftsmen", "DisplayName": "Craftsmen", "CreateIfMissing": true}
}
```

Use a native type appropriate for the destination. A recipe-journal entry also requires a resolvable `RecipeData`, an `ItemData` matching one of that recipe's outputs, and a valid crafting `StationTableRowHandle`. `UnlockOnAcquire: true` is supported for an entry with `ItemData`; when `Unlock` is omitted it suppresses the default immediate unlock.

Keep keys and persistence IDs stable after release so saved discovery state survives updates. Safe Clean removes orphaned RuneSchema identities when supplying content is removed; it cannot turn a rejected native unlock into a successful one.

[← All loaders](../../LOADER-WALKTHROUGHS.md) · [Authoring guide](../../AUTHORING-GUIDE.md)
