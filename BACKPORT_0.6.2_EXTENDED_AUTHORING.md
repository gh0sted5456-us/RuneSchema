# RuneSchema 0.6.1E extended authoring backport

Base: `220a7e4417325cf78b0c43cabc10ae166e522e5e` (the frozen 0.6.2 Experimental.2 tree).

## Included

- `/assets` accepts the deterministic `$Patch`/`$Target` envelope. Patch targets cannot change `PersistenceID` or `InternalName` and nested create/clone/patch directives are rejected.
- `/assets` accepts exact-case `$Clone` and `$Create` directives. `$Clone` is restricted to `ItemData`, constructs a distinct runtime object, copies only safe reflected properties, clears inherited identity, applies authored fields, and registers the new identity with `ItemSubsystem`.
- Created items receive deterministic `/Game/RuneSchema/<mod>/Items/<internal-name>` runtime paths and remain discoverable by authoring path, object path, internal name, and persistence ID.
- Generic hard- and soft-object property writes now use the clone registry as a fallback, translating authored aliases into the live canonical object. This is required for `/recipes`, `/raw`, and nested `/assets` references to cloned items.
- Extended consumable-pack handling accepts concise and native exported field spellings and resolves cloned items through the normal reflected soft-object writer.
- Native item `bSoftDeleted` (the exact cooked FModel spelling) is exposed through the ordinary reflected `/assets` writer and documented as an availability/retirement flag; clone identities remain permanent and must not be reused.
- Recipe `AddTo` placement follows the demonstrated `Table`/`Row`/`Category` contract. Category entries store the rooted recipe's soft path without manually constructing a weak pointer during early GameInstance initialization.
- New items require explicit, canonical `PersistenceID` and non-empty `InternalName`; the later temporary/session identity fallback was intentionally excluded.

## Narrow dependencies

- `AssetCreateSchema`: directive validation and deterministic runtime item paths.
- `JsonPatchDirective` and `JsonLoadOrderMerge`: strict patch parsing and deterministic recursive merge rules.
- `PersistenceId`: canonical Dragonwilds persistence-ID validation.
- `AssetConsumablePackSchema` and the header-only `SpawnSchemaFields`: extended `/assets` pack validation.
- `IdentityOverride`: included only for native item identity-map registration/read-back. It is referenced only by the asset loader in this backport; recipe and journal loaders remain unchanged.
- The later `ActorHelper` numeric argument/result and normalized-path operations, plus the header-only `PlayerAttributeNames` normalizer, are included only for the complete player-adjustment path.

## Deliberately excluded

- Later recipe, journal, raw-table, blueprint, building, course, quest, event, NPC, dialogue, resource, and spawn-identity rewrites.
- Temporary generated item identities and their later configuration/UI controls.
- Content-ownership preflight and later global runtime registries.

## `/players`

- `/players` is integrated into the existing 0.6.2 spawn/runtime loader and its engine tick; no later spawn, quest, event, inventory, dialogue, or NPC runtime was imported.
- Rules are re-evaluated once per second, so players joining later and replacement pawns created on respawn receive their rules.
- `PlayerName`, `PlayerNames`, `PlayerGuid`, and `PlayerGuids` selectors are supported.
- `*` selects all connected players. `*1`, `*2`, and subsequent numbered wildcards select stable first-seen connection slots for the current process rather than controller-enumeration order.
- The complete later adjustment set is transplanted: scale; absolute and multiplier health/stamina; general damage/defense; walk/run speed; absolute and multiplier carry weight; poison resistance; stamina recovery; physical/magical/ranged attack and defense; named `AttributeMultipliers`; typed `Attributes` operations (`Set`, `Add`, `Multiply`); and appearance selection.
- Baselines are captured per pawn and resolved property/attribute, preventing multiplier drift while allowing deterministic composition of matching rules.
- Appearance supports body, face/head, hair, facial hair/beard, skin, hair color, eye color, and eyebrow color. Enabled appearance packs are registered before player rules, so `Source` references are independent of `mods.txt` position.
- Custom appearance sources require safe vanilla fallbacks. Provenance is persisted under `RuneSchema/player-data` so disabling a source pack can restore fallback rows.
- Rules naturally reapply to replacement pawns after respawn; numbered player slots remain stable for the world session.

## Named AI spawns

- `AISpawnPoint` entries accept a top-level `DisplayName` of 1–128 characters.
- The name is applied to the spawned character instance's `AIName`; the shared AI class default is never mutated, allowing two instances of the same class to have different names.
- No later spawn-definition registry, NPC system, event system, or replacement scheduler was imported.
- `PowerLevel` retains the native spawn-point field name used by both `/spawns` and `/assets`.
- Per-instance `HealthMultiplier` and `DamageMultiplier` provide bounded combat scaling without editing shared class defaults.
- `LootRow` selects a native `DT_EnemyLootDropTable` row, allowing named characters to receive additional drops authored through `/raw`.
- `CharacterProperties` and `ComponentProperties` expose exact reflected FModel fields on the spawned character and its named components, including class-specific curves, health, attack, and attribute fields.

## Credits

- The DLL and UI identify version `0.6.1E` and state: RuneSchema created by Snorkles; extended features by Jonesing4Space. PalSchema lineage is credited separately to Okaetsu.

## Verification

- Configured against pinned UE4SS commit `0bfec09e` using Visual Studio 18/MSVC 19.51 and Windows SDK 10.0.26100.
- Built successfully with configuration `Game__Shipping__Win64`.
- Output: `RuneSchema.dll`.
- Re-examined against the supplied working handoff and restored its narrow `RuntimeObjectResolver` path plus soft-pointer resolved-object cache. Invalid recipes are now excluded from placement and unlocks if a required property write fails.
- Runtime game validation is still required for every player attribute family, appearance source/fallback persistence, join/respawn ordering, UI refresh, per-instance AI name display, native item registration, clone persistence across restart, deterministic multi-mod patch ordering, and consumable-pack soft references.
