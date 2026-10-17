# Bounded cooked registry shards

RuneSchema 0.8.0.0 supports a **flat, explicit shard index**. A conventional root
inside each enabled mod's PAK lists its child data assets by exact object path.
RuneSchema loads those assets directly on Steam, Game Pass and dedicated-server
builds using the same code path. It does not require a whole-project
`AssetRegistry.bin`, wildcard package scans or Asset Registry enumeration of
each shard. Live validation on all three environments remains necessary.

This extends the [cooked PAK declaration](COOKED-PAK-REGISTRY-MANIFEST.md).
The per-document ceiling is 512 KiB in the October 16 correction; lane,
reference, shard-count and aggregate budgets remain independently enforced.

## Cook the root

For an enabled mod folder named `Dye`, cook exactly this root data asset:

```text
/Game/Mods/Dye/Registry/DA_RuneSchemaRegistry_Dye.DA_RuneSchemaRegistry_Dye
```

The asset must expose these reflected **String/FString** properties:

- `RegistryOwner`: `Dye` (exact enabled mod folder name).
- `RuneSchemaRegistryJson`: the JSON below, serialized as a string.
  `RegistryJson` remains a supported alias.

A mod-owned Blueprint class derived from `PrimaryDataAsset` with those string
properties is supported. The reader does not require RuneSchema's shared
Blueprint base or a new framework PAK. If the class lives in a Core module,
other modules still require that Core class to be cooked and available.

```json
{
  "SchemaVersion": 1,
  "Entries": [],
  "RegistryShards": [
    "/Game/Mods/Dye/Registry/Shards/RSREG_Dye_0001.RSREG_Dye_0001",
    "/Game/Mods/Dye/Registry/Shards/RSREG_Dye_0002.RSREG_Dye_0002"
  ]
}
```

The root may additionally contain its own normal `NativeRegistries`,
`LoaderDefinitions` and up to 256 ordinary presentation `Entries`. Omit native
fields entirely for an index-only root; do not add empty native declarations.
Root presentation entries must all validate before the group is published.

## Cook each child

Each child must be directly inside `/Game/Mods/<Owner>/Registry/Shards/`.
Its package and object names must be identical and follow
`RSREG_<Owner>_<Id>`, using ASCII letters, digits and underscores only.
The complete asset name is limited to 128 characters. No subfolders,
traversal, cross-owner paths, alternative mounts, subobjects or nested indexes
are accepted. Duplicate paths are rejected case-insensitively.

For the first example child, set `RegistryOwner` to `Dye` and
`RuneSchemaRegistryJson` to:

```json
{
  "SchemaVersion": 1,
  "Entries": [],
  "RegistryShard": true,
  "LoaderDefinitions": {
    "Recipes": {
      "/Example/Recipes/RECIPE_BlackArmor.RECIPE_BlackArmor": {
        "Unlock": false,
        "Discovery": {
          "AllItems": [
            "/Example/Items/ITEM_BlackDye.ITEM_BlackDye",
            "/Example/Items/ITEM_Armor.ITEM_Armor"
          ]
        },
        "AddTo": [{
          "DataTable": "/Example/Stations/DT_Stations.DT_Stations",
          "Row": "DyeStation",
          "Category": "Dyes"
        }]
      }
    }
  }
}
```

Replace the example recipe, item, table, row and category with actual native
paths and values. Each child must have `RegistryShard: true`, `Entries: []`
and a nonempty `NativeRegistries` and/or `LoaderDefinitions` payload. It cannot
have `RegistryShards`. All supported native lanes can be split across children,
including journals and lore. No duplicate loose `/assets`, `/recipes`,
`/journal` or `/lore` pointer file is needed for these announced assets.

`LoaderDefinitions.Recipes` requires **existing cooked RecipeData object
paths**, not local runtime recipe keys. Converting a previously runtime-authored
recipe to a cooked asset is separate authoring work: retain its exact shipped
PersistenceID and compatible internal identity. Sharding does not clone assets,
generate new saved identities, change quantities or synthesize missing recipes.

## Limits and failure behavior

| Scope | Limit |
| --- | --- |
| Each UTF-8 JSON string | 512 KiB |
| Each native lane array / loader map in one document | 4,096 entries |
| All native paths plus loader definitions in one document | 16,384 references |
| Children listed by one root | 256 |
| Root plus children combined | 64 MiB of UTF-8 JSON; 262,144 references |
| JSON nesting | 64 parser levels |

Budget against the actual serialized UTF-8 strings, not only item counts.
Duplicate JSON keys are rejected. The conventional root can therefore discover
up to 256 explicitly named pages without any shard metadata enumeration.
The root and each child must independently fit their document limit.
Grouping is flat: no recursion, include cycles or directory scans.

The complete index is path-validated **before any child callback runs**.
Every listed child must load, match `RegistryOwner`, have a valid envelope and
parse its native declarations. Conflicting recipe/journal/lore bodies across
pages reject the group. Native declarations publish as one batch, with no
earlier-page prefix left behind if preparation or conflict checks fail.
Independent metadata discovery cannot enroll a child on its own after its
root fails. Disabled owners do not contribute their shard group.

Declaration publication is not gameplay-object registration. The normal
loaders still class-check assets, reconcile native identities, place recipes
and apply supported discovery rules. A later registration failure also vetoes
automatic character cleanup; there is no promise to roll back every native
subsystem or third-party gameplay side effect. Missing/invalid enabled shards
are not evidence that their saved IDs are orphaned.

Unindexed child metadata for an enabled owner is ignored and reported once per
owner, deferring automatic character cleanup. A legacy mod with no conventional
root remains supported; without either a root or usable metadata the loader
cannot infer that a completely absent declaration was intended. Always include
the root and verify the final cooked package. Correct a failed group and fully
restart before expecting cleanup authority or new enrollment.

## Cook and validate

1. Use one root per enabled module (`Dye`, `Dye_BronzeTrim`, etc.), with matching
   owner-specific paths. Modules may reference shared Core station assets in
   recipe bodies; **shard references themselves cannot cross owners**.
2. Serialize valid JSON into the string property. JSONC comments are for loose
   authoring files, not the cooked JSON string.
3. Explicitly include the root, every child, its Blueprint class/dependencies
   and all referenced gameplay assets in cooking. Paths inside JSON strings
   are not Unreal hard references and do not automatically force cooking.
4. Check the finished PAK/IoStore asset list, not only the source project.
5. Expect `[PAK-REGISTRY][SHARDS][READY] owner='Dye' documents=... references=...`.
   A healthy group includes the root in `documents`; an index with two pages
   reports three documents. Inspect every subsequent loader/registry rejection.
6. Ship matching packages to participating clients and the server. Never ship
   a whole-project `AssetRegistry.bin` just to make shard discovery work.
7. Test fresh and existing-character discovery, save/reload, multiplayer and
   disabled-content orphan cleanup on disposable copies before release.

The authoring schema is [cooked-registry.schema.json](schemas/cooked-registry.schema.json).
Examples live in `docs/examples/CookedRegistryShards`. Runtime remains
authoritative for owner-dependent paths, case-insensitive duplicates, aggregate
budgets and actual reflected/gameplay type checks.
