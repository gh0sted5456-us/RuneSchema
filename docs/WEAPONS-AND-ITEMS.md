# Create weapons and items

## Step-by-step workflow

1. Choose a compatible existing item clone or a cooked ItemData route; new art needs a mounted content provider.
2. Assign stable object path, InternalName and canonical persistence identity; resolve equipment/attack dependencies.
3. Use the JSONC mock-ups below to register/patch the item, then add its recipe, vendor or discovery tie-in.
4. Restart and verify delivery, equip/use, replication and save/reload independently; a visible card is only presentation.

JSONC snippets are authoring mock-ups. Replace example paths, IDs and values with
inspected content; a valid snippet cannot supply missing cooked dependencies.

**Tie-ins:** [Assets](reference/loaders/assets.md), [equipment](reference/loaders/equipment.md), [recipes](reference/loaders/recipes.md) and [PAK content](PAK-CONTENT-GUIDE.md).


There are two practical ways to create an item with RuneSchema. Copy an existing
item when the game already has the behavior and art you need. Use a PAK when the
item needs new art, a new Blueprint, or a fully cooked item asset.

## Choose the simplest route

Use a RuneSchema item copy for:

- a new name or description;
- different durability, weight, or power;
- a new recipe using existing art;
- an armor or weapon variant that keeps an existing equipment family;
- an item that grants a verified existing effect.

Use a PAK for:

- a new mesh or texture;
- a new icon or sound;
- a new held-weapon Blueprint;
- a new animation or attack Blueprint;
- a new cooked `ItemData` asset.

New art does not automatically require new combat behavior. The most dependable
first release uses a known weapon family and changes its art, item data, balance,
and recipe.

## Copy an existing item

Helpy can inspect a working item, export a starter file, and generate a new
persistence ID. Select the closest source item so the copy inherits valid
equipment behavior.

A basic item copy looks like this:

```jsonc
{
  "/Game/RuneSchema/GoldenArsenal/Items/ITEM_GoldenSpear.ITEM_GoldenSpear": {
    "$Clone": "/Game/Gameplay/Items/Weapons/EXACT_SOURCE.EXACT_SOURCE",
    "InternalName": "golden_arsenal_spear",
    "PersistenceID": "PASTE_THE_ID_GENERATED_BY_HELPY",
    "Name": "Golden Spear",
    "FlavourText": "A balanced spear made for the long road."
  }
}
```

Replace the source path with a path copied from the running game. Replace the
persistence ID with the valid ID generated for this item. Do not release the
placeholder text above.

Only override values you intend to change. Inherited fields are often what keep
the item's equip, animation, repair, and interaction behavior working.

## Connect a cooked weapon

If the PAK already contains the item:

```jsonc
{
  "/GoldenArsenal/Items/ITEM_GoldenSpear.ITEM_GoldenSpear": {}
}
```

This tells RuneSchema to load the exact item. You may add supported adjustments
inside the braces, but the cooked asset should already have its required class,
type, internal name, and permanent identity.

