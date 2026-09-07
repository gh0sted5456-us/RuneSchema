# RuneSchema 0.6.1E authoring guide

This guide explains how to build RuneSchema mods for *RuneScape: Dragonwilds* using the features verified during development of RuneSchema 0.6.1E. It uses the installed Armor Collection, Resources and Loot, World Appearance, player-profile, named-spawn, named-resource, upgraded-rune, and patch collections as practical examples.

The Gravetide Staff section documents an original mod by **Snorkles**. It is included as a verified example of integrating an externally cooked mod with RuneSchema. Credit for the Gravetide Staff concept and original mod belongs to Snorkles.

RuneSchema 0.6.1E is released by **Jonesing4Space**. RuneSchema 0.6.0 was created by **Snorkles** and is based on **PalSchema** by Okaetsu.

## 1. What a RuneSchema mod contains

Each mod has one folder beneath `ue4ss/Mods/RuneSchema/mods/`. Inside it, files are grouped by the system they author:

```text
RuneSchema/
  mods/
    MyMod/
      assets/
      raw/
      blueprints/
      recipes/
      journal/
      equipment/
      players/
      spawns/
      buildings/
      courses/
      strings/
      paks/
        MyCookedContent/
          MyCookedContent.pak
          MyCookedContent.utoc
          MyCookedContent.ucas
```

Create only the folders the mod uses. JSON files may be named freely. Numeric filename prefixes are useful when several files in one loader have dependencies:

```text
assets/00-items.json
raw/10-stat-rows.json
recipes/20-recipes.json
journal/30-journal.json
```

The folder name is the mod identity used by cross-mod `$Patch` targets such as `ArmorCollection:RECIPE_RS_Dawnveil_Body`.

### Loader responsibilities

| Folder | Intended purpose |
| --- | --- |
| `assets` | Patch or clone item data; set icons, names, stats stored on the item, gameplay-effect grants, perk text, and equipment appearance metadata. |
| `raw` | Add or patch DataTable rows, including wearable stat rows and loot tables. |
| `blueprints` | Patch reflected Blueprint class/component values and assign Blueprint appearance rules. |
| `recipes` | Add crafting, processing, or destruction recipes to existing station tables. |
| `journal` | Add or patch journal entries and recipe discovery pages. |
| `equipment` | Configure validated runtime behavior that data alone cannot express, currently Surge-on-evade and Shadowveil action protection. |
| `players` | Apply named or positional player profiles and player appearance. |
| `spawns` | Create or patch AI/world actors, names, scaling, drops, respawn behavior, and appearance. |
| `buildings` | Author supported building definitions and building-related data. |
| `courses` | Author supported course records. |
| `strings` | Add or replace supported localized strings. |
| `paks` | Store cooked Unreal content used by the JSON, such as icons, meshes, or externally created item assets. |

Keep ordinary equipment data in `assets`. Do not duplicate an asset patch or a gameplay-effect grant in `equipment`. The `equipment` folder exists only for behavior that requires RuneSchema's validated runtime hooks.

### How the installed examples are organized

The installed collection demonstrates two useful scales of mod organization.

`ArmorCollection` groups a common equipment theme while keeping every set independently editable:

```text
ArmorCollection/
  assets/
    00-DawnveilPaladinSet.json
    02-ShadowPaladinSet.json
    03-VeilrunnerSet.json
    97-GhostlyRemembrancer.json
    98-GhostlyWatcher.json
    99-GhostlyWarden.json
    100-GhostlyWardenWeapons.json
  raw/
    matching wearable-row files
  recipes/
    matching crafting and destruction files
  journal/
    set and collection entries
  equipment/
    only Surge and Shadowveil runtime rules
  paks/
    GhostlyItems/
```

`ResourcesAndLoot` groups related world-economy edits:

```text
ResourcesAndLoot/
  assets/
    00-Extended Enemy Drops.json
    01-Extended Resources.json
  blueprints/
    01-Extended Resources.json
  raw/
    00-Extended Enemy Drops.json
    02-Extended Treasure Chests.json
```

This structure keeps a concept together without mixing loader ownership. The installed Resources and Loot set currently spans 33 resource asset edits, 29 Blueprint definitions, enemy-loot data, and three treasure-table groups. Large mods remain easier to diagnose when each loader has a focused file and late corrections live in a separate `ZZ_` compatibility mod.

## 2. Enabling mods and controlling order

RuneSchema reads its mod-order file from `distribution/mods/runeschema.txt` in the packaged source and the equivalent installed runtime location. Each entry uses `1` for enabled and `0` for disabled:

```text
ArmorCollection : 1
ResourcesAndLoot : 1
WorldAppearance : 1
ZZ_CollectionPatches : 1
```

Folder names beginning with `AA_` load before ordinary names. Folder names beginning with `ZZ_` load after ordinary names. Ordering is case-insensitive and deterministic inside each group.

Use the order to express dependencies:

1. A base mod creates or appends data.
2. A later `ZZ_` mod patches the completed definition.
3. Every referenced item, row, recipe, and spawn must already have a stable identity.

`AA_` and `ZZ_` control RuneSchema JSON order. They do not control Unreal pak mount priority. Avoid shipping two copies of the same cooked package under different mod folders.

Restart after adding folders, changing load order, adding appearance rules, or changing Blueprint rules. Auto-reload is useful for supported data edits, but it is not a complete replay of startup and streaming.

## 3. Paths, identities, and references

Most failed mods come from mixing three different identities:

- **Authoring key:** the JSON object's key or `$Patch` target.
- **Runtime object path:** the final Unreal object path used by recipes, equipment rules, and other references.
- **Persistence ID:** a unique save identity stored on a cloned item.

An Armor Collection clone demonstrates all three:

```json
{
  "/Game/RuneSchema/ArmorCollection/Items/rs_ghostly_warden_legs.rs_ghostly_warden_legs": {
    "$Clone": "/Game/Gameplay/Character/Player/Equipment/Legs/ITEM_Armour_T5_Legs_Steel.ITEM_Armour_T5_Legs_Steel",
    "InternalName": "rs_ghostly_warden_legs",
    "PersistenceID": "aq_C39nMU7ef7Aj7sPJjhQ",
    "Name": "Ghostly Warden Greaves"
  }
}
```

Use full canonical paths when a field expects an object reference. Do not copy FModel export-number suffixes such as `.0` into a field that expects the canonical asset object. Existing RuneSchema examples show both accepted structured references and direct path strings.

Every cloned item needs a unique `PersistenceID` and `InternalName`. Reusing either can corrupt save identity or make recipes resolve the wrong object. Once a mod has shipped, treat both values and the clone's runtime path as permanent.

## 4. A complete equipment set

