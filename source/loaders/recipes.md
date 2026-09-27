# Recipes loader

**Folder:** `RuneSchema/mods/<ModName>/recipes/`

Crafting, processing, station placement, merchants, and vendor contributions.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

Recipes can feed vanilla crafting or processing rows, vanilla merchants, and
RuneSchema vendors.

```jsonc
{
  "RECIPE_MY_ITEM": {
    "PersistenceID": "AAAAAAAAAAAAAAAAAAAAAA",
    "AddTo": [{
      "DataTable": "/Game/MyMod/Data/DT_MyStation.DT_MyStation",
      "Row": "StationRow",
      "Array": "Recipes"
    }],
    "Properties": {
      "ItemsConsumed": [{"ItemData":"/Game/Gameplay/Items/ITEM_Log.ITEM_Log","Count":2}],
      "ItemsCreated": [{"ItemData":"/Game/MyMod/Items/ITEM_MyItem.ITEM_MyItem","Count":1}]
    },
    "Unlock": false
  }
}
```

Placement rules:

- Use `Table` for a unique legacy short table name.
- Use `DataTable` for an exact vanilla or modded cooked table path.
- Use `Category` for a native `LabeledRecipes` category. Both crafting-station
  rows and merchant rows use this layout.
- Use `Array` for a direct `RecipeData` object array. Dragonwilds processing
  stations use `Array: "Recipes"`.
- Use `RuneSchemaVendors` for one or more RuneSchema store IDs.
- Use `VanillaVendors` for native merchant table/row targets.
- `Unlock:true` grants the recipe; placement alone does not.
- `PersistenceID` is optional. When omitted, RuneSchema derives a stable ID
  from the mod folder and recipe key.
- `Order` sorts within a category. Lower values appear first.
- `ItemsConsumed[].ItemData` and `ItemsCreated[].ItemData` should use full item
  object paths. RuneSchema accepts a timed-processing output only after an
  `/assets` clone is fully registered and its persistent identity resolves back
  to that exact item.
- `ItemsConsumed[].Count` and `ItemsCreated[].Count` are exact quantities per
  operation for both crafting and processing. RuneSchema reads full-array
  writes back from the live `RecipeData` and refuses placement if an entry or
  count was not retained exactly.
- The output asset still controls its own type and stack rules. An asset cooked
  as `FuelItemData` remains fuel even if a recipe also uses it as a resource.

The verified vanilla station targets are:

```text
/Game/Gameplay/World/Stations/DT_CraftingStationsDataTable.DT_CraftingStationsDataTable
  Category layout: CraftingTable, RuneAltar, GarouMasonsBench, PotteryWheel,
  RangeBronze, RangeStone, JewelersBench, BrewingCauldron, ToolsMeleeBench,
  MysticForge, FletchingBenchv2, ArmourBench

/Game/Gameplay/World/Stations/DT_ProcessingStationDataTable.DT_ProcessingStationDataTable
  Array "Recipes": Campfire, Kiln, Grinder, Loom, Sawmill, Spinning Wheel,
  Tannery, Furnace, Air Altar, Fire Altar, BrewingCauldron, Grill, Stonecutter,
  AdvancedSmelter, FermentationBarrel, AdvancedTannery,
  AdvancedSpinningWheel
```

For example, the older dye definitions target the crafting-side
`BrewingCauldron` and correctly create a `Dyes` category:

```json
{"Table":"DT_CraftingStationsDataTable","Row":"BrewingCauldron","Category":"Dyes"}
```

To make the same recipe run through the timed processing cauldron, use:

```json
{"Table":"DT_ProcessingStationDataTable","Row":"BrewingCauldron","Array":"Recipes"}
```

`Category` and `Array` are mutually exclusive. RuneSchema validates the live
row structure and the array's accepted object class before writing. A missing
table, row, category layout, or recipe array rejects only that placement.
Exact `DataTable` paths are recommended for custom or potentially ambiguous
tables. Helpy's station picker derives its choices from the same reflected
`LabeledRecipes` and `RecipeData[]` layouts.

## Simple rules

- Recipe placement and recipe unlocking are separate operations.
- Use `DataTable` for an exact cooked table path; use `Table` only for a unique legacy short name.
- `Category` targets native `LabeledRecipes`; `Array` targets a direct recipe array. Do not use both on the same placement.

## FAQ

### FAQ-RECIPES-001 — Does placing a recipe at a station automatically unlock it? {#faq-recipes-001}

No. Placement makes the station or vendor contain the recipe; `Unlock:true`
controls whether the recipe is granted.

### FAQ-RECIPES-002 — When should I use DataTable instead of Table? {#faq-recipes-002}

Use `DataTable` when you want an exact vanilla or modded cooked table path,
especially for custom or potentially ambiguous tables. Use `Table` only when
the short table name is unique.

### FAQ-RECIPES-003 — Can a placement use both Category and Array? {#faq-recipes-003}

No. `Category` and `Array` are mutually exclusive placement modes.

### FAQ-RECIPES-004 — Can a timed processing station create an `/assets` clone? {#faq-recipes-004}

Yes. Reference the clone by its RuneSchema object path. RuneSchema verifies the
registered clone before adding the recipe to a processing queue. An unresolved
or duplicate item identity rejects that recipe placement instead of leaving an
unsafe queued output.

### FAQ-RECIPES-005 — Must every recipe provide PersistenceID? {#faq-recipes-005}

No. It is optional. A supplied value must be unique and stable; otherwise
RuneSchema derives one from the owning mod and recipe key.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
