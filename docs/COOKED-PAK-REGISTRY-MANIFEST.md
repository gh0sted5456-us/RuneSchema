# Declare cooked content for live registries

## Step-by-step workflow

1. Identify cooked content that lacks another reliable enrollment route; avoid duplicating ordinary loader registration.
2. Choose a loose `registry/*.jsonc` declaration or a mod-owned cooked registry asset containing exact object paths and supported lanes.
3. Keep identity/construction in the asset or owning loader; declarations do not create missing classes or permission.
4. Mount the content provider and restart; verify every required lane and its gameplay consumer before relying on cleanup certification. Cook a declaration asset only when choosing the cooked route.

JSONC snippets are authoring mock-ups. Replace example paths, IDs and values with
inspected content; a valid snippet cannot supply missing cooked dependencies.

**Tie-ins:** [Registry loader](reference/loaders/registry.md), [PAK content](PAK-CONTENT-GUIDE.md), [recipes](reference/loaders/recipes.md) and [journal](reference/loaders/journal.md).


A PAK mount makes cooked packages available to Unreal. It does **not** by
itself promise that an unreferenced item, recipe, spell, or attack is in the
gameplay list that uses it. RuneSchema supports a mod-owned cooked registry
asset that names specific content to resolve and reconcile with supported live
registries. The game remains authoritative: an already-present matching entry
is accepted rather than duplicated.

Use this declaration for a cooked catalogue or a gameplay asset that has no
other reliable native path into its live registry. A small mod may instead
reference a cooked item through [`assets`](reference/loaders/assets.md) and
connect it with [`recipes`](reference/loaders/recipes.md). RuneSchema 0.8.0.0 also
accepts recipe, journal and lore loader metadata inside the cooked declaration,
so a PAK need not duplicate those pointers in loose files. Do not create
thousands of empty pointer files for a large PAK.

## Choose loose JSONC or a cooked declaration