The Ghostly Warden set in Armor Collection shows the complete workflow: clone items, connect armor stat rows, mark masterwork state, grant native effects, describe the effects with icons, add appearance, add recipes, add destruction recipes, and configure runtime-only equipment behavior.

### 4.1 Clone the item in `assets`

```json
{
  "/Game/RuneSchema/ArmorCollection/Items/rs_ghostly_warden_legs.rs_ghostly_warden_legs": {
    "$Clone": "/Game/Gameplay/Character/Player/Equipment/Legs/ITEM_Armour_T5_Legs_Steel.ITEM_Armour_T5_Legs_Steel",
    "InternalName": "rs_ghostly_warden_legs",
    "PersistenceID": "aq_C39nMU7ef7Aj7sPJjhQ",
    "Name": "Ghostly Warden Greaves",
    "FlavourText": "Greaves for a guardian who arrives before the bell falls silent.",
    "WearableEquipmentDataTableRowHandle": {
      "RowName": "T06_rs_ghostly_warden_legs"
    },
    "PowerLevel": 6,
    "bIsMasterworkItem": true,
    "MasterworkType": "EMasterworkUpgradeType::Masterwork",
    "BaseDurability": 1800,
    "$VisualEffect": {
      "Type": "Ghost",
      "Overlay": true,
      "BodyMaterial": true,
      "MainColor": { "R": 0.2, "G": 0.8, "B": 1.0, "A": 1.0 }
    },
    "BuffDatas": [
      {
        "Title": "Surge",
        "Description": "Evade becomes Surge while these legs are worn.",
        "BuffIcon": {
          "AssetPathName": "/Game/Art/UI/Skills/Icons/Unlock/Magic/T_Skill_Magic_Spell_Surge.T_Skill_Magic_Spell_Surge"
        }
      }
    ]
  }
}
```

The item inherits unspecified fields from the steel-leg source. This is safer than rebuilding every reflected field from scratch. Override only values that define the new item.

`bIsMasterworkItem` and `MasterworkType` make the item present as masterwork data. They do not automatically create a complete future ascension tree. Upgrade compatibility still depends on the game's row naming, recipes, and native expectations.

### 4.2 Add the wearable row in `raw`

```json
{
  "DT_WearableEquipment": {
    "T06_rs_ghostly_warden_legs": {
      "Defense": 33.0,
      "MeleeResistance": 3.0,
      "RangedResistance": -1.5,
      "MagicResistance": -3.0,
      "MaxVitalShield": 0.0
    }
  }
}
```

The row name must match `WearableEquipmentDataTableRowHandle.RowName`. The `T06_` prefix follows the native power-level naming behavior observed for wearable rows. Use the tier matching the item's intended power level.

### 4.3 Add a crafting recipe

```json
{
  "RECIPE_rs_ghostly_warden_legs": {
    "AddTo": [
      {
        "Table": "DT_CraftingStationsDataTable",
        "Row": "MysticForge",
        "Category": "Ghostly Warden - Heavy Vigil"
      }
    ],
    "Properties": {
      "ItemsConsumed": [
        {
          "ItemData": "/Game/Gameplay/Character/Player/Equipment/Legs/ITEM_Armour_T5_Legs_Steel.ITEM_Armour_T5_Legs_Steel",
          "Count": 1
        },
        {
          "ItemData": "/Game/Gameplay/Items/Resources/Magic/ITEM_Resources_WildAnima.ITEM_Resources_WildAnima",
          "Count": 50
        }
      ],
      "ItemsCreated": [
        {
          "ItemData": "/Game/RuneSchema/ArmorCollection/Items/rs_ghostly_warden_legs.rs_ghostly_warden_legs",
          "Count": 1
        }
      ],
      "ExtraItemsCreated": [],
      "bIgnoreNotification": true
    }
  }
}
```

The recipe consumes a real base item plus Wild Anima, then creates the cloned item. This makes the upgrade relationship clear to players without changing the vanilla source item.

### 4.4 Add a destruction recipe

Ghostly Warden items can be dismantled into ectoplasm and materials. For testing, Armor Collection places destruction recipes on the Campfire processing list:

```json
{
  "DESTRUCTION_RECIPE_rs_ghostly_warden_sword": {
    "AddTo": [
      {
        "Table": "DT_ProcessingStationDataTable",
        "Row": "Campfire",
        "Array": "Recipes"
      }
    ],
    "Properties": {
      "ItemsConsumed": [
        {
          "ItemData": "/Game/RuneSchema/ArmorCollection/Items/rs_ghostly_warden_sword.rs_ghostly_warden_sword",
          "Count": 1
        }
      ],
      "ItemsCreated": [
        {
          "ItemData": "/Game/Gameplay/Items/Resources/Animal/ITEM_Resources_Ectoplasm.ITEM_Resources_Ectoplasm",
          "Count": 10
        },
        {
          "ItemData": "/Game/Gameplay/Items/Resources/Metal/ITEM_Resources_SteelBar.ITEM_Resources_SteelBar",
          "Count": 5
        }
      ],
      "ExtraItemsCreated": [],
      "SkillXPAwardedOnCraft": 0,
      "bIgnoreNotification": true
    }
  }
}
```

Balance returned materials below the original crafting cost unless the design intentionally creates a conversion loop.

### 4.5 Add native gameplay effects and visible perk descriptions

Native equipment effects belong on the item in `assets`:

```json
{
  "/Game/RuneSchema/ArmorCollection/Items/rs_ghostly_warden_body.rs_ghostly_warden_body": {
    "$Clone": "/Game/Gameplay/Character/Player/Equipment/Body/ITEM_Armour_T4_Body_Paladin.ITEM_Armour_T4_Body_Paladin",
    "InternalName": "rs_ghostly_warden_body",
    "PersistenceID": "RrbSh7x7UmmpBChFo6PFaw",
    "GrantedEffects": [
      "/Game/Gameplay/GameplayEffects/ArmorSets/GE_HeavyArmorSetT3IncreaseStaggeredTargetDamageModifier.GE_HeavyArmorSetT3IncreaseStaggeredTargetDamageModifier_C"
    ],
    "BuffDatas": [
      {
        "Title": "Last Stand",
        "Description": "Deal 25% more damage to staggered targets while equipped.",
        "BuffIcon": {
          "AssetPathName": "/Game/Art/UI/Skills/Icons/Tags/T_Icon_Tag_Skill_Attack.T_Icon_Tag_Skill_Attack"
        }
      }
    ]
  }
}
```

`GrantedEffects` controls the native effect. `BuffDatas` explains it in the item interface and supplies an icon. The text does not create the effect, and the effect does not automatically create accurate text. Keep them aligned.

Prefer a verified native effect whose attribute, duration, and stacking behavior have been inspected. Reusing an effect can inherit conditions that are not obvious from its name.

### 4.6 Add runtime-only equipment behavior

