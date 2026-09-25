# Loader walkthroughs

Every loader accepts supported files throughout its complete nested subtree.
Subfolders are organizational only; processing order is the normalized path
relative to the loader root.

The `assets` loader accepts direct `DA_` field patches. A full target ending in
`_C` identifies its Blueprint class default object; an ordinary `DA_` path
identifies the DataAsset. No schema URL or repeated mod ID is needed. For hair,
keep the DataTable rows in `raw` and append the unique menu option to
`DA_CharacterOptionData_C` from `assets`; see `examples/CharacterCustomization`.

Each loader reads files from `RuneSchema/mods/<ModName>/<loader>/`. Examples in
this repository are authoring references; release ZIPs do not install them.

## The common authoring model

RuneSchema derives ownership from `<ModName>`. Authors do not repeat a mod ID in
each loader file. Inside that boundary, identity comes from the native object
path, DataTable row name, loader `Id`, or `PersistenceID` appropriate to the
record. References to another RuneSchema record use `ModName:Id`; an unqualified
`Id` means the current mod.

Every loader recursively discovers `.json` and `.jsonc`. It does not follow
directory links. Files are sorted by normalized path relative to the loader
root, so numbered names such as `00-base.jsonc` and `20-overrides.jsonc` make
intent visible. Nested folders are organizational and never become identity.
Later compatible definitions may extend or replace earlier values according to
the loader contract; duplicate persistent identities are rejected rather than
silently reassigned.

There are four common shapes:

- A map keyed by cooked object path or record key: `/assets`, `/raw`,
  `/recipes`, `/buildings`, `/journal`, `/lore`, and `/strings`.
- An array of independently validated records: `/spawns`, `/players`,
  `/quests`, `/events`, and most `/npc` definitions.
- A reusable catalogue keyed by an `Id`: `/vendors`, `/effects`, `/niagara`,
  and `/nameplates`.
- A direct reflected target followed by native field paths: advanced `/assets`
  `DA_` edits and compatible `/raw` transactions.

Normal successful section writes use `[LOADER:<name>][OK]` and are shown only
when advanced logging is enabled. A bad file, record, field, or optional native
capability uses `[PARTIAL]` or `[DISABLED]`; RuneSchema continues with unrelated
files, sections, mods, and loaders. Errors are reserved for core invariants and
save-integrity boundaries where continuing could persist an unsafe identity.
Advanced logging keeps a small sample of successful recipe, asset-clone, and
Blueprint operations, then reports how many additional detail lines were
omitted. Loader totals, warnings, partial failures, and errors are never hidden.
This keeps the front-end log readable without removing the evidence needed to
identify which kind of operation ran.

The server owns gameplay mutations: inventory grants, purchases, quest state,
spawning, building placement, AI, drops, and event progression. Clients present
replicated actors and the visual data they have installed. A cooked asset path
must therefore exist wherever it is rendered. Identical JSON on server and
clients does not turn a presentation declaration into server authority; use
`/registry` for the explicit server/client action bridge.

SafeSave records only RuneSchema-owned persistent identities. At the next load,
the current definitions are compared with that compact snapshot. Missing owned
items, recipes, journal/lore entries, quests, buildings, and declarations are
eligible for cleanup. Vanilla content and arbitrary unresolved game content are
outside that boundary. Reinstalling a removed mod is a fresh install; RuneSchema
does not restore removed state from a historical ledger.

## Active-mod example index

These examples are based on the active local mod set inspected for this guide.
They are references to patterns, not files bundled into a public runtime.