If the item assets are already cooked but their PAK has no registry declaration,
you do not need to recook that PAK just to announce them. Ship a loose
`RuneSchema/mods/<ModName>/registry/10-native-items.jsonc` document instead.
The [loose item-registration walkthrough](reference/loaders/registry.md#register-a-cooked-item-without-cooking-a-registry-declaration)
includes the folder layout, enable line, JSONC example, downloadable starter,
and verification steps. `NativeRegistries` and `LoaderDefinitions` are accepted
from enabled loose registry documents as well as cooked declaration assets.

Use one declaration route per reference. Neither route creates missing cooked
content or substitutes for its stable native identity. Ordinary item clones
created by `/assets` already use their owning loader's registration route.

## Alternatively, put the declaration in the owning PAK

Create a cooked data asset named `RSREG_*` or `DA_RuneSchemaRegistry*`. It
must contain a `RegistryOwner` string and a `RuneSchemaRegistryJson` string
(or `RegistryJson`). Use a stable owner matching the enabled mod folder for
the conventional path described below. This cooked route is optional when
using a loose registry document. Keep `SchemaVersion: 1` and `Entries: []` even when
the asset declares only native content.

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

```jsonc
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
| `Journals` | Uses the journal loader to register an existing cooked journal entry of any supported native journal type. | Cooked pages, stable identity, section placement and an unlock trigger. |
| `Lore` | Uses the lore loader to register existing cooked Lore entries. | Cooked Lore data, stable identity, placement and an unlock trigger. |
| `CombatSpells` | Reconciles combat-spell data. | Cooked spell data and the weapon or gameplay path that uses it. |
| `UtilitySpells` | Reconciles utility-spell data. | Cooked spell data and its invocation path. |
| `EquipmentEffects` | Reconciles held-equipment effect data. | Valid effect references and owning equipment behavior. |
| `WearableMeshData` | Resolves and class-checks named mesh-data dependencies. | An equipped wearable item that references the correct male/female mesh data, meshes, materials, and hide rules. |
| `MeleeAttackClasses` | Supplies an ordered class plan to the optional melee fallback. | Complete cooked attack classes in intended order; enable the specific fallback setting. |
| `RangedAttackClasses` | Accepted by the schema but **not installed** as a global ranged class list. | Use an item-owned ranged collection instead. |

These are the only `NativeRegistries` fields. Each field accepts at most 4,096
unique paths; a declaration accepts at most 16,384 references in total,
including `LoaderDefinitions` entries. The cooked JSON string is bounded to
512 KiB in 0.8.0.0 (older builds: 256 KiB). For larger catalogues, use a conventional root with explicit
`RegistryShards` and marked child assets as described in
[bounded cooked registry shards](COOKED-REGISTRY-SHARDS.md). Those pages load
directly, without Asset Registry enumeration or a whole-project registry file.
Independent metadata-discoverable declarations remain supported. Duplicate,
invalid, unresolved, wrong-class, or conflicting entries are reported rather
than silently converted to a different content type.

## Keep recipe, journal and lore metadata in the PAK

`LoaderDefinitions` accepts `Recipes`, `Journals` and `Lore` maps. Each key is
the **full cooked object path**; its value is the ordinary loader definition
body. It references an existing cooked asset, not a new runtime clone. A path
in `LoaderDefinitions` does not also need a `NativeRegistries` pointer.

```jsonc
{
  "SchemaVersion": 1,
  "Entries": [],
  "NativeRegistries": {
    "Items": ["/Example/Items/ITEM_Dye.ITEM_Dye"]
  },
  "LoaderDefinitions": {
    "Recipes": {
      "/Example/Recipes/RECIPE_Dye.RECIPE_Dye": {
        "Unlock": false,
        "AddTo": [{
          "DataTable": "/Example/Stations/DT_Stations.DT_Stations",
          "Row": "DyeStation",
          "Category": "Dyes"
        }]
      }
    },
    "Journals": {
      "/Example/Journal/JOURNAL_Trader.JOURNAL_Trader": {
        "Unlock": false,
        "AddTo": {"Section": "Knowledge", "Category": "People", "Group": "Craftsmen"}
      }
    },
    "Lore": {
      "/Example/Journal/JOURNAL_Note.JOURNAL_Note": {
        "Unlock": false,
        "UnlockOnAcquire": true,
        "ItemData": "/Example/Items/ITEM_Note.ITEM_Note",
        "AddTo": {"Section": "Knowledge", "Category": "Lore Scraps"}
      }
    }
  }
}
```

All paths, station row/category and group above are examples. The station must
already have the matching native row/array contract. Cooked ingredient/output
references remain part of the recipe asset. See the [recipe loader](reference/loaders/recipes.md)
and [journal loader](reference/loaders/journal.md) for the supported body fields,
native discovery conditions and journal routes. `/lore` remains Lore-only;
`Journals` can use People, Place, World, Recipe, Treasure or Lore entries.

Rules:

- Registration alone **does not grant player progress**. Cooked declarations
  default to `Unlock: false`, including bare `NativeRegistries.Recipes`,
  `Journals` and `Lore` paths. Explicit `Unlock: true` opts into the existing
  loader's automatic delivery; it does not bypass native unlock confirmation.
- Recipe bodies may carry `Discovery` for native pickup, interaction and
  skill-level rules. This is the same [recipe discovery contract](RECIPE-DISCOVERY.md)
  as a loose `/recipes` override, not the journal `UnlockCondition` struct.
- An omitted journal `UnlockCondition` leaves the asset's native condition
  intact. Supply a supported condition in its loader body to override it, or
  unlock via an NPC's `JournalEntries` full cooked path or a cooked gameplay
  event. Merely placing a page does not prove discovery.
- Existing loose `/recipes`, `/journal` and `/lore` definitions for the same
  cooked path take precedence. Identical repeated declarations are idempotent;
  conflicting cooked bodies reject that lane rather than choosing a winner.
  Declare each journal asset in either `Journals` or `Lore`, not both.
- These declarations cannot contain `$Clone`, `$Patch`, `$Target` or vendor-offer
  directives. Keep those in their ordinary loose authoring loaders. Cooked
  journal identity assertions must match the shipped asset, not replace it.
- This is an explicit announcement contract, not a wildcard scan of every
  asset in a PAK. Existing PAKs can be accompanied by a loose declaration;
  recooking is necessary only to embed the declaration inside the PAK.
  No game saves or mod lists are rewritten to adopt the contract.

The authoring shape is available in
[`cooked-registry.schema.json`](schemas/cooked-registry.schema.json). Runtime
validation additionally checks class compatibility, identity conflicts,
cross-document conflicts and the combined document limit.

## Orphan-only automatic cleanup

Automatic character cleanup consults the settled live identity registries,
not whether a loose pointer file exists. A registered cooked recipe or
journal/lore ID remains valid when its duplicate loose declaration is removed.
Already learned progress is not revoked merely because `Unlock` becomes false.
Unresolved IDs can be pruned after their content is disabled or removed and
the applicable registry has been verified. No orphaned IDs means no cleanup
write.

A rejected enabled cooked declaration, missing persistent registry binding,
failed recipe/journal enrollment, or disabled required loader vetoes automatic
character cleanup for that session. A failed load is **not** evidence of an
orphan. Restart after correcting the declaration to rebuild its authority.
World-building cleanup retains its separate live-building-registry and
enabled-definition safeguards. Explicit manual owner-removal is a separate
opt-in operation, not the automatic orphan-only policy.

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
