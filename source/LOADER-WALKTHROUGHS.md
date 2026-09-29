# Loader reference

Each loader reads `.json` and `.jsonc` files recursively from:

```text
RuneSchema/mods/<ModName>/<loader>/
```

The mod folder is the owner namespace. Nested folders are for organization only.
Use `OtherMod:Id` for an explicit cross-mod reference.

## Loader map

| Folder | Use it for |
|---|---|
| `assets` | Item/DataAsset edits and compatible clones |
| `blueprints` | Supported reflected Blueprint defaults and runtime widget rules |
| `buildings` | BuildingPieceData registration, cloning, costs, placement |
| `courses` | Course definitions |
| `dialogue` | Conversations and actions |
| `effects` | Gameplay-effect aliases |
| `enums` | Supported loaded enum extensions |
| `equipment` | Equipped effects and utility behaviour |
| `events` | Timed encounters and event state |
| `journal` | Journal and recipe-discovery entries |
| `lore` | Lore entries and pages |
| `nameplates` | Reusable player nameplate definitions |
| `niagara` | Niagara attachment definitions |
| `npc` | Persistent interactable NPCs and merchants |
| `players` | Player selectors, attributes, appearance, nameplates |
| `quests` | Quest definitions and rewards |
| `raw` | DataTable rows and row patches |
| `recipes` | Crafting, processing, station and vendor recipes |
| `registry` | Server-action/client-presentation bridge |
| `spawns` | AI, actors, resources and world placements |
| `strings` | Text replacement |
| `vendors` | Reusable RuneSchema stores |

Open the matching loader page for its schema, examples, and limits.

## Common rules

- Use full cooked object paths when short names can be ambiguous.
- Keep stable IDs and PersistenceIDs once a mod is released.
- `$Clone` creates a new supported definition from a compatible source.
- `$Append` adds entries; it does not deduplicate them.
- `$AppendUnique` requires a stable identity for each entry.
- `$Patch` changes an existing supported definition.
- Unknown fields are rejected on closed authoring surfaces.
- Live reflection remains authoritative. A schema-valid field can still fail if
  the running game no longer exposes the expected object or type.

## Items and recipes

An `/assets` clone is usable only after RuneSchema registers its identity with
the live ItemSubsystem. Recipes should reference that registered object path.

Recipe placement and recipe discovery are separate:

- `AddTo` places a recipe at a station or vendor.
- `Unlock: true` grants the recipe to the player.
- valid RuneSchema recipe unlocks may persist;
- generated vendor recipes remain transient.

## Multiplayer

RuneSchema content does not have a single-player/multiplayer authoring switch.
The same definition is used in every network mode. The runtime decides whether
the current process owns gameplay authority or client presentation.

Install matching definitions and cooked assets anywhere they are required.
Use `/registry` only when a feature needs an explicit server action paired with
client presentation.

## Failures

Common log scopes are:

```text
[DISABLED][LOADER:name]       loader initialization failed
[DEGRADED][LOADER:name]       part of the loader failed
[DEGRADED][LOADER:name][MOD]  one mod section failed
[PERF][LOADER:name]           a step exceeded the performance threshold
```

Fix the first error for the named loader and mod. Restart unless that loader
explicitly supports hot reload.

## Test order

1. Test one definition.
2. Confirm the startup storefront and loader lines.
3. Confirm cooked paths resolve.
4. Test save/reload.
5. Test removal and Safe Clean.
6. Test host/client or dedicated-server use when the mod has multiplayer-facing content.