| Loader | Active reference | What it demonstrates |
|---|---|---|
| `assets` | `ArmorCollection`, `Currency`, `MoreHair`, `ResourcesAndLoot` | item clones, cooked items, character-menu DA edits, and native asset edits |
| `blueprints` | `ArmorCollection`, `ResourcesAndLoot` | loaded Blueprint-default changes |
| `buildings` | `Currency/20-CurrencyProps.jsonc` | registering a cooked BuildingPieceData entry and adding it to a page |
| `dialogue` | `RuneSchema2VendorTest`, `TravellingMerchants` | vendor, quest, event, and NPC actions |
| `equipment` | `ArmorCollection/98-GhostlyWatcher.json` | Surge evade and Shadowveil preservation on equipped paths |
| `events` | `RuneSchema2VendorTest`, `TravellingMerchants` | wave encounters, areas, time, and messages |
| `journal` | `ArmorCollection`, `Currency`, `RuneSchemaJournalTest` | recipe discovery and authored journal records |
| `lore` | `LoreEditTest`, `RuneSchema2VendorTest` | native lore edits and new readable entries |
| `nameplates` | `PlayerActivityNameplates` | reusable activity badge presentation |
| `npc` | `RuneSchema2VendorTest`, `TravellingMerchants` | human merchants, story NPCs, and an interactable prop |
| `players` | `PlayerProfiles`, `PlayerActivityNameplates`, `RespawnGhostTest` | selectors, attributes, scale, ghost presentation, and badges |
| `quests` | `RuneSchema2VendorTest`, `TravellingMerchants` | collect/kill progression, persistence, events, and rewards |
| `raw` | `MoreHair`, `ArmorCollection`, `Currency`, `ResourcesAndLoot` | customization, wearable, loot, and station DataTable rows |
| `recipes` | `ArmorCollection`, `BlackG` | crafting/destruction recipes and station placement |
| `registry` | `ElementalStaves` | server/client spell presentation declarations |
| `spawns` | `RuneSchema2VendorTest`, `TravellingMerchants`, `RuneSchemaGeneratedSpawnExample` | AI, props, night content, event templates, and generated definitions |
| `vendors` | `RuneSchema2VendorTest`, `TravellingMerchants` | categories, stock, repair, power-level, and time gates |

No active definition was present for `courses`, `effects`, `enums`, `niagara`,
or `strings` during this audit. Their examples below are built from the loader's
current runtime schema rather than presented as locally proven mod content.

## Loader map

| Folder | Main job | Common consumers |
|---|---|---|
| `assets` | Items, icons, stats, unlock links | recipes, equipment, journal, vendors |
| `blueprints` | Supported reflected class defaults | cooked gameplay classes |
| `buildings` | BuildingPieceData registration and cloning | build menu, spawns |
| `courses` | Course definitions and patches | course runtime |
| `dialogue` | Conversations and actions | npc, vendors, quests, events, lore |
| `effects` | GameplayEffect class aliases | equipment, players, spawns |
| `enums` | Loaded enum extensions | reflected fields |
| `equipment` | Wear-triggered effects and utility behavior | assets |
| `events` | Timed waves driven by dialogue | spawns, quests |
| `journal` | Journal and recipe entries | recipes, assets |
| `lore` | Lore entries and pages | dialogue, npc |
| `nameplates` | Reusable nameplate definitions | players |
| `niagara` | Niagara attachment definitions | equipment, players, dialogue, spawns |
| `npc` | Persistent interactable actors | dialogue, vendors, quests |
| `players` | Player rules and presentation | nameplates, effects, niagara |
| `quests` | Per-character quest definitions | dialogue, npc, events |
| `raw` | DataTable rows and patches | assets, recipes, loot |
| `recipes` | Crafting, processing, and merchant offers | stations, vendors |
| `registry` | Multiplayer action/presentation manifest | server and clients |
| `spawns` | AI, actors, resources, building props | events, quests, Helpy |
| `strings` | Source-text replacement | UI text |
| `vendors` | Reusable RuneSchema stores | npc, dialogue, recipes |

## `assets`

Use `/assets` to patch a loaded item or clone a compatible item into a new
cooked path.

```json
{
  "/Game/MyMod/Items/ITEM_Test.ITEM_Test": {
    "$Clone": "/Game/Gameplay/Items/ITEM_Source.ITEM_Source",
    "PersistenceID": "stable-unique-id",
    "InternalName": "mymod_test",
    "Name": "Test Item",
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

Reference: `examples/RSv16/ExampleMods/RuneSchema2VendorTest/assets`.

### Direct `DA_` fields

For an existing DataAsset or generated-class default, put the complete `DA_`
path first and native field paths below it. RuneSchema infers a class default
object from `_C`; the mod directory supplies ownership.

```json
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

## `blueprints`

Use `/blueprints` for supported reflected defaults on an existing loaded
class or component. It does not create Blueprint classes.

```json
[
  {
    "/Game/Path/BP_Target": {
      "Data": { "Duration": 10.0 }
    }
  }
]
```

Confirm every field against live reflection. Restart after changing a class
default. Use a cooked Blueprint in a PAK when a new class is required.

## `buildings`

Use `/buildings` to register an existing `BuildingPieceData` asset or clone one
as a separate build-menu entry.