Surge-on-evade belongs in `equipment` because it changes the native evade pathway while a particular leg item is worn:

```json
{
  "SurgeEvadeLegs": {
    "/Game/RuneSchema/ArmorCollection/Items/rs_ghostly_warden_legs.rs_ghostly_warden_legs": true
  }
}
```

Shadowveil action protection also belongs in `equipment`:

```json
{
  "ShadowveilWearables": {
    "/Game/RuneSchema/ArmorCollection/Items/rs_ghostly_watcher_cape.rs_ghostly_watcher_cape": {
      "PreserveOn": [
        "MeleeAttack",
        "RangedAttack",
        "MagicAttack",
        "UtilityCast",
        "Evade"
      ]
    }
  }
}
```

Each `PreserveOn` list replaces the earlier list for that item. Supported actions are `MeleeAttack`, `RangedAttack`, `MagicAttack`, `UtilityCast`, and `Evade`. An empty list or `false` disables protection. Damage, interactions, unequipping, and other native removal conditions remain native behavior.

These rules are exact-path mappings with a capacity of 64 items per behavior. They require the matching client or dedicated-server native contract. RuneSchema disables the affected behavior if validation fails rather than installing a partial hook.

## 5. Appearance effects

RuneSchema's ghost appearance is a material effect. It is not a texture replacement, particle system, spell enchantment, or character-creation skin color.

```json
{
  "Type": "Ghost",
  "Overlay": true,
  "BodyMaterial": true,
  "MainColor": { "R": 0.2, "G": 0.8, "B": 1.0, "A": 1.0 },
  "SecondaryColor": { "R": 0.05, "G": 0.2, "B": 0.5, "A": 1.0 }
}
```

- `Overlay` defaults to `true` and enables the glow overlay.
- `BodyMaterial` defaults to `false` and replaces physical mesh material slots with the baked ghost body material.
- At least one of `Overlay` or `BodyMaterial` must be enabled.
- Omit a color to use the material's default.
- Widgets and nameplates are excluded.

### Equipment appearance

Place `$VisualEffect` beside an asset patch or `$Clone`. RuneSchema applies it to the relevant worn head, body, legs, or cape mesh and to physical held-item meshes.

```json
{
  "$Patch": "/Game/Gameplay/Character/Player/Equipment/Cape/ITEM_Cape_Artisan.ITEM_Cape_Artisan",
  "$Target": {
    "$VisualEffect": {
      "Type": "Ghost",
      "Overlay": true,
      "BodyMaterial": true
    }
  }
}
```

This enables ideas such as ghostly armor sets, spectral weapons, glowing tools, themed shields, and different colors for each piece. Inventory icons and hidden or stowed meshes are separate assets and are not changed by the material rule.

### Blueprint appearance

World Appearance makes standing and fellable ash trees spectral without adding a new loader:

```json
[
  {
    "$Patch": "BP_Tree_Ash_01_C",
    "$Target": {
      "$VisualEffect": {
        "Type": "Ghost",
        "BodyMaterial": false,
        "MainColor": { "R": 0.2, "G": 0.8, "B": 0.95, "A": 1.0 },
        "SecondaryColor": { "R": 0.05, "G": 0.2, "B": 0.8, "A": 1.0 }
      }
    }
  },
  {
    "$Patch": "BP_FellableTree_Ash_C",
    "$Target": {
      "$VisualEffect": {
        "Type": "Ghost",
        "BodyMaterial": false,
        "MainColor": { "R": 0.2, "G": 0.8, "B": 0.95, "A": 1.0 }
      }
    }
  }
]
```

Blueprint rules apply to matching actors as they initialize, including later streamed actors. Streaming foliage that is not represented by a matching actor needs a different game pathway.

### Player and spawn appearance

Use `VisualEffect` in `players` and `spawns`. A player rule may use `Target: "PlayerMesh"` for the body/head or `Target: "EntirePerson"` to include equipped meshes. A specific equipment rule can override a broader player appearance for that item.

Spawn patches can add appearance to an existing RuneSchema spawn identity:

```json
[
  {
    "$Patch": "NamedSkeletonGroundingTest:rattleblade-fen",
    "$Target": {
      "VisualEffect": {
        "Type": "Ghost",
        "MainColor": { "R": 0.45, "G": 0.15, "B": 0.95, "A": 1.0 },
        "SecondaryColor": { "R": 0.08, "G": 0.55, "B": 0.85, "A": 1.0 }
      }
    }
  }
]
```

Dedicated servers skip cosmetic material work. Server and client still need matching item identities and gameplay definitions for equipment and recipes.

## 6. External cooked content: Gravetide Staff

**Gravetide Staff is an original Snorkles mod.** It is included in this guide as a verified external mod creation, showing how RuneSchema can integrate a cooked asset produced outside RuneSchema.

The pak supplies the item asset at `/Game/Mods/GravetideStaff/Gameplay/ITEM_Staff_Gravetide`. Because the item already exists in cooked content, the asset JSON does not use `$Clone`; it patches the loaded object directly:

```json
{
  "/Game/Mods/GravetideStaff/Gameplay/ITEM_Staff_Gravetide.ITEM_Staff_Gravetide": {
    "Name": "Gravetide Staff",
    "FlavourText": "A prison of whispering souls bound around a heart of black glass.",
    "DamageMultiplier": 16.2,
    "PowerLevel": 12,
    "BaseDurability": 2800,
    "Weight": 15.0,
    "AnimationPoseType": "EAnimationPoseType::Default",
    "AnimationPosesSequence": {
      "ObjectName": "AnimSequence'A_PlayerM_Idle_Staff_Poses'",
      "ObjectPath": "/Game/Art/Animation/PlayerM/Staff/A_PlayerM_Idle_Staff_Poses.0"
    }
  }
}
```

RuneSchema then exposes the cooked item through an ordinary recipe:

```json
{
  "RECIPE_Weapon_Staff_Gravetide": {
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
          "ItemData": "/Game/Gameplay/Items/Resources/Magic/ITEM_Rune_Essence.ITEM_Rune_Essence",
          "Count": 1
        }
      ],
      "ItemsCreated": [
        {
          "ItemData": "/Game/Mods/GravetideStaff/Gameplay/ITEM_Staff_Gravetide.ITEM_Staff_Gravetide",
          "Count": 1
        }
      ],
      "ExtraItemsCreated": [],
      "bIgnoreNotification": true
    }
  }
}
```

This establishes two supported creation routes:

1. `$Clone` an existing loaded item entirely through RuneSchema.
2. Cook a new Unreal asset in a pak, then use RuneSchema to configure and connect it to recipes and other game data.

Use the second route when a mod needs a genuinely new mesh, icon, material, Blueprint, animation, or cooked class. JSON cannot manufacture cooked Unreal assets.

