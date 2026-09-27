# Loader consistency audit — 0.7.6

This audit compares every public loader folder with its compiled structural
schema, lifecycle ordering, ownership boundary and failure-isolation policy.
All loader folders are case-insensitive and recursively accept `.json` and
`.jsonc`; nested folders affect deterministic file order only.

| Loader | Durable identity or target | Main safety boundary |
|---|---|---|
| `assets` | item `PersistenceID` and object path | clones register before recipe resolution; identity fields cannot be patched casually |
| `blueprints` | exact loaded class/default path | reflected fields only; no invented class generation |
| `buildings` | building `PersistenceID` and BuildingPieceData path | actor type, requirements, menu placement and station row are preflighted |
| `courses` | course key | record-isolated validation |
| `dialogue` | `Mod:Id` | server owns actions; bad nodes do not disable other conversations |
| `effects` | `Mod:Effects/Id` | alias resolves to a cooked GameplayEffect class before use |
| `enums` | loaded enum path and value name | loaded enum only; no native behavior fabrication |
| `equipment` | cooked ItemData object path | all requested fields validate before an item is changed |
| `events` | `Mod:Id` | server-owned waves and bounded time/quest conditions |
| `journal` / `lore` | exact `PersistenceID` | registered entries are ledgered and exact-ID cleanup verifies saved ownership |
| `nameplates` | `Mod:Id` | presentation-only state; server data remains authoritative |
| `niagara` | `Mod:Id` and cooked system path | attachment and parameter validation; clients require the asset |
| `npc` | definition ID / persistent actor identity | section isolation, authority checks and idempotent scale |
| `players` | rule ID and selector | appearance/presentation isolated from inventory and armor |
| `quests` | exact `PersistenceID` | native registry, per-player state and exact-ID SafeSave ownership |
| `raw` | exact DataTable and row | arbitrary tables are allowed, but row layout is verified against live reflection |
| `recipes` | recipe path plus canonical `PersistenceID` | stable rooted recipe objects; output items round-trip through the item registry |
| `registry` | `Mod:Id` | duplicate definitions fail closed; authority and presentation remain separate |
| `spawns` | spawn ID | server authority, ground/scale idempotence and bounded conditions |
| `strings` | table/source key | text replacement only |
| `vendors` | `Mod:Id`, category and offer identity | category ordering and power/time/quest gates do not leak offers between categories |

## Equipment findings

The game exports use more than the original two convenience behaviors. The
universal lane now supports the explicit fields `GrantedEffects`,
`AssociatedSkill`, `SkillUsed`, and `SkillPerkRequiredToEquip`, plus a guarded
`Items.<path>.Properties` map for real reflected fields such as
`PrimaryActionClass`, `SpecialActionClass`, `BuffDatas`,
`GameplayEffectOnBlock`, projectile fields and `GrantedTags`.

The map is transactional per item. `PersistenceID`, `InternalName`, the four
managed fields and `$` operations are protected. Effect aliases are resolved
through `/effects` to a cooked class before assignment. Surge/Dash and
Shadowveil remain optional storefront-lane native capabilities; failure to
validate either address disables only that feature.

## Buildings and fuel

FModel's UE 5.6.1 processing table confirms the safe station-row surface:

- `Recipes`
- `AcceptedFuels`
- `StartingFuelItem` and `StartingFuelCount`
- `MaxResourceSlots` and `MaxFuelSlots`
- `ProcessingRate` (authored as `Rate`)
- `InfluenceRange`
- `bIgnitesBurning`
- `bStopsWhenRecipeChanges`
- `bCanProcessBeStartedThroughUI`
- `bAutoStartProcess`

RuneSchema exposes those fields under `Overrides.Processing` and maps the
friendly boolean names to their native `b` fields. Fuel arrays support
`Append`, `Replace`, and `Clear`. The loader locates the one processing row
that references the building, validates every requested property and fuel path,
then writes. An ambiguous or missing row rejects only that building override.
Arbitrary component mutation, function calls and save-provider internals are
not exposed because they are not safe data authoring surfaces.

## SafeSave findings

Runtime item clones, authored recipes, runtime quests, created journal/lore
entries, building clones and verified cooked declarations all contribute their
persistence identities to the current snapshot. Items, recipes, quests and
buildings use canonical IDs; journal/lore retain the game's readable entry ID.
Missing content is compared by that exact identity. Player state remains in
the player save—the snapshot contains ownership only.

Steam/GOG JSON cleanup and Game Pass live-provider cleanup remain separate
lanes. Steam's offline transformer now removes retired quests and journal/lore
records by exact ID and verifies the saved ownership marker. It no longer uses
one retired item or recipe as evidence that every quest/journal record owned by
that mod should be deleted. Inventory/loadout and recipe progress are likewise
matched to the ledger ID. Unknown vanilla or third-party identities are never
inferred to be RuneSchema content.

## Remaining deliberate limits

- Cooked assets must exist on every machine that renders them.
- A USMAP helps authoring and diagnostics but never overrides live runtime
  reflection.
- Runtime-created processing outputs must be registered, rooted and
  identity-verifiable; unsafe transient outputs are rejected before queueing.
- World/building persistence and Xbox WGS are not rewritten as loose files.
  Storefront-specific adapters must verify their own read-back contract.
- Unknown reflected fields are not silently ignored. The affected record is
  reported partial while unrelated records continue.