```jsonc
{
  "MyWall": {
    "$Clone": "/Game/Gameplay/Building/Data/BUILDING_Source.BUILDING_Source",
    "Properties": {
      "BuildableActor": "/Game/MyMod/Buildings/BP_MyWall.BP_MyWall_C"
    },
    "Requirements": [
      {"ItemData":"/Game/Gameplay/Items/ITEM_Log.ITEM_Log","Amount":4}
    ],
    "Unlock": true,
    "AddTo": {"Collection":"Modded Buildings","PageIndex":0}
  }
}
```

Walkthrough:

1. Select a compatible `BuildingPieceData` source.
2. Use `$Clone` for a new identity or `Asset` to register an existing record.
3. Point `BuildableActor` at a cooked child of the game's base building actor.
4. Replace the full requirements list when changing cost.
5. Set `AddTo`, or omit it to inherit pages containing the clone source.
6. Test placement, collision, navigation, save/reload, and deconstruction on
   both host and client.

`PersistenceID`, `InternalName`, piece index, and requirements cannot be hidden
inside `Properties`; RuneSchema owns those fields. For imported assemblies and
static parent objects, see [raw/BASE-BUILDER-IMPORT.md](raw/BASE-BUILDER-IMPORT.md)
and [BUILDING-CLONING-FMODEL-AUDIT.md](BUILDING-CLONING-FMODEL-AUDIT.md).

## `courses`

Use `/courses` to add course records or patch a course by ID. A course can
define a starter, finish, orb sets, stamina orbs, props, and a zone.

```json
[
  {
    "Id": "mymod_course",
    "StarterLocation": [1000, 2000, 300],
    "FinishLocation": [2000, 2000, 300],
    "Orbs": []
  }
]
```

Use `$Patch` with `$Target` for an existing authored course. Keep points in
Unreal centimeters and verify the zone encloses the playable route.

## `dialogue`

Dialogue files define one conversation with an entry node and up to four
choices per node.

```json
{
  "Id": "hello",
  "Entry": "start",
  "Nodes": {
    "start": {
      "Text": "Welcome.",
      "Choices": [
        {"Id":"trade","Text":"Show me your wares.","VendorID":"shop"},
        {"Id":"leave","Text":"Goodbye.","End":true}
      ]
    }
  }
}
```

Walkthrough:

1. Create the dialogue ID.
2. Bind it from an NPC's `DialogueID`.
3. Link choices with `Next`, or end them with `End:true`.
4. Add requirements before actions that must be gated.
5. Use supported actions for vendors, quests, events, lore, animation, and
   Niagara.
6. Test the initial prompt, every branch, and reconnect behavior.

Example: `examples/RSv16/dialogue/60-GoblinPatrol.json`.

## `effects`

`/effects` assigns virtual IDs to cooked GameplayEffect classes.

```json
{
  "Movement/Dash": {
    "Class": "/Game/MyMod/Effects/GE_Dash.GE_Dash_C"
  }
}
```

Reference the definition as `MyMod:Effects/Movement/Dash` or use the full
class path where a consumer permits it. This loader does not clone or patch
effect class defaults.

## `enums`

Use `/enums` to add names to a supported loaded enum.

```json
{
  "EMyLoadedEnum": ["NewValue", "AnotherValue"]
}
```

Write values without the `EnumName::` prefix. This changes the loaded enum
name set; it does not add native code or Blueprint logic for a new value.

## `equipment`

Use `/equipment` to bind supported behavior to worn item paths.

```json
{
  "SurgeEvadeLegs": {
    "/Game/MyMod/Items/ITEM_DashLegs.ITEM_DashLegs": true
  },
  "GrantedEffects": {
    "/Game/MyMod/Items/ITEM_DashLegs.ITEM_DashLegs": {
      "Mode": "Append",
      "Effects": ["MyMod:Effects/Movement/Dash"]
    }
  }
}
```

`GrantedEffects` supports `Replace`, `Append`, and `Clear`. Use item data paths,
not executable addresses. Validate equip, unequip, death, respawn, reconnect,
and client presentation.

## `events`

Events run server-owned waves from `/spawns` entries marked `EventOnly:true`.

```json
[
  {
    "Id": "night_attack",
    "TimeOfDay": "Night",
    "TimeoutSeconds": 900,
    "Waves": [[
      {"SpawnID":"night_raider","Location":[1000,2000,"$+10"]}
    ]]
  }
]
```

Create the spawn template first, then start or cancel the event from dialogue.
`$`, `$+offset`, and `$-offset` resolve Z against blocking ground. Event IDs can
scope quest kill credit.