The organized pak layout is:

```text
GravetideStaff/
  assets/
    01-GravetideStaff.json
  recipes/
    01-GravetideStaff.json
  paks/
    GravetideStaff/
      GravetideStaff.pak
      GravetideStaff.utoc
      GravetideStaff.ucas
```

Keep companion pak, utoc, ucas, and signature files together with their original names. A pak-only build does not need fabricated IoStore companions.

## 7. Patching existing definitions

Use `$Patch` to change a specific existing definition without replacing unrelated fields. `$Target` contains only the desired edits.

### Patch a scalar field

```json
{
  "$Patch": "ArmorCollection:RECIPE_RS_Dawnveil_Body",
  "$Target": {
    "Properties": {
      "SkillXPAwardedOnCraft": 500
    }
  }
}
```

Custom loader identities use `ModName:EntryId`. Examples include recipes, journal entries, player rules, and spawns:

```json
{
  "$Patch": "ArmorCollection:RS_Journal_Dawnveil_Armor",
  "$Target": {
    "DisplayName": "The Dawnveil Oathplate — Patched"
  }
}
```

### Patch one element in a reflected struct array

Resources and Loot first expands ash-tree drops. `ZZ_CollectionPatches` then adjusts only the ordinary ash entry:

```json
[
  {
    "$Patch": "BP_FellableTree_Ash_C",
    "$Target": {
      "ItemDropOnSplitComponent": {
        "ItemsToDrop": {
          "$Patch": [
            {
              "$Match": {
                "ItemDataClass": "/Game/Gameplay/Items/Resources/Wood/ITEM_Resources_Wood_Ash.ITEM_Resources_Wood_Ash"
              },
              "$Target": {
                "MinToDrop": 3,
                "MaxToDrop": 5
              }
            }
          ]
        }
      }
    }
  }
]
```

`$Match` must identify exactly one struct. A missing or ambiguous match fails without clearing or appending the array. Add more match fields when another mod can create duplicates.

Use `$Index` only when array order is stable:

```json
{
  "$Patch": [
    {
      "$Index": 0,
      "$Target": { "MinToDrop": 3, "MaxToDrop": 5 }
    }
  ]
}
```

Reference-field matches use full canonical object paths. Nested struct patches and nested struct-array patches are supported. Scalar or UObject arrays are outside the initial reflected struct-array operation.

### Patch a raw DataTable row

The installed enemy-drop patch changes the Monstrous Fang quantity in the Wolf loot row:

```json
{
  "$Patch": "DT_LootDropTable:Wolf",
  "$Target": {
    "Resources": {
      "$Patch": [
        {
          "$Match": {
            "SpawnedItemData": "/Game/Gameplay/Items/Resources/Animal/ITEM_Resources_Monstrous_Fang.ITEM_Resources_Monstrous_Fang"
          },
          "$Target": {
            "MinimumDropAmount": 2,
            "MaximumDropAmount": 3
          }
        }
      ]
    }
  }
}
```

A raw `$Patch` never creates a missing row. Define the row first or target a native row that exists in the current game build.

### 7.1 Buildings

Building definitions support `$Clone` and `$Patch`. A clone copies a loaded
`BuildingPieceData` asset into a RuneSchema-owned runtime object, after which
`Properties` may override reflected fields such as icon, mesh, skeleton, or
other build-piece parameters. The referenced cooked asset must exist on the
client:

```json
{
  "RuneForgeWall": {
    "$Clone": "/Game/Gameplay/BaseBuilding_New/BuildingPieces/SomePiece.SomePiece",
    "Properties": {
      "DisplayName": "Rune Forge Wall",
      "Icon": "/Game/Mods/RuneForge/UI/T_Icon_Wall.T_Icon_Wall",
      "Mesh": "/Game/Mods/RuneForge/Buildings/SM_RuneWall.SM_RuneWall",
      "Skeleton": "/Game/Mods/RuneForge/Buildings/SK_RuneWall.SK_RuneWall"
    },
    "Unlock": true,
    "AddTo": { "Collection": "Modded Buildings", "PageIndex": 0 }
  }
}
```

Patch an authored building by its stable `ModName:Key` identity. Property
patches are partial; `Requirements`, `Unlock`, and `AddTo` replace their whole
field when supplied:

```json
{
  "$Patch": "RuneForge:RuneForgeWall",
  "$Target": {
    "Properties": {
      "DisplayName": "Rune Forge Reinforced Wall",
      "Icon": "/Game/Mods/RuneForge/UI/T_Icon_ReinforcedWall.T_Icon_ReinforcedWall"
    }
  }
}
```

Building patches must load after the definition they target, normally by using
a later `ZZ_` compatibility mod. `$Patch` cannot change the source `Asset` or
`$Clone` path or the identity key.

### 7.2 Courses

Courses support identity-based `$Patch` after the course definition has been
loaded. `$Target` is merged into the original course document, so omitted
fields remain unchanged and arrays such as `Orbs`, `Props`, and rewards are
replaced when supplied:

```json
{
  "$Patch": "RacePack:ash-run",
  "$Target": {
    "DisplayName": "Ash Run — Expert",
    "OrbXP": 25,
    "Gold": { "TimeSeconds": 45 }
  }
}
```

Course `Id` is protected. Courses are built into the world during world load;
restart the game after changing a course definition or patch.

## 8. Loot tables and named AI

Named Spawn Showcase demonstrates the complete connection between an AI spawn and custom loot.

First, `raw` creates a loot row and an enemy-loot routing row:

```json
{
  "DT_LootDropTable": {
    "RS_WhiteRoadRevenant": {
      "Conditions": [],
      "DropChance": 100.0,
      "Resources": [
        {
          "SpawnedItemData": {
            "ObjectName": "BP_Consumables_ItemEmitter_C'ITEM_Consumable_ZombiePack'",
            "ObjectPath": "/Game/Gameplay/Items/Consumables/Misc/ITEM_Consumable_ZombiePack.0"
          },
          "MinimumDropAmount": 1,
          "MaximumDropAmount": 1,
          "DropChance": 100.0,
          "bOneInstancePerPlayerOnlyVisibleToThem": false,
          "bAutoAddToInventory": false,
          "bOnlyForPlayersThatInflictedDamage": false
        }
      ],
      "Recipes": []
    }
  },
  "DT_EnemyLootDropTable": {
    "RS_WhiteRoadRevenant": {
      "TablesByPowerLevel": [
        {
          "MinimumPowerLevel": 0,
          "TableHandles": [
            {
              "DataTable": {
                "ObjectName": "DataTable'DT_LootDropTable'",
                "ObjectPath": "/Game/Gameplay/Items/LootDropTables/DT_LootDropTable.0"
              },
              "RowName": "RS_WhiteRoadRevenant"
            }
          ]
        }
      ]
    }
  }
}
```

