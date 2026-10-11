# Declare cooked content for live registries

A PAK mount makes cooked packages available to Unreal. It does **not** by
itself promise that an unreferenced item, recipe, spell, or attack is in the
gameplay list that uses it. RuneSchema supports a mod-owned cooked registry
asset that names specific content to resolve and reconcile with supported live
registries. The game remains authoritative: an already-present matching entry
is accepted rather than duplicated.

Use this declaration for a cooked catalogue or a gameplay asset that has no
other reliable native path into its live registry. A small mod may instead
reference a cooked item through [`assets`](reference/loaders/assets.md) and
connect it with [`recipes`](reference/loaders/recipes.md). Do not create
thousands of empty pointer files for a large PAK.

## Put the declaration in the owning PAK

Create a cooked data asset named `RSREG_*` or `DA_RuneSchemaRegistry*`. It
must contain a `RegistryOwner` string and a `RuneSchemaRegistryJson` string
(or `RegistryJson`). Use a stable owner matching the enabled mod folder for
the conventional path described below. `NativeRegistries` is accepted only
from a cooked registry asset, not loose `registry/*.jsonc` files.

With normal Asset Registry metadata, RuneSchema discovers matching cooked
assets. If an IoStore package has no usable metadata, it checks this one
conventional path for each enabled mod:

```text
/Game/Mods/<ModFolder>/Registry/DA_RuneSchemaRegistry_<ModFolder>.DA_RuneSchemaRegistry_<ModFolder>
```

For the conventional path, `RegistryOwner` must exactly match `<ModFolder>`.
It may point to owned gameplay assets under another permanent content root.
The [shared bridge walkthrough](SHARED-REGISTRY-BRIDGE-PAK.md) explains how to
author the data asset without replacing the shared base.

## Minimal example: a cooked item and recipe

The JSON below is the value of the cooked asset's `RuneSchemaRegistryJson`
property. These are example paths; replace them with exact cooked object paths.

```json
{
  "SchemaVersion": 1,
  "Entries": [],
  "NativeRegistries": {
    "Items": ["/GoldenArsenal/Items/ITEM_GoldenSpear.ITEM_GoldenSpear"],
    "Recipes": ["/GoldenArsenal/Recipes/RECIPE_GoldenSpear.RECIPE_GoldenSpear"]
  }
}
```

The item needs the correct game data class, unique internal name, and stable
persistence ID. The recipe must point to the item and be connected to the
intended station through cooked relationships or the RuneSchema `recipes`
loader. Registry enrollment does not make a recipe appear at a station on its
own.

## Supported declaration fields

| Field | What RuneSchema does | What the mod must still provide |
| --- | --- | --- |
| `Items` | Resolves cooked item data and reconciles the item identity registry. | Complete item data, unique identity, art, and gameplay references. |
| `Recipes` | Resolves cooked recipe data and reconciles the recipe registry. | Ingredient/output references and station or unlock relationship. |
| `Quests` | Resolves cooked quest data and reconciles the quest registry. | Quest objectives, triggers, and valid progress behavior. |
| `CombatSpells` | Reconciles combat-spell data. | Cooked spell data and the weapon or gameplay path that uses it. |
| `UtilitySpells` | Reconciles utility-spell data. | Cooked spell data and its invocation path. |
| `EquipmentEffects` | Reconciles held-equipment effect data. | Valid effect references and owning equipment behavior. |
| `WearableMeshData` | Resolves and class-checks named mesh-data dependencies. | An equipped wearable item that references the correct male/female mesh data, meshes, materials, and hide rules. |
| `MeleeAttackClasses` | Supplies an ordered class plan to the optional melee fallback. | Complete cooked attack classes in intended order; enable the specific fallback setting. |
| `RangedAttackClasses` | Accepted by the schema but **not installed** as a global ranged class list. | Use an item-owned ranged collection instead. |

These are the only `NativeRegistries` fields. Each field accepts at most 4,096
unique paths; a declaration accepts at most 16,384 paths in total. Duplicate,
invalid, unresolved, wrong-class, or conflicting entries are reported rather
than silently converted to a different content type.

## Physical, ranged, and magic attacks

These are different game paths; there is no universal attack registry.

- **Melee:** cook the attack classes and list a complete ordered collection in
  `MeleeAttackClasses`. This lane is an optional, guarded fallback controlled
  by [combat fallback settings](COMBAT-FALLBACK-SETTINGS.md). Ordinary item
  registration is independent of that switch.
- **Ranged:** declare the cooked weapon item in `Items`. Its
  `HeldEquipmentData` must reference a cooked `RangedAttackCollection` with a
  complete `QuickAttackData` and `FullAttackData` pair. RuneSchema can admit
  that pair to the live ranged component when the corresponding fallback is
  enabled. A list of projectile classes in `RangedAttackClasses` does not do
  this.
- **Magic:** declare persistent spell data in `CombatSpells` or
  `UtilitySpells`. Keep the weapon's cooked `MagicAttackCollection` and its
  spell links intact. Do not put spell data in a melee or ranged class field.

For step-by-step weapon authoring, use [weapons and items](WEAPONS-AND-ITEMS.md).
AI attack collection registration is not offered by this declaration.

## Validate the complete path

1. Put the PAK and its cooked registry asset inside an enabled mod folder.
2. Keep the shared Registry Bridge plugin enabled if the asset derives from
   its authoring base.
3. Confirm the log reports the package as mounted and the cooked declaration
   as discovered with the expected owner.
4. Confirm each requested object resolves and the relevant live-registry
   summary reports it as present or added, not rejected.
5. Confirm the item can be obtained, equipped, saved, and used; confirm a
   recipe is connected to its intended station and an attack actually fires.
6. For multiplayer gameplay, ship matching cooked content to the server and
   participating clients. Registry parity diagnostics are not content
   download or replication of missing PAK files.

`RuneSchema/mods/runeschema.txt` controls whether the mod contributes its PAK
and declaration. A line set to `0` contributes neither on the next launch.
Avoid reusing another mod's content path or persistence identity. RuneSchema's
save cleanup checks the applicable settled live registry; the declaration
itself is not a cleanup authority.