## `journal`

Use `/journal` for recipe and discovery entries.

```json
{
  "RS_MyRecipe": {
    "Type": "Recipe",
    "DisplayName": "My Recipe",
    "RecipeData": "RECIPE_MY_ITEM",
    "ItemData": "/Game/MyMod/Items/ITEM_MyItem.ITEM_MyItem",
    "PageDescriptions": [{"Description":"First page."}],
    "Unlock": true,
    "AddTo": {
      "SubCategory": "/Game/UI/JournalData/JOURNAL_SC_Recipe.JOURNAL_SC_Recipe",
      "Key": "RS_MyRecipe"
    }
  }
}
```

`AddTo` can target a full subcategory path or an unambiguous loaded category.
Groups can be created where the native category supports them. Verify the
entry, pages, icon, unlock, save, and reload.

## `lore`

`/lore` uses the journal registry but fixes the entry type to lore.

```json
{
  "RS_MyBook": {
    "Type": "Lore",
    "DisplayName": "My Book",
    "PageDescriptions": [
      {"Description":"Page one."},
      {"Description":"Page two."}
    ],
    "Unlock": false
  }
}
```

Open the entry from a supported dialogue lore action or unlock it normally.
Cooked entries may use `$declaration` for ownership tracking.

## `nameplates`

Nameplates are reusable definitions consumed by `/players`.

```json
[
  {
    "Id": "ActivityBadge",
    "Nameplate": {
      "Mode": "Icon",
      "Icon": "/Game/MyMod/UI/T_Badge.T_Badge",
      "Distance": 2500,
      "Client": "Yes",
      "Server": "Yes"
    }
  }
]
```

States and observed function events can change or pulse a badge. Use exact
function paths and narrow parameter conditions. Test self, host, remote client,
distance, inactivity timeout, death, and respawn.

## `niagara`

Niagara definitions wrap a cooked system and attachment settings.

```json
{
  "NightAura": {
    "System": "/Game/MyMod/VFX/NS_NightAura.NS_NightAura",
    "Socket": "root",
    "AutoActivate": true,
    "LocationOffset": {"X":0,"Y":0,"Z":0},
    "Parameters": {"User.Intensity":1.0}
  }
}
```

Only `User.*` parameters are accepted. Supported values are Boolean, number,
vector, and color. Reference the definition from a consumer's visual-effect
block and verify it on every client.

## `npc`

`/npc` creates persistent AI, human, or resource-style actors.

```jsonc
{
  "Id": "merchant",
  "Type": "AI",
  "Multiplayer": true,
  "DisplayName": "Merchant",
  "VendorID": "shop",
  "DialogueID": "hello",
  "VisualSource": "/Game/Gameplay/NPCs/BP_BaseInteractableNPC.BP_BaseInteractableNPC_C",
  "Location": [1000, 2000, "$+10"],
  "PowerLevel": 20
}
```

Choose one actor role, provide a compatible visual source, and bind dialogue or
vendor IDs. `HideWeapon` can suppress inherited weapon presentation. Prefer
`/buildings` or `/spawns` for static architecture rather than using an NPC as a
prop.

## `players`

Player rules select by name, GUID, or wildcard and apply supported attributes,
appearance, effects, archetypes, map icons, and nameplates.

```json
[
  {
    "Id": "all-players",
    "PlayerName": "*",
    "Scale": 1.0,
    "Nameplate": {"Definition":"ActivityBadge","ShowSelf":true}
  }
]
```

Use GUID selectors when names are not unique. Keep appearance rows and cooked
assets installed on every client. Test respawn because player pawns and
components can be recreated.

## `quests`

Quests are per-character definitions with stages, objectives, rewards, and a
stable persistence ID.

```json
{
  "Id": "gather_logs",
  "PersistenceID": "stable-quest-id",
  "Title": "Gather Logs",
  "Description": "Bring back five logs.",
  "Completion": "ReturnToNPC",
  "Stages": [{
    "Id": "gather",
    "Objectives": [{
      "Id": "logs",
      "Type": "Fetch",
      "Item": "/Game/Gameplay/Items/ITEM_Log.ITEM_Log",
      "Count": 5
    }]
  }],
  "Reward": {"Item":"/Game/Gameplay/Items/ITEM_Reward.ITEM_Reward","Count":1}
}
```

Walkthrough:

1. Assign stable quest and persistence IDs.
2. Define stages and unique objective IDs.
3. Bind accept and turn-in actions from dialogue.
4. Add event IDs or search areas to kill objectives when required.
5. Test the acceptance toast, progress, reconnect, reload, turn-in, repeat, and
   removal behavior.

## `raw`

`/raw` creates or patches supported DataTable rows.

```json
{
  "DT_WearableEquipment": {
    "mymod_legs": {
      "Defense": 20.0,
      "MagicResistance": 5.0
    }
  }
}
```

Use an exact DataTable object path when a short table name is ambiguous. New
registry-patch documents can declare the target path, row struct, ownership,
preconditions, and operation. See
[`schemas/registry-patch-v1.schema.json`](schemas/registry-patch-v1.schema.json)
and `examples/RegistryPatch`.

Do not guess nested layouts from a USMAP alone. RuneSchema compares the target
against live reflection before committing a row.

## `recipes`

Recipes can feed vanilla crafting or processing rows, vanilla merchants, and
RuneSchema vendors.

```jsonc
{
  "RECIPE_MY_ITEM": {
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
- Use `Array` for station/processor recipe arrays.
- Use `Category` for merchant labeled-recipe categories.
- Use `RuneSchemaVendors` for one or more RuneSchema store IDs.
- Use `VanillaVendors` for native merchant table/row targets.
- `Unlock:true` grants the recipe; placement alone does not.
- `Order` sorts within a category. Lower values appear first.

## `registry`

Registry files merge client presentation and server authority declarations
under the owning mod namespace.

```json
{
  "SchemaVersion": 1,
  "Entries": [{
    "Id": "spell_presentation",
    "Kind": "SpellPresentation",
    "Spell": "/Game/MyMod/Spells/DA_MySpell.DA_MySpell",
    "Presentation": [{
      "Phase": "SpawnVFX",
      "Class": "/Game/MyMod/VFX/BP_MyImpact.BP_MyImpact_C",
      "Classification": "PureVFX"
    }]
  }]
}
```

Duplicate `ModName:Id` keys are rejected and reported. Presentation assets must
exist on clients. Authority actions are validated on the server; the JSON does
not grant permission by itself.

## `spawns`

Spawns place AI, actors, resources, and supported building props.

```json
[
  {
    "Id": "night_guard",
    "Type": "AISpawnPoint",
    "AIClass": "/Game/Gameplay/AI/BP_Guard.BP_Guard_C",
    "Location": [1000, 2000, "$+10"],
    "PowerLevel": 30,
    "TimeOfDay": "Night",
    "SpawnRadiusMeters": 100
  }
]
```

Use `GroundOffset:10` or `$+10` for ten centimeters above traced ground, not
both. `AdditionalDrops` supports an item, minimum, maximum, and chance. A
quest-completed condition can latch with `PersistAfterCondition`. `EventOnly`
templates are definitions for `/events` and do not place permanent actors.

## `strings`

Strings replace matching source text globally or within a named table.

```json
{
  "DT_SomeTextTable": {
    "Old text": "New text"
  }
}
```

A replacement may also be a list where the loader supports multiple results.
Exact source text matters. Later loaded replacements win within the same scope.
This loader does not patch string-table keys.

## `vendors`

Vendors define reusable stores bound from NPCs or dialogue.

```json
{
  "Id": "shop",
  "MerchantName": "Supply Shop",
  "Repairable": false,
  "Items": [{
    "Category": "Supplies",
    "Item": "/Game/Gameplay/Items/ITEM_Log.ITEM_Log",
    "Currency": "/Game/Gameplay/Items/ITEM_Coin.ITEM_Coin",
    "Price": 2,
    "Count": 10,
    "Order": 10
  }]
}
```

Bind the store with an NPC `VendorID`, a dialogue vendor action, or a recipe
contribution. Ordering is within each category. Confirm purchase authority,
stock refresh, reconnect, and save/reload.

## Related files

- [AUTHORING-GUIDE.md](AUTHORING-GUIDE.md): installation, ordering, plugins,
  mappings, multiplayer, and testing.
- [COMPATIBILITY-BACKBONE.md](COMPATIBILITY-BACKBONE.md): storefront and plugin
  compatibility flow.
- [REGISTRY-PATCHING.md](REGISTRY-PATCHING.md): transactional DataTable patches.
- [HELpy-REDESIGN-AUDIT.md](HELpy-REDESIGN-AUDIT.md): Helpy runtime design.
- `raw/include/Generator/LoaderSchemas.h`: runtime-generated schema source.