Then `spawns` creates the AI and points it at that row:

```json
[
  {
    "Type": "AISpawnPoint",
    "Id": "white-road-revenant",
    "AIClass": "/Game/Gameplay/AI/DragonImaru/Minions/LostSoul/BP_AI_SpectralZombieImaru_Character.BP_AI_SpectralZombieImaru_Character_C",
    "DisplayName": "The White-Road Revenant",
    "LootRow": "RS_WhiteRoadRevenant",
    "PowerLevel": 8,
    "HealthMultiplier": 2.0,
    "DamageMultiplier": 1.35,
    "Location": { "X": 4629.0, "Y": 187800.0, "Z": -3493.0 },
    "Rotation": { "Pitch": 0.0, "Yaw": 0.0, "Roll": 0.0 },
    "Scale": 1.25,
    "Mandatory": true,
    "Respawn": true,
    "RequiresActivation": false,
    "IgnoreNavmeshRequirement": true,
    "MinSpawnDistance": 0.0,
    "MaxSpawnDistance": 2000.0
  }
]
```

Give every authored spawn an `Id` if another file may patch it. Use the resulting identity `ModName:Id`, for example `NamedSpawnShowcase:white-road-revenant`.

Tune one dimension at a time: location and grounding, then lifecycle, then name, then combat scaling, then loot. This makes it easier to distinguish a bad class path from a bad row or activation rule.

## 9. Consumable reward packs

The verified Mixed Supply Pack test shows that RuneSchema can create a consumable reward pack without another DLL or cooked asset package. The pack can emit both vanilla items and items cloned by another RuneSchema mod. A `raw` patch can then place the pack in enemy, boss, or compatible treasure loot.

This uses the existing loader boundaries:

- `assets` clones a native item-emitter pack and defines what it emits when consumed.
- `raw` distributes the completed pack through one or more loot rows.

There is no separate consumable-pack loader. The behavior comes from the cloned native pack class and ordinary reflected asset fields.

### 9.1 Folder structure and load order

```text
ZZ_MixedSupplyPackTest/
  assets/
    00-mixed-supply-pack.json
  raw/
    10-zombie-loot-patch.json
```

The `ZZ_` prefix is appropriate when the pack references items created by ordinary mod folders or patches loot rows created by another mod. Install every dependency on the server and participating clients.

### 9.2 Clone the consumable pack

Clone a native consumable pack so the new item inherits the game's working item-emitter behavior:

```json
{
  "/Game/RuneSchema/MyPack/Items/rs_my_supply_pack.rs_my_supply_pack": {
    "$Clone": "/Game/Gameplay/Items/Consumables/Misc/ITEM_Consumable_ZombiePack.ITEM_Consumable_ZombiePack",
    "InternalName": "rs_my_supply_pack",
    "PersistenceID": "REPLACE_WITH_UNIQUE_22_CHARACTER_ID",
    "Name": "My Supply Pack",
    "FlavourText": "A cache of useful supplies.",
    "Items to Drop": [
      {
        "ItemDataClass": {
          "AssetPathName": "/Game/Gameplay/Items/Resources/Magic/ITEM_Rune_Air.ITEM_Rune_Air",
          "SubPathString": ""
        },
        "MinToDrop": 12,
        "MaxToDrop": 20,
        "MaximumGrouping": 20,
        "ProbabilityOfDrop": 1.0,
        "RequiredTagQuery": {
          "TokenStreamVersion": 0,
          "TagDictionary": [],
          "QueryTokenStream": [],
          "UserDescription": "",
          "AutoDescription": ""
        },
        "ExtraDropPercentageForPlayerAmount": []
      },
      {
        "ItemDataClass": {
          "AssetPathName": "/Game/RuneSchema/ArmorCollection/Items/rs_ghostly_warden_dagger.rs_ghostly_warden_dagger",
          "SubPathString": ""
        },
        "MinToDrop": 1,
        "MaxToDrop": 1,
        "MaximumGrouping": 1,
        "ProbabilityOfDrop": 0.25,
        "RequiredTagQuery": {
          "TokenStreamVersion": 0,
          "TagDictionary": [],
          "QueryTokenStream": [],
          "UserDescription": "",
          "AutoDescription": ""
        },
        "ExtraDropPercentageForPlayerAmount": []
      }
    ]
  }
}
```

Replace the placeholder with a permanent unique `PersistenceID` before testing. The example combines a vanilla Air Rune path with the final runtime path of an Armor Collection clone.

`ProbabilityOfDrop` is a fraction from `0.0` to `1.0`. A value of `1.0` is guaranteed and `0.25` is a 25% roll. Each entry rolls independently when the player consumes the pack. `MinToDrop` and `MaxToDrop` set the quantity range. Keep `MaximumGrouping` at least as large as `MaxToDrop` unless testing a specific native grouping behavior.

The in-game verified mixed pack contained:

- 12–20 Air Runes, guaranteed.
- 6–10 Wild Anima, guaranteed.
- One Weapon Poison, 50% chance.
- One Ghostly Warden dagger, 25% chance.

This demonstrates that one pack may combine guaranteed resources, optional consumables, and a rare modded equipment roll.

### 9.3 Add the pack to a loot row

Use `raw` and `$Append` to preserve the loot row's existing contents:

```json
{
  "DT_LootDropTable": {
    "Zombie_Zombie": {
      "$Append": {
        "Resources": [
          {
            "SpawnedItemData": {
              "ObjectName": "BP_Consumables_ItemEmitter_C'rs_my_supply_pack'",
              "ObjectPath": "/Game/RuneSchema/MyPack/Items/rs_my_supply_pack.0"
            },
            "MinimumDropAmount": 1,
            "MaximumDropAmount": 1,
            "DropChance": 100.0,
            "bOneInstancePerPlayerOnlyVisibleToThem": false,
            "bAutoAddToInventory": false,
            "bOnlyForPlayersThatInflictedDamage": false
          }
        ]
      }
    }
  }
}
```

Raw loot-table `DropChance` uses a percentage from `0.0` to `100.0`. This differs from the pack asset's `ProbabilityOfDrop`, which uses `0.0` to `1.0`. Here, `100.0` guarantees that one pack is emitted whenever `Zombie_Zombie` runs; the contents still make their own independent rolls when the pack is consumed.

The verified test also appended the pack to `RS_Zogre_Grimtooth`, `RS_Zogre_Borgruk`, and `RS_Zogre_Mournmaw`. Those custom rows depend on Zogre Spawn Lifecycle Test. A missing row is a dependency or ordering error and should not be silently replaced with a new encounter definition.

Use a guaranteed outer drop rate during development, then lower it or remove the test patch for normal balance. Replacing a complete loot row is appropriate only when the mod deliberately owns every entry in that row.

