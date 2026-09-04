# Ten capes: complete `/assets` + `/raw` + `/recipes` example

This creates ten independent baseline items from the native White Adventurer's
Cape without changing the original. FModel confirmed the source asset, both mesh
data references, icon, slot, skill, identity fields, and its native
`DT_WearableEquipment/Cape_Adventurers_White` row (2 defense, zero resistances).

## `/assets`

Top-level keys are target object paths. An ordinary object edits an existing asset.
The exact-case creation directives are:

- `$Patch` plus `$Target`: deterministic recursive merge into an existing asset.
- `$Clone`: safe `ItemData` property copy into a genuinely new runtime item.
- `$Create`: construct a supported DataAsset/Curve from a reflected class path.
- `$ConsumablePack`: normalized concise/native item-drop pack authoring.

`$Patch` rejects nested creation directives and identity changes. `$Clone` and new
items require unique `PersistenceID` and `InternalName`; persistence IDs are
canonical 22-character unpadded base64url values encoding 16 bytes. Directive case
is strict. Created items are registered with the native item subsystem and remain
addressable through their authoring path aliases.

All other keys are reflected properties of the target class. Common item fields:

| Field | Purpose |
| --- | --- |
| `PersistenceID`, `InternalName` | Permanent save/catalog identity. |
| `Name`, `FlavourText` | Visible name and inventory description. |
| `Icon`, `StaticMesh` | UI icon and dropped/world mesh soft paths. |
| `MaleMeshData`, `FemaleMeshData` | Equipped visual object references. |
| `WearableEquipmentDataTableRowHandle` | DataTable object plus stat `RowName`. |
| `Slot`, `SkillUsed` | Equipment slot and associated skill. |
| `PowerLevel`, `BaseDurability`, `Weight` | Numeric item tuning. |
| `ItemFilterTags`, `AudioSwitchTag` | Filters and material audio. |
| `bDropOnDeath`, `OnBreakSound` | Death/break behavior. |
| `bSoftDeleted` | Exact native FModel item availability flag. Set `false` for active items; set `true` only when intentionally retiring an item without reusing its identity. |
| `BuffDatas`, `GrantedEffects` | Supported buff presentation/effects. |

Reflection also supports correctly shaped structs, arrays, sets, maps, enums,
texts, objects, and soft references. Exact legal fields depend on the target class;
verify them in FModel. Transient/editor/instanced identity fields are intentionally
excluded from safe cloning.

`Scale` is not a general `ItemData` field. A dropped pickup is a separate spawned
actor; resizing it requires scaling that actor after the native drop operation.
RuneSchema therefore does not claim that `/assets` `Scale` changes every pickup,
equipped mesh, or inventory icon. Use a verified reflected mesh-data transform when
the particular item class provides one.

## `/raw`

The outer key is the native DataTable name, the next key is a row name, and its
value matches that table's cooked row struct. This pack adds ten new
`DT_WearableEquipment` rows, each with every wearable field:

```json
{"Defense":12,"MeleeResistance":8,"RangedResistance":2,"MagicResistance":-2,"MaxVitalShield":0}
```

Using an existing row name edits it; using a unique row name adds it. Each cape's
row handle points to its corresponding new row.

## `/recipes`

Each recipe object supports:

- `AddTo[]`: `Table`, `Row`, and `Category` destinations.
- `Properties.ItemsConsumed[]`: item reference plus `Count`.
- `Properties.ItemsCreated[]`: result reference plus `Count`.
- `Properties.ExtraItemsCreated[]`: optional additional outputs.
- `Properties.AudioTag`: optional `{TagName}`.
- `Properties.SkillUsedToCraft`: optional skill object reference.
- `Properties.bIgnoreNotification`: boolean notification control.

The ten examples add the capes to `MysticForge` under `Armour/Cape` and consume
three to eight Mystic Cloth. Result paths point to the new `/assets` aliases.
Placement follows the demonstrated RuneSchema contract exactly:
`{"Table":"DT_CraftingStationsDataTable","Row":"MysticForge","Category":"Armour/Cape"}`.

## Deterministic composition and uses

`mods.txt` determines enabled-mod order; filenames are sorted; JSON object/array
processing is stable. This supports a base item pack followed by balance patches,
server-specific `/raw` overrides, compatibility `$Patch` mods, and separate recipe
packs exposing the same assets at different stations. Later matching field writes
win; creation identities and row/recipe names must remain globally unique.

Test registration, crafting, name/flavor/icon/mesh display, equip stats, drop,
save/reload, duplicate rejection, and a second mod that deterministically patches
one cape.

Credits: Snorkles and Jonesing4Space.
