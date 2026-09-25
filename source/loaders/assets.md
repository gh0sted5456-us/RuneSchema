# Assets loader

**Folder:** `RuneSchema/mods/<ModName>/assets/`

Items, icons, stats, unlock links, DataAssets, and reflected object patches.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

Use `/assets` to patch a loaded item or clone a compatible item into a new
cooked path.

```json
{
  "/Game/MyMod/Items/ITEM_Test.ITEM_Test": {
    "$Clone": "/Game/Gameplay/Items/ITEM_Source.ITEM_Source",
    "PersistenceID": "stable-unique-id",
    "InternalName": "mymod_test",
    "Name": "Test Item",
    "PowerLevel": 20
  }
}
```

Walkthrough:

1. Choose a source with the same native item type and behavior you need.
2. Assign a unique destination path, `PersistenceID`, and `InternalName`.
3. Override only reflected fields that exist on the source type.
4. Add `/raw` rows when the item points to row handles such as wearable data.
5. Add `/recipes`, `/journal`, or `/vendors` references after the item path is
   stable.

`$DominionSpheres` patches validated named sphere subobjects. `$VisualEffect`
can attach a supported equip effect. `RecipesToUnlock` and
`BuildingPieceToUnlock` are inherited by a clone unless explicitly replaced.

Reference: `examples/RSv16/ExampleMods/RuneSchema2VendorTest/assets`.

## Direct `DA_` fields

For an existing DataAsset or generated-class default, put the complete `DA_`
path first and native field paths below it. RuneSchema infers a class default
object from `_C`; the mod directory supplies ownership.

```json
{
  "/Game/UI/MainMenu/CharacterCreate/Data/DA_CharacterOptionData.DA_CharacterOptionData_C": {
    "CharacterOptions[FacialHairPreset].OptionData": {
      "$MergeWhere": {
        "Field": "DataHandle.RowName",
        "Values": ["F_A_PresetNone", "M_A_PresetNone"],
        "Value": {
          "BodyTypeCompatability": "both",
          "FaceTypeCompatibility": "all"
        }
      }
    }
  }
}
```

Direct scalar values perform `Set`; direct objects perform `Merge`. `$Set` and
`$Merge` make that choice explicit. `$Append` adds typed array members.
`$AppendUnique` adds only when `$Identity` does not already exist; character
options infer `DataHandle.RowName` and their native template. `$MergeWhere`
updates existing struct-array entries selected by `Field` and `Values`. It
requires every selector to match exactly one entry and validates every changed
field before committing the group. A missing or duplicate beard row therefore
rejects that grouped edit instead of changing an arbitrary option.

The active MoreHair menu file motivated this form: its former registry envelope
repeated target, transaction, profile, and mod identity for every option. In the
current form, the target is written once, each field is written once, and only
the values that differ remain. Legacy envelope documents are still translated
internally so installed mods are not forced to migrate immediately.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