### 9.4 Consumable-pack test sequence

1. Stop the game and dedicated server before installing or editing the pack.
2. Confirm the source mods for every modded item are enabled before the `ZZ_` pack mod.
3. Start with Advanced verbose logging disabled.
4. Confirm the asset summary reports one new clone with no errors.
5. Confirm the raw table updates without unresolved item or row warnings.
6. Kill an enemy using a 100% test placement and collect the pack.
7. Consume several packs and verify guaranteed quantities and independent optional rolls.
8. Confirm the modded reward has the correct identity after save and reload.
9. Repeat on a dedicated server with matching client files before reducing the test drop rate.

## 10. Named resource nodes and component drops

Named Resource Node Showcase demonstrates a spawned world actor with a display name, material effect, native respawn, and component-level drop data:

```json
[
  {
    "Type": "Actor",
    "Id": "DWS_ResourceTest_IronOre",
    "Class": "/Game/Gameplay/World/Mining/Ores/Iron/BP_OreNode_Iron_Medium_A1.BP_OreNode_Iron_Medium_A1_C",
    "DisplayName": "Ironheart Vein",
    "VisualEffect": {
      "Type": "Ghost",
      "MainColor": { "R": 0.3, "G": 0.68, "B": 1.0, "A": 1.0 },
      "SecondaryColor": { "R": 0.08, "G": 0.2, "B": 0.7, "A": 1.0 }
    },
    "UseNativeRespawn": true,
    "Location": { "X": 16606, "Y": 185344, "Z": -3300 },
    "Rotation": { "Pitch": 0.0, "Yaw": 90.0, "Roll": 0.0 },
    "ComponentProperties": {
      "ItemDropComponent": {
        "ItemsToDrop": {
          "Items": [
            {
              "ItemDataClass": {
                "AssetPathName": "/Game/Gameplay/Items/Resources/Gem/ITEM_Resources_Jade.ITEM_Resources_Jade",
                "SubPathString": ""
              },
              "MinToDrop": 1,
              "MaxToDrop": 1,
              "MaximumGrouping": 1,
              "ProbabilityOfDrop": 1.0
            }
          ]
        }
      }
    }
  }
]
```

Component names differ by actor class. Trees may expose `ItemDropOnDestructionComponent` or `ItemDropOnSplitComponent`; ore nodes may expose `ItemDropComponent`. Use Inspector and focused exports to verify the actual component and field names before authoring.

This pattern supports named mining veins, themed harvest nodes, rare resource landmarks, spectral trees, custom guaranteed drops, and resource-node encounters. It does not turn a non-actor foliage instance into an actor.

## 11. Player profiles

Player Profiles uses positional name selectors to give the first four players distinct roles:

```json
[
  {
    "Id": "first-player-tank",
    "PlayerName": "*1",
    "Scale": 1.2,
    "MaxHealth": 300,
    "MaxStamina": 160,
    "DefenseMultiplier": 1.5,
    "PhysicalDefenseMultiplier": 1.35,
    "CarryWeightMultiplier": 1.4,
    "PoisonResistanceMultiplier": 1.2
  },
  {
    "Id": "second-player-runner",
    "PlayerName": "*2",
    "Scale": 0.85,
    "MaxHealth": 180,
    "MaxStamina": 320,
    "WalkSpeedMultiplier": 1.15,
    "RunSpeedMultiplier": 1.25,
    "StaminaRecoveryMultiplier": 1.5,
    "RangedAttackMultiplier": 1.3
  }
]
```

Because the entries have IDs, a `ZZ_` mod can adjust one profile without copying the entire document:

```json
[
  {
    "$Patch": "PlayerProfiles:first-player-tank",
    "$Target": {
      "MaxHealth": 325,
      "DefenseMultiplier": 1.6
    }
  }
]
```

Player rules are applied through lifecycle events. They should not be used as a polling system. Verify join, respawn, world travel, save reload, and dedicated-server behavior when a profile changes gameplay values.

### 11.1 Player nameplates

`Nameplate` can retain the normal player name, replace it with a cooked texture,
or hide the nameplate. The icon is added to the existing
`WBP_Player_Nameplate` at runtime, so a mod does not need to author or replace
the game widget:

```json
[
  {
    "Id": "skulled-player",
    "PlayerName": "*1",
    "Nameplate": {
      "Mode": "Icon",
      "Icon": "/Game/Mods/Skulling/UI/T_Skull.T_Skull",
      "Scale": 1.0,
      "Distance": 2500,
      "States": {
        "Dead": {
          "Icon": "/Game/Mods/Skulling/UI/T_Skull_Dead.T_Skull_Dead",
          "Scale": 1.2,
          "InactivitySeconds": 2.0
        }
      }
    }
  }
]
```

`Mode` is `Name`, `Icon`, or `Hidden`. `Icon` requires the full object path of
a cooked texture. `Scale` accepts `0.1` through `4.0`, and `Distance` accepts
`0` through `100000` Unreal units. Omitted values default to scale `1.0` and
the game's observed nameplate distance of `2500`. `Client` controls whether a
player sees their own badge; `Server` controls whether other modded clients in
the session see that player's badge. Both accept `"Yes"`/`"No"` or JSON
booleans. `Client` defaults to `"No"`, while `Server` defaults to `"Yes"`.
`ShowSelf` remains available as a boolean compatibility alias for `Client`.

`Server: "Yes"` does not make the dedicated server render UI and cannot add a
widget to clients that do not have the mod installed. It means each modded
client renders the rule on its replicated copy of the other player's pawn.

`States` supports `Dead`, `Attack`, `Ranged`, `Magic`, `Woodcutting`, and
`Mining`. Each state supplies a cooked icon and optional scale. This can be
combined with any base mode: for example, `Name`
shows the normal player name while alive and the state icon while dead; `Icon`
uses one icon while alive and another while dead. RuneSchema reads the verified
`PlayerDamage.FatalDamageInfo.bIsSet` state and responds to the player's native
gameplay-tag and damage events. Returning to the living state restores the base
mode automatically. For `Dead`, `InactivitySeconds` may retain the icon after
respawn. For an activity, it is the display period started or renewed by each
matching game event. Values from `0` through `3600` are accepted. The default is
`0` for `Dead` and `2` seconds for activity states.

To make an event icon vanish completely after a delay, set the base `Mode` to
`Hidden` and put the delay on that state, such as `"InactivitySeconds": 5`.
With base mode `Name`, the same five-second delay restores the player's name;
with base mode `Icon`, it restores the persistent role icon instead.

