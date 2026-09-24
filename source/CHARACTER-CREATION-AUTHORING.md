# Character creation authoring

This records the RuneScape: Dragonwilds 5.6.1 character-creation contract
verified from the FModel export, runtime traces, and the working MoreHair mod.
RuneSchema extends the native menu; it does not replace it.

## Native menu source

The menu reads the class default object (CDO) for:

`/Game/UI/MainMenu/CharacterCreate/Data/DA_CharacterOptionData.DA_CharacterOptionData_C`

Its reflected `CharacterOptionData` map contains values with
`NumberOfColumns`, `Type`, and `OptionData`. Each option contains:

- `Name` and `OptionalText`
- `Color` and `Image`
- `BodyTypeCompatability`
- `FaceTypeCompatibility`
- `EyeTypeCompatibility`
- `DataHandle.DataTable` and `DataHandle.RowName`
- `bIsDisabled`

RuneSchema validates the DataTable and row before appending an option.
`AppendUnique` uses `DataHandle.RowName` as identity, rejects conflicts and
duplicates, verifies the resulting count, and replays character-option patches
before `WBP_CharacterOptionSelect` constructs or opens. Replay is required
because the game can reconstruct the CDO after initial mod loading.

## Verified categories

All tables are below `/Game/Gameplay/Character/Player/Customization/`.

| Category | DataTable | Vanilla options | Columns | Button |
|---|---|---:|---:|---|
| `BodyType` | `DT_Customization_BodyType` | 2 | 1 | Text |
| `FaceType` | `DT_Customization_FaceType` | 32 | 4 | Index/image |
| `HairColor` | `DT_Customization_HairColor` | 8 | 4 | Color |
| `EyeColor` | `DT_Customization_EyeColor` | 8 | 4 | Color |
| `SkinTone` | `DT_Customization_SkinTone` | 8 | 4 | Color |
| `EyeType` | `DT_Customization_EyeType` | 32 | 4 | Index/image |
| `HairPreset` | `DT_Customization_HairPresets` | 31 | 4 | Index/image |
| `FacialHairPreset` | `DT_Customization_FacialHairPresets` | 40 | 4 | Index/image |
| `EyebrowColor` | `DT_Customization_EyebrowColor` | 8 | 4 | Color |

The exact row structure differs by table. Copy a verified row from the same
table and change only fields exposed by that reflected row type.

## Required two-part transaction

A new choice requires both parts:

1. Add or patch its DataTable row through `/raw`.
2. Append its menu option through `/assets`.

A row alone creates no button. A button is rejected when its DataHandle does
not resolve. The menu document needs only the native `DA_` target and the field
being changed. RuneSchema already knows the owning mod from its folder:

```json
{
  "/Game/UI/MainMenu/CharacterCreate/Data/DA_CharacterOptionData.DA_CharacterOptionData_C": {
    "CharacterOptionData[ECharacterOptionType::HairPreset].OptionData": {
      "$AppendUnique": [{
        "Name": "My Hair",
        "OptionalText": "",
        "Image": "/Game/Mods/MyMod/UI/T_MyHair.T_MyHair",
        "BodyTypeCompatability": "both",
        "FaceTypeCompatibility": "all",
        "EyeTypeCompatibility": "all",
        "DataHandle": {
          "DataTable": "/Game/Gameplay/Character/Player/Customization/DT_Customization_HairPresets.DT_Customization_HairPresets",
          "RowName": "MyHair01"
        },
        "bIsDisabled": false
      }]
    }
  }
}
```

For another category, change the enum selector and use its own table. Copy a
template from the same category because text, color, and image entries have
different presentation expectations.

Do not add `$schema`, `schema`, `modId`, `profile`, transaction IDs, or another
copy of the target. For this verified option array, RuneSchema infers the CDO,
the expected class, `DataHandle.RowName` uniqueness, and the native template.
The older registry-patch envelope remains readable so existing mods are not
broken, but it is not the current authoring form.

For edits shared by many existing options, `$MergeWhere` groups the selectors
under the option array. The vanilla beard example changes forty entries while
stating the `DA_` target, array field, and merged value only once:

```json
{
  "/Game/UI/MainMenu/CharacterCreate/Data/DA_CharacterOptionData.DA_CharacterOptionData_C": {
    "CharacterOptions[FacialHairPreset].OptionData": {
      "$MergeWhere": {
        "Field": "DataHandle.RowName",
        "Values": ["F_A_PresetNone", "F_A_Preset1", "M_A_PresetNone", "M_A_Preset1"],
        "Value": {
          "BodyTypeCompatability": "both",
          "FaceTypeCompatibility": "all"
        }
      }
    }
  }
}
```

The abbreviated list above demonstrates the shape. The complete forty-entry
definition is in `examples/CharacterCustomization`.

## Compatibility values

- `BodyTypeCompatability`: `both`, `male`, or `female`.
- `FaceTypeCompatibility`: `all` or a native numeric compatibility index.
- `EyeTypeCompatibility`: `all` or a native numeric compatibility index.
- `DataHandle.RowName`: unique in the category and already present in its
  DataTable at commit time.

## Evidence and diagnostics

With advanced logging enabled, successful writes report the common
`[LOADER:assets][OK]` form and character verification includes
`[CHARACTER-OPTIONS][VERIFIED]` with `missing=0 duplicates=0`. Menu opening
replays only retained character-option patches. MoreHair established the
boundary: 47 HairPreset rows plus a 47-entry CDO produced 47 native buttons;
47 rows with a reconstructed 31-entry CDO still produced only 31.

RuneSchema cannot correct mesh fitting. Hair and facial-hair skeletal meshes
must be authored against the Dragonwilds heads/skeleton and contain their final
scale and local offset.

## Nested organization

Every JSON/JSONC loader directory is recursive. Files may be organized as
`assets/character/hair/menu.json` and `raw/character/hair/rows.json`.
Symlinked directories are not followed, paths cannot escape the loader root,
and each loader is capped at 4,096 JSON files.