If the PAK did not cook a registry declaration, you can instead announce its
exact ItemData paths with a loose `registry/*.jsonc` document containing
`NativeRegistries.Items`. See the
[complete loose-registration example](reference/loaders/registry.md#register-a-cooked-item-without-cooking-a-registry-declaration).
This does not require recooking the registry declaration; the referenced item
and its dependencies must still exist. Choose either this enrollment route or
the `assets` pointer above rather than duplicating the same registration.

## Keep the weapon family consistent

A weapon normally connects several pieces:

- item data;
- held or equipped actor;
- mesh and materials;
- icon;
- weapon family and animation pose;
- attacks and damage behavior;
- crafting and repair information.

Start with one working family, such as an existing spear, bow, shield, or staff.
Mixing a spear item with a sword actor and an unrelated animation set can produce
an item that appears in the inventory but fails when equipped or attacked with.

## Add custom attacks carefully

Dragonwilds uses different authorities for melee attacks, ranged attacks,
combat magic, utility magic, and equipment effects. A weapon appearing in the
item list does not prove that its attack or spell path is usable.

### Physical melee attacks

1. Cook the item, held-equipment data, animation references, and every custom
   attack class into the mod's PAK. Use a known weapon family as the template.
2. Connect the item to the cooked equipment/attack collection in its own data.
3. If the game's native path does not enroll the class collection, list the
   complete, correctly ordered classes under `MeleeAttackClasses` in the
   mod-owned cooked registry declaration. Enable the specific melee fallback
   only when the mod calls for it.
4. Check the live class order, every combo step, and multiplayer behavior. A
   partial or wrong-class collection must be fixed in the PAK, not padded with
   placeholder entries.

### Bows, crossbows, and other ranged weapons

1. Cook and register the weapon's item data; cook the `HeldEquipmentData` it
   references.
2. Keep its `RangedAttackCollection` reference intact. That collection needs
   both `QuickAttackData` and `FullAttackData` pointing to valid cooked player
   attack classes.
3. If the normal item-owned path needs the optional RuneSchema fallback, use
   the ranged-equipment switch. Do not put shot/projectile implementation
   classes into `RangedAttackClasses`; that field is not a global ranged
   registry.
4. Verify equip, firing, ammo use, damage, and re-equip separately from item
   registration.

### Combat and utility magic

1. Cook spell data and its effect, projectile, animation, and presentation
   dependencies into the mod's PAK.
2. Declare persistent spell assets in `CombatSpells` or `UtilitySpells` in the
   cooked registry asset. Keep the equipped weapon's
   `MagicAttackCollection` link to the intended spells.
3. Verify that the live spell registry accepts the data and that the weapon's
   cast path invokes it. Registering a spell does not create a new cast input,
   animation, or network behavior.

The [cooked registry guide](COOKED-PAK-REGISTRY-MANIFEST.md) gives the exact
declaration fields, while [combat fallback settings](COMBAT-FALLBACK-SETTINGS.md)
explains which optional switches affect attack collections.

The melee class lane is guarded and must resolve completely before
RuneSchema changes the process-scoped component default. A component that was
already constructed in the first gameplay world may receive the same complete
plan once; later worlds inherit the default. AI attack collection registration
is not offered by the current cooked declaration.

For a cooked content mod, the
[shared registry bridge PAK walkthrough](SHARED-REGISTRY-BRIDGE-PAK.md) shows
how to ship one mod-owned registry declaration without thousands of loose
pointer files.

Custom attacks are still more demanding than a new mesh or balance change. Test:

- the first attack and every combo step;
- short and long combo endings;
- stamina use;
- hit detection and damage;
- blocking, interruption, and unequip behavior;
- host and client use;
- world exit and re-entry.

If a new attack family is not recognized, keep the weapon on a known attack
family for the public release and continue the custom behavior as an experimental
option.

## Add a crafting recipe

```jsonc
{
  "RECIPE_GoldenSpear": {
    "AddTo": [
      {
        "Table": "DT_CraftingStationsDataTable",
        "Row": "MysticForge",
        "Category": "Weapons"
      }
    ],
    "Properties": {
      "ItemsConsumed": [
        {
          "ItemData": "/Game/Gameplay/Items/Resources/EXACT_MATERIAL.EXACT_MATERIAL",
          "Count": 10
        }
      ],
      "ItemsCreated": [
        {
          "ItemData": "/GoldenArsenal/Items/ITEM_GoldenSpear.ITEM_GoldenSpear",
          "Count": 1
        }
      ],
      "ExtraItemsCreated": [],
      "bIgnoreNotification": true
    }
  }
}
```

Copy the exact native station row and material path from working content. A
recipe path is not a guessable display name.

## Item release checklist

- The PAK mounts from the enabled mod folder.
- The item path loads.
- The item appears in the item registration summary.
- The internal name and persistence ID are unique.
- The icon appears in every relevant menu.
- The item equips and unequips.
- Every attack, block, tool, or use action works.
- The recipe consumes the correct amount and creates the correct item.
- The item survives save, menu return, re-entry, and full restart.
- Host and client see the same item.
- Removing the mod from a backup save removes only the missing item reference.
