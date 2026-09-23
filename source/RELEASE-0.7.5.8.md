# RuneSchema 0.7.5.8

This release makes custom crafting and processing station recipe placement deterministic.

- Existing `AddTo[].Table` definitions remain supported and continue resolving a DataTable by its short object name.
- New `AddTo[].DataTable` accepts an exact cooked DataTable object path such as `/Game/MyMod/Data/DT_MyStation.DT_MyStation`.
- Exact-path targets are loaded on demand and are also recognized by the late DataTable-serialization callback.
- A placement must select exactly one table target (`Table` or `DataTable`) and exactly one layout (`Array` for stations or `Category` for merchants).
- A missing asset, row, or incompatible row field skips only that placement; it does not disable the recipe loader.

Example:

```json
{
  "MyMod_Recipe": {
    "Properties": {},
    "AddTo": [
      {
        "DataTable": "/Game/MyMod/Data/DT_MyProcessingStations.DT_MyProcessingStations",
        "Row": "MyForge",
        "Array": "Recipes"
      }
    ]
  }
}
```
