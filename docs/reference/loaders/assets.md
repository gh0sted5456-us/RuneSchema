# Assets loader

## Step-by-step workflow

Use `RuneSchema/mods/MyMod/assets/` for this example. JSONC blocks are
**mock-ups**, not an automatically installed feature: replace placeholder paths,
IDs, coordinates and reflected fields with values from your content. New art or
classes must be cooked/mounted separately. Keep published identities stable.

1. Choose a mounted item of the same native class and behavior; inspect its exact object path.
2. Create `assets/10-camp-item.jsonc`. For a clone, assign a new destination, canonical persistence ID and InternalName; for a patch, retain the existing identity.
3. Use the mock-up below and replace every placeholder. Change only reflected fields present on that class.
4. Restart, inspect registration, then reference the resulting path from recipes, shops or journal discovery. Verify save/reload before distribution.

**Tie-ins:** [Recipes](recipes.md), [raw rows](raw.md), [vendors](vendors.md), [journal](journal.md) and [quest acceptance items](../../modders/quests-and-questboards.md).


Craftable consumables may declare [`$QuestScroll`](../../QUEST-SCROLLS.md) to
accept an existing RuneSchema quest through the pre-consumption native item-use
route. The expected item PersistenceID is verified; this is not a quest/reward
script field or an identity override.

**Folder:** `RuneSchema/mods/<ModName>/assets/`

Items, icons, stats, unlock links, DataAssets, and reflected object patches.

[← Loader reference](../../LOADER-WALKTHROUGHS.md)

Use `/assets` to patch a loaded item or clone a compatible item into a stable
RuneSchema runtime asset. A JSON clone is not a newly cooked Unreal package.

```jsonc
{
  "/Game/MyMod/Items/ITEM_CampSupplies.ITEM_CampSupplies": {
    "$Clone": "/Game/Gameplay/Items/ITEM_Source.ITEM_Source",
    "PersistenceID": "uxA9m4CWTOyRV2K8e5PHgQ",
    "InternalName": "mymod_camp_supplies",
    "Name": "Camp Supplies",
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

For a connected item/quest recipe, use the [Quest Board starter](../../modders/quests-and-questboards.md).

## Wearables without a PAK

Schema-created `WearableEquipmentData` items enter the same native wearable
enumeration/cache bridge after successful identity and network registration.
No additional PAK registry declaration is needed for ordinary `/assets` clones.
Existing cooked, vanilla or schema-created wearables can request enrollment
after their field patch:

```jsonc
{
  "/Game/RuneSchema/MyClones/Items/MyCape.MyCape": {
    "$WearableBridge": true
    // MaleMeshData/FemaleMeshData may reference compatible existing mesh data.
  }
}
```

`$WearableBridge` is a Boolean directive, not a native item property. `false`
means no additional request; it does not disable automatic wearable support.
The target must be wearable equipment. Late patches require the existing
persistence and two-way network identities to resolve to that exact item;
the bridge does not allocate an identity to make an invalid patch appear valid.
Startup requests can wait for normal settled registration. Presentation failures
do not revoke otherwise valid item registration.

The native equipment system still owns mesh-cache construction. JSON cannot
invent a missing skeletal mesh, skeleton, texture or material; use compatible
existing assets or ship new assets in a PAK. This directive does not promise
immediate repainting of an already equipped actor after arbitrary edits to
shared mesh/material data. Re-equip the item when testing a changed appearance.
Helpy's transmog card follows the donor's resolved inventory icon unless an
explicit icon override is supplied; this is not a generated 3D thumbnail.

## Direct `DA_` fields

For an existing DataAsset or generated-class default, put the complete `DA_`
path first and native field paths below it. RuneSchema infers a class default
object from `_C`; the mod directory supplies ownership.

```jsonc
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

## Simple rules

- Use `/assets` for loaded items, DataAssets, reflected object edits, or compatible item clones.
- A new clone needs a stable destination path, `PersistenceID`, and `InternalName`.
- Change only fields that exist on the reflected source type. Put referenced DataTable-row work in `/raw`.

## FAQ

### FAQ-ASSETS-001 — Can I clone an item and change only a few fields? {#faq-assets-001}

Yes. Start from a compatible source with `$Clone`; inherited fields remain unless
you explicitly replace them. Give the clone its own stable path,
`PersistenceID`, and `InternalName`.

### FAQ-ASSETS-002 — Do direct character-menu DataAsset edits need the old registry metadata envelope? {#faq-assets-002}

No. For the verified direct `DA_` authoring form, put the complete target path
first and the native field path below it. The mod directory supplies ownership;
the legacy registry-patch envelope remains readable for compatibility.

## Working examples

- [Quest Board acceptance item starter](../../examples/QuestBoardWalkthrough/assets/20-ash-contract.jsonc)

---

[← All loaders](../../LOADER-WALKTHROUGHS.md) · [Authoring guide](../../AUTHORING-GUIDE.md)
