# Create stations and recipes

A station is more than a world model. A complete station needs a buildable or
placed actor, an interaction, a recipe list, and a way for the player to unlock
or find it. RuneSchema can connect those pieces without replacing a native game
table.

## Decide which station type you need

Use a crafting station when the player selects a recipe and crafts it directly.
Crafting stations usually group recipes by a named category.

Use a processing station when the player inserts materials and waits for an
output. Processing stations usually keep recipes in a `Recipes` list and may
also have fuel, speed, slot, and automatic-start settings.

Choose a native station with the same behavior as your starting reference.

## Build the station PAK content

A new station commonly includes:

- a station actor Blueprint;
- a building-piece data asset if players can build it;
- meshes, materials, icons, sounds, and effects;
- a crafting or processing table with a station row;
- an interaction and interface compatible with the selected station family.

Keep the station actor and its data in your own content root. Compile the
Blueprint and verify every referenced file is included before cooking.

## Connect a buildable station

A RuneSchema `buildings` file may point to the cooked building data and adjust
supported behavior:

```json
{
  "GoldenForge": {
    "Asset": "/GoldenArsenal/Buildings/BUILDPIECE_GoldenForge.BUILDPIECE_GoldenForge",
    "Unlock": true,
    "Overrides": {
      "Names": {
        "Catalogue": "Golden Forge",
        "World": "Golden Forge",
        "Interact": "Use Golden Forge",
        "Menu": "Golden Forge"
      },
      "Placement": {
        "RequiresFoundation": true,
        "bAllowUserRotationModification": true
      },
      "Processing": {
        "Rate": 1.0
      }
    }
  }
}
```

Only include overrides the station needs. A PAK asset that already has correct
placement, stability, and processing settings does not need those values copied
into JSON.

## Add a recipe to a native crafting station

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
      "ExtraItemsCreated": []
    }
  }
}
```

`Category` is the section shown in a crafting menu. Use a category that the
station supports or deliberately add a new category tested with that interface.

## Add a recipe to a processing station

```json
{
  "RECIPE_GoldenIngot": {
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
          "ItemData": "/GoldenArsenal/Items/ITEM_GoldenOre.ITEM_GoldenOre",
          "Count": 2
        }
      ],
      "ItemsCreated": [
        {
          "ItemData": "/GoldenArsenal/Items/ITEM_GoldenIngot.ITEM_GoldenIngot",
          "Count": 1
        }
      ],
      "ExtraItemsCreated": []
    }
  }
}
```

`Array: "Recipes"` places the recipe in the station's processing list. Fuel,
timing, input slots, and automatic-start behavior come from the station and may
need separate building overrides.

## Add a recipe to a custom cooked station table

Use the full path when the station table lives in your PAK:

```json
"AddTo": [
  {
    "DataTable": "/GoldenArsenal/Data/DT_GoldenForge.DT_GoldenForge",
    "Row": "GoldenForge",
    "Category": "Weapons"
  }
]
```

Use either `Table` or `DataTable`, not both. `Table` is a short lookup suited to
known native tables. `DataTable` is the exact path for a custom cooked table.

## Recipe unlock behavior

Placing a recipe in a station does not have to grant it immediately. RuneSchema
keeps station placement separate from recipe discovery.

Use the recipe's supported unlock setting only when the recipe should be granted
automatically. For normal progression, connect the recipe to a quest, journal
entry, discovery event, or recipe-unlock item.

This avoids filling a new character with every modded recipe at first login.

## Why RuneSchema helps

Without RuneSchema, a PAK recipe can exist without the game ever adding it to a
station or save-aware recipe list. RuneSchema can:

- load the recipe and its item references;
- verify that ingredient and output paths are real items;
- add the recipe to an existing or custom station row;
- give a newly authored recipe a stable identity;
- keep recipe placement separate from automatic unlocking;
- report missing station rows and item paths clearly;
- remove a saved recipe reference if the mod is later removed.

## Station and recipe test checklist

1. Confirm the station PAK mounts.
2. Confirm the building data and actor paths load.
3. Place the station and test rotation, snapping, stability, and deconstruction.
4. Open the station and confirm the intended category or processing list.
5. Confirm every ingredient icon, name, and count.
6. Craft or process one output.
7. Test full inventory, missing fuel, interrupted processing, and cancelled use.
8. Save, return to the menu, and re-enter the world.
9. Restart and verify the placed station and recipe again.
10. Test host and client interaction.
11. Disable the mod only on a backup and confirm cleanup warnings name only the
    missing content.
