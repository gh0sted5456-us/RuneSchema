# Create weapons and items

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

```json
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

```json
{
  "/GoldenArsenal/Items/ITEM_GoldenSpear.ITEM_GoldenSpear": {}
}
```

This tells RuneSchema to load the exact item. You may add supported adjustments
inside the braces, but the cooked asset should already have its required class,
type, internal name, and permanent identity.

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
item list does not prove that its combat classes joined the correct collection.

RuneSchema has verified subsystem lanes for combat spells, utility spells, and
held-equipment effects. Cooked manifests can declare a complete ordered
`MeleeAttackClasses` collection. Ranged weapons do not share an equivalent
global player registry: register the cooked `HeldEquipmentData` item and keep
its `RangedAttackCollection` reference intact. RuneSchema resolves the
collection defaults after item registration and admits only its complete
quick/full attack-data pair to the live ranged component; it never inserts
lower-level shot classes. Magic similarly keeps its normal weapon-owned attack
collection while persistent spell data is declared through `CombatSpells` or
`UtilitySpells`.

The melee class lane remains experimental and must resolve completely before
RuneSchema changes the process-scoped component default. A component that was
already constructed in the first gameplay world may receive the same complete
plan once; later worlds are read-only and inherit the default. Read the
[cooked PAK registry manifest guide](COOKED-PAK-REGISTRY-MANIFEST.md) before
building a new custom attack family around them. AI attack registration is
still undergoing lifecycle validation.

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

```json
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
