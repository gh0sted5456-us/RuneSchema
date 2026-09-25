# Loader reference

RuneSchema loader definitions are organized under:

```text
RuneSchema/mods/<ModName>/<loader>/
```

Each loader has its own reference page. Use this overview for the shared
authoring model, loader selection, active examples, and cross-loader rules.

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
| [`assets`](loaders/assets.md) | `ArmorCollection`, `Currency`, `MoreHair`, `ResourcesAndLoot` | item clones, cooked items, character-menu DA edits, and native asset edits |
| [`blueprints`](loaders/blueprints.md) | `ArmorCollection`, `ResourcesAndLoot` | loaded Blueprint-default changes |
| [`buildings`](loaders/buildings.md) | `Currency/20-CurrencyProps.jsonc` | registering a cooked BuildingPieceData entry and adding it to a page |
| [`dialogue`](loaders/dialogue.md) | `RuneSchema2VendorTest`, `TravellingMerchants` | vendor, quest, event, and NPC actions |
| [`equipment`](loaders/equipment.md) | `ArmorCollection/98-GhostlyWatcher.json` | Surge evade and Shadowveil preservation on equipped paths |
| [`events`](loaders/events.md) | `RuneSchema2VendorTest`, `TravellingMerchants` | wave encounters, areas, time, and messages |
| [`journal`](loaders/journal.md) | `ArmorCollection`, `Currency`, `RuneSchemaJournalTest` | recipe discovery and authored journal records |
| [`lore`](loaders/lore.md) | `LoreEditTest`, `RuneSchema2VendorTest` | native lore edits and new readable entries |
| [`nameplates`](loaders/nameplates.md) | `PlayerActivityNameplates` | reusable activity badge presentation |
| [`npc`](loaders/npc.md) | `RuneSchema2VendorTest`, `TravellingMerchants` | human merchants, story NPCs, and an interactable prop |
| [`players`](loaders/players.md) | `PlayerProfiles`, `PlayerActivityNameplates`, `RespawnGhostTest` | selectors, attributes, scale, ghost presentation, and badges |
| [`quests`](loaders/quests.md) | `RuneSchema2VendorTest`, `TravellingMerchants` | collect/kill progression, persistence, events, and rewards |
| [`raw`](loaders/raw.md) | `MoreHair`, `ArmorCollection`, `Currency`, `ResourcesAndLoot` | customization, wearable, loot, and station DataTable rows |
| [`recipes`](loaders/recipes.md) | `ArmorCollection`, `BlackG` | crafting/destruction recipes and station placement |
| [`registry`](loaders/registry.md) | `ElementalStaves` | server/client spell presentation declarations |
| [`spawns`](loaders/spawns.md) | `RuneSchema2VendorTest`, `TravellingMerchants`, `RuneSchemaGeneratedSpawnExample` | AI, props, night content, event templates, and generated definitions |
| [`vendors`](loaders/vendors.md) | `RuneSchema2VendorTest`, `TravellingMerchants` | categories, stock, repair, power-level, and time gates |

No active definition was present for `courses`, `effects`, `enums`, `niagara`,
or `strings` during this audit. Their individual loader pages are based on the current runtime schema rather than presented as locally proven mod content.

## Loader reference

| Folder | Main job | Common consumers |
|---|---|---|
| [`assets`](loaders/assets.md) | Items, icons, stats, unlock links | recipes, equipment, journal, vendors |
| [`blueprints`](loaders/blueprints.md) | Supported reflected class defaults | cooked gameplay classes |
| [`buildings`](loaders/buildings.md) | BuildingPieceData registration and cloning | build menu, spawns |
| [`courses`](loaders/courses.md) | Course definitions and patches | course runtime |
| [`dialogue`](loaders/dialogue.md) | Conversations and actions | npc, vendors, quests, events, lore |
| [`effects`](loaders/effects.md) | GameplayEffect class aliases | equipment, players, spawns |
| [`enums`](loaders/enums.md) | Loaded enum extensions | reflected fields |
| [`equipment`](loaders/equipment.md) | Wear-triggered effects and utility behavior | assets |
| [`events`](loaders/events.md) | Timed waves driven by dialogue | spawns, quests |
| [`journal`](loaders/journal.md) | Journal and recipe entries | recipes, assets |
| [`lore`](loaders/lore.md) | Lore entries and pages | dialogue, npc |
| [`nameplates`](loaders/nameplates.md) | Reusable nameplate definitions | players |
| [`niagara`](loaders/niagara.md) | Niagara attachment definitions | equipment, players, dialogue, spawns |
| [`npc`](loaders/npc.md) | Persistent interactable actors | dialogue, vendors, quests |
| [`players`](loaders/players.md) | Player rules and presentation | nameplates, effects, niagara |
| [`quests`](loaders/quests.md) | Per-character quest definitions | dialogue, npc, events |
| [`raw`](loaders/raw.md) | DataTable rows and patches | assets, recipes, loot |
| [`recipes`](loaders/recipes.md) | Crafting, processing, and merchant offers | stations, vendors |
| [`registry`](loaders/registry.md) | Multiplayer action/presentation manifest | server and clients |
| [`spawns`](loaders/spawns.md) | AI, actors, resources, building props | events, quests, Helpy |
| [`strings`](loaders/strings.md) | Source-text replacement | UI text |
| [`vendors`](loaders/vendors.md) | Reusable RuneSchema stores | npc, dialogue, recipes |

## Related references

- [Authoring Guide](AUTHORING-GUIDE.md) — installation, ordering, plugins,
  mappings, multiplayer, and testing.
- [Registry Patching](REGISTRY-PATCHING.md) — transactional DataTable patches.
- [Compatibility Backbone](COMPATIBILITY-BACKBONE.md) — storefront and plugin
  compatibility flow.
- [Example Library](EXAMPLES.md) — focused authoring and integration examples.