The base `Mode`, `Icon`, and `Scale` are the player's persistent identity badge,
not another activity state. This makes it suitable for ordinary-player, host,
moderator, admin, clan, or other role icons. Use an `AllPlayers` rule for the
ordinary badge, then later rules selected by `PlayerName`, `PlayerGuid`, or
`PlayerLoadSlot` to override particular people. Activity-state icons temporarily
take precedence and always return to the selected base badge after the state and
its inactivity delay finish.

For example, this gives everyone a client badge and then replaces it for the
host/admin identified by a stable player GUID:

```json
[
  {
    "Id": "ordinary-player-badge",
    "PlayerName": "*",
    "Nameplate": {
      "Mode": "Icon",
      "Icon": "/Game/Mods/Roles/UI/T_Client.T_Client"
    }
  },
  {
    "Id": "host-admin-badge",
    "PlayerName": "replace-with-the-host-name",
    "Nameplate": {
      "Mode": "Icon",
      "Icon": "/Game/Mods/Roles/UI/T_Admin.T_Admin"
    }
  }
]
```

When more than one entry matches a player, the last matching nameplate entry in
resolved mod load order wins. For badges that every client must see, use the
stable displayed `PlayerName`; numbered load slots are convenient but can change
as players reconnect. `PlayerGuid` remains suitable where the receiving runtime
can resolve that character's controller.

The rule is reapplied when the player's pawn or replicated player state is
created, including after a normal respawn. Nameplates are client-side UI:
dedicated servers retain the rule, while every client that uses the mod applies
it to its local copy of every replicated player nameplate, not only the local
character. Consequently, a server-and-client mod can show each player's state
to the other players without the server attempting to render UI. Attack,
ranged, magic, woodcutting, and mining activity is triggered by the game's
native multicast attack/spell hooks. RuneSchema does not poll action fields. It
only advances an active icon's fallback timer, then restores the base badge when
that timer expires. Melee attacks are divided into ordinary `Attack`,
`Woodcutting`, and `Mining` by the native action supplied with the attack event.
Blocking and cooking are not states in this version because the inspected game
fields did not provide reliable start/end signals.

The complete cooked-icon example is provided as `PlayerActivityNameplates` in
the example-mod package. It uses existing game textures, so the test does not
require a custom pak.

## 12. Icons and cooked art

Upgraded Runes shows a small data patch backed by cooked icon files:

```json
{
  "/Game/Gameplay/Items/Resources/Magic/ITEM_Rune_Air.ITEM_Rune_Air": {
    "Icon": {
      "AssetPathName": "/Game/Mods/UpgradedRunes/Art/UI/Runes/T_Icon_Rune_Air.T_Icon_Rune_Air",
      "SubPathString": ""
    },
    "AmmoCounterIcon": "/Game/Mods/UpgradedRunes/Art/UI/Runes/T_Icon_Ammo_Infused_Air.T_Icon_Ammo_Infused_Air"
  }
}
```

The JSON tells the existing item which cooked texture to use. The pak must contain that texture at the exact cooked path. A correct external folder name does not repair an incorrect internal `/Game/...` mount path.

The same approach can supply icons for cloned armor, weapons, perk descriptions, journal pages, and rune counters.

Changing a rune icon or ordinary item fields does not create a new spell-ammunition relationship. Rune consumption, staff acceptance, spell modules, damage chains, and ammo decrement can involve private native or Blueprint pathways beyond the item asset.

## 13. Blueprint component tuning with JSONC

RuneSchema accepts JSONC where supported, allowing comments during authoring. Light Adjustment patches existing torch components:

```jsonc
{
  "/Game/Gameplay/Character/Player/Equipment/Held/Torch/BP_Torch": {
    "PointLight_top": {
      "AttenuationRadius": 3000,
      "CastShadows": true,
      "Temperature": 2000
    }
  },
  "BP_BaseBuilding_TorchStanding_C": {
    "PointLight": {
      "AttenuationRadius": 1200.0,
      "CastShadows": true,
      "MaxDrawDistance": 15000.0,
      "intensity": 1
    }
  }
}
```

Component and property names are exact and class-specific. Large light radii, shadow casting, and long draw distances can reduce performance even when the JSON is valid. Provide conservative presets when distributing visual tuning.

## 14. Journal integration

A recipe can be represented in the journal with a separate entry:

```json
{
  "RS_Journal_Dawnveil_Armor": {
    "Type": "Recipe",
    "DisplayName": "The Dawnveil Oathplate",
    "PageDescriptions": [
      {
        "Description": "Paladin plate reforged beneath a shadowscale hood.",
        "bUnlocked": true
      }
    ],
    "Image": "/Game/Art/UI/Icons/Armours_07/test/test2/T_Icon_Paladin_s_Chestplatel.T_Icon_Paladin_s_Chestplatel",
    "RecipeData": "RECIPE_RS_Dawnveil_Body",
    "ItemData": "/Game/RuneSchema/ArmorCollection/Items/rs_dawnveil_paladin_body.rs_dawnveil_paladin_body",
    "StationTableRowHandle": {
      "DataTable": "DT_CraftingStationsDataTable",
      "RowName": "ArmourBench"
    },
    "bUseDefaultDescriptionFormat": false,
    "UnlockCondition": { "UnlockType": "Manual" },
    "Unlock": true,
    "AddTo": {
      "SubCategory": "/Game/UI/JournalData/JOURNAL_SC_Recipe_Armor.JOURNAL_SC_Recipe_Armor",
      "Biome": "Ghornfell",
      "Key": "RS_Journal_Dawnveil_Armor"
    }
  }
}
```

Keep the journal key, `RecipeData`, `ItemData`, station row, and recipe definition synchronized. Test both a fresh profile and an existing save because discovery state may already be persisted.

## 15. Manual authoring tools

RuneSchema tools are disabled at each launch. Activate them from RuneSchema's UE4SS tab only when needed.

### Inspector

Use Inspector to capture the camera's first blocking target or the local player, search loaded object names, inspect reflected fields, and copy exact paths. The search is bounded and returns snapshots; it does not continuously poll.

### Presets and exports

Use presets for repeatable, narrowly scoped research. Presets can capture selected objects, known classes, tables, equipment APIs, spell graphs, appearance state, and other focused data. The default maximum depth is 7. A session-only option permits depths up to 16, but deeper traversal can stall the game or consume significant memory.

Keep these limits conservative:

- Follow object references only when required.
- Restrict class and name filters.
- Prefer one known object over a broad search.
- Increase depth one step at a time.
- Restart after heavy dumps when game behavior becomes inconsistent.

### Traces

Player Trace records bounded player actions and relevant state changes. Appearance Trace records equipment and material lifecycle observations. Traces are diagnostic evidence, not gameplay features.

### Normal and verbose logging

Normal output is consolidated. Loader summaries report counts such as new, updated or altered, and errors. Spawn summaries separate AI, bosses, resource nodes, other actors, and removals. Successful native cooked-spawn narration and per-entry detail require Advanced verbose logging.

Leave verbose logging off for ordinary play. Warnings and errors remain visible without it.

## 16. Client and dedicated-server authoring

Use the same RuneSchema data set on clients and servers when it changes gameplay identity, recipes, stats, loot, spawns, or equipment behavior.

- The server needs authoritative gameplay definitions and matching persistence identities.
- Clients need matching item and recipe definitions to display and use the content correctly.
- Clients need cooked icons, meshes, and material assets they render.
- Dedicated servers skip cosmetic appearance work.
- Native equipment behavior requires the matching executable contract for the client or server build.
- A contract mismatch disables that native feature and logs an error; it should not be treated as successful partial support.

The supplied StackFix1 UE4SS host is part of the 0.6.1E runtime requirement. Its `version.dll` is the dedicated-server entry point; single-player clients ignore that entry point. Replace host and RuneSchema DLLs only while the game and server are stopped.

## 17. A safe development workflow

Build and test a mod in small layers:

1. **Capture a native reference.** Use Inspector or a focused preset to verify the source object, property names, enum spelling, row structure, and component names.
2. **Create the identity.** Add one item clone, row, spawn, recipe, or journal entry with a permanent ID.
3. **Validate startup.** Check RuneSchema's consolidated summary and every warning or error.
4. **Test the narrow behavior.** Craft one item, equip one piece, kill one AI, harvest one node, or load one profile.
5. **Add dependent layers.** Connect stats, recipes, journal pages, effects, appearance, and patches only after the base identity works.
6. **Test reload boundaries.** Restart for Blueprint, appearance, ordering, or native equipment changes.
7. **Test persistence.** Save, leave the world, reload, re-equip, respawn, and reconnect.
8. **Test multiplayer.** Verify a dedicated server and at least one client when gameplay data is shared.
9. **Package cooked content last.** Confirm internal `/Game/...` paths before distributing the pak.
10. **Keep backups.** Never replace the RuneSchema or UE4SS DLL while either process is running.

### Recommended mod layout for a full set

```text
MyArmorSet/
  assets/
    00-items.json
  raw/
    10-wearable-rows.json
  recipes/
    20-crafting.json
    21-destruction.json
  journal/
    30-journal.json
  equipment/
    40-runtime-behavior.json
  paks/
    MyArmorSetArt/
      MyArmorSetArt.pak
      MyArmorSetArt.utoc
      MyArmorSetArt.ucas
```

If a separate compatibility mod patches this set, name that folder `ZZ_MyArmorSetCompatibility` and target the base mod's stable identities.

## 18. Troubleshooting

### The JSON parses, but the item is missing

- Confirm the mod is enabled.
- Confirm the file is under the correct loader folder.
- Check the source asset or cooked asset exists and is loaded.
- Check the clone path, `PersistenceID`, and `InternalName` are unique.
- Check whether a later patch changed or removed the definition.

### The armor appears, but its stats are missing

- Confirm the `raw` row was added to `DT_WearableEquipment`.
- Confirm the item's row handle uses the exact same row name.
- Use the observed tier prefix such as `T06_` when required by native power-level behavior.
- Inspect DataTable warnings during startup.

### The recipe exists but cannot create the item

- Confirm `ItemsCreated.ItemData` uses the final runtime path.
- Confirm every consumed item path exists.
- Confirm the station table and row are correct.
- Confirm server and client use the same item identity.

### A `$Patch` reports no match

- Inspect the final array after all earlier mods load.
- Use the canonical object path in `$Match`.
- Confirm the target row or Blueprint exists in this game build.
- Move compatibility patches into a `ZZ_` mod.

### A `$Patch` reports an ambiguous match

- Add another stable field to `$Match`.
- Remove duplicate mods that append the same entry.
- Use `$Index` only if the array's order is stable and controlled.

### Appearance is logged but not visible

- Confirm the target owns a supported physical mesh.
- Test `Overlay` and `BodyMaterial` separately.
- Check whether Shadowveil or another temporary material effect currently owns the mesh.
- Re-equip the item after changing an equipment appearance rule.
- Remember that inventory icons and frontend presentation can use different meshes or render paths.

### Equipment behavior is disabled

- Read the native contract error.
- Confirm the supplied game executable and UE4SS host match the 0.6.1E build.
- Confirm the equipment path is the final runtime path.
- Do not try to repair a contract mismatch by broadening the JSON path.

### Startup pauses or becomes unstable

- Disable heavy diagnostics and verbose tracing.
- Remove duplicate content mods and overlapping append definitions.
- Check normal loader summaries for errors.
- Keep tools manually activated.
- Restart after a diagnostic dump.
- Never swap DLLs while the game or server is running.

## 19. What can be built now

The verified 0.6.1E pathways can support:

- Complete light, medium, and heavy armor families with separate stats and recipes.
- Masterwork-marked armor and weapons using verified item fields.
- Native equipment perks with visible titles, descriptions, and skill icons.
- Surge-on-evade leg items and action-selective Shadowveil equipment.
- Ghostly or glowing armor, weapons, tools, characters, AI, trees, and resource nodes.
- New item variants cloned from existing equipment.
- Externally cooked items integrated through assets, recipes, journals, and destruction recipes.
- Named bosses with custom scaling, lifecycle rules, and loot routing.
- Named harvesting landmarks with component-level drops.
- Large raw-table loot expansions followed by narrow compatibility patches.
- Consumable reward packs containing mixed guaranteed, optional, vanilla, and modded items.
- Player role profiles for cooperative servers.
- Lighting and Blueprint component tuning.
- Multiple cooked packs organized beneath one RuneSchema mod.

Some ideas still need additional verified game pathways:

- A truly new rune or ammo type accepted by every spell and staff.
- Rune consumption, decrement, spell-module cloning, and damage-chain replacement through item JSON alone.
- Arbitrary permanent weapon enchantment systems.
- Character-creation skin-color integration.
- Guaranteed damage support for every building type from Rocksplosion without a verified damage-filter pathway.
- Automatic compatibility with future game builds whose native layouts or signatures change.

Treat these as research targets rather than supported authoring claims.

## 20. Related documentation

- [Appearance](APPEARANCE.md)
- [Field patches](PATCHING.md)
- [Equipment behavior](EQUIPMENT.md)
- [Pak organization](PAKS.md)
- [Manual tools](TOOLS.md)
- [Diagnostic presets](PRESETS.md)
- [Tracing](TRACING.md)
- [Loading](LOADING.md)
- [Dedicated-server equipment](SERVER-EQUIPMENT.md)
- [Build and size requirements](BUILDING.md)
- [Implementation constraints](IMPLEMENTATION.md)
- [0.6.1E release notes](../RuneSchema-0.6.1E.md)
