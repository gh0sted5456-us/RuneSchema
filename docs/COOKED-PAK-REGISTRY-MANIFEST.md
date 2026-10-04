# Cooked PAK authoritative registry manifest

!!! warning "Experimental in 0.7.7.1"
    The first native-first manifest slice is available on the experimental
    branch. Items, recipes, quests, combat spells, utility spells,
    held-equipment effects, and ordered player melee and ranged classes are
    supported. AI collections remain unavailable until their authoritative
    lifecycle is confirmed in live reflection.

## Purpose

Large Dragonwilds content packs should be able to carry their gameplay content
through the native cooked PAK route without requiring thousands of RuneSchema
JSON files that repeat individual object paths.

The solution is one small cooked registry manifest inside each PAK.
The manifest declares which cooked assets must join Dragonwilds' authoritative
gameplay registries. RuneSchema discovers the manifest, validates every entry,
and commits only a complete and safe registration plan.

The manifest is passive data. It does not contain executable hooks and does not
modify the game merely because its PAK mounted.

## Goals

- Let large PAKs own their cooked items, recipes, attacks, spells, effects, and
  native relationships.
- Remove the need for one empty RuneSchema file per cooked asset.
- Use Dragonwilds' real live registries as the final authority.
- Preserve stable ordering for replicated combat collections.
- Reject invalid or incomplete manifests without partially changing the game.
- Register process-wide data once per game execution.
- Install validated combat collections once on the component defaults before
  gameplay components are created.
- Produce concise status summaries without logging thousands of success lines.
- Keep save pruning dependent on complete live registries, not on the manifest.

## Non-goals

- The manifest does not repair an incorrectly cooked or corrupt PAK.
- It does not invent persistence IDs or replace missing assets.
- It does not permit clients to grant themselves server-authoritative actions.
- It does not scan and root every loaded Unreal object.
- It does not repeatedly rebuild process-wide registries during menu and world
  transitions.
- It does not replace ordinary RuneSchema authoring for smaller mods.

## Why a manifest is needed

Mounting a PAK makes its packages available, but availability alone does not
guarantee that Dragonwilds loads an asset or places it in the subsystem or
component collection used by gameplay.

Small mods can explicitly reference cooked objects through RuneSchema loaders.
That does not scale well to a PAK containing thousands of items and recipes.
Large packs should bake their ordinary relationships into the PAK and use one
manifest for the remaining authoritative registration requirements.

## Current authoritative lanes

Dragonwilds does not expose one universal registry. Different content families
use different authorities.

| Content family | Authoritative owner | Current state |
| --- | --- | --- |
| Items | `ItemSubsystem.PersistenceIDToDataMap` | Known |
| Recipes | `RecipeSubsystem.PersistenceIDToDataMap` | Known |
| Quests | `QuestDataSubsystem.PersistenceIDToDataMap` | Known |
| Journal and lore | `JournalSubsystem` live identity map | Known, not in the first manifest slice |
| Combat spells | `CombatSpellDataSubsystem` | Known |
| Utility spells | `UtilitySpellDataSubsystem` | Known |
| Held-equipment effects | `HeldEquipmentEffectDataSubsystem` | Known |
| Player melee attacks | `PlayerMeleeAttackComponent.AttackDataCollection` | Experimental and order-sensitive |
| Player ranged attacks | Registered `HeldEquipmentData.RangedAttackCollection`, cached by `PlayerRangedAttackComponent` when equipped | Item-owned; RuneSchema admits only its complete quick/full attack-data pair |
| Player magic | Combat/utility spell subsystem maps plus the equipped `PlayerMagicAttackCollection` | Spell registries supported; equipment collection is a selector, not a separate global registry |
| AI attacks and abilities | `DominionAISubsystem.ActionAttackRegistry` with separate action, attack, and ranged-attack arrays/maps | Layout known; safe lifecycle mutation still requires a runtime audit |
| Buildings | Building catalogues and DataTable collections | Separate registration model |

Map-based registries are primarily identity driven. Attack collections can be
index and order driven, which makes partial or differently ordered registration
unsafe for multiplayer.

## Cooked asset contract

For the recommended shared base plus one mod-owned data asset per PAK, follow
the [shared registry bridge PAK walkthrough](SHARED-REGISTRY-BRIDGE-PAK.md).

RuneSchema already recognizes cooked registry assets whose names begin with
`RSREG_` or `DA_RuneSchemaRegistry`. The manifest should extend that cooked
asset contract rather than introduce thousands of external files.

The string stored in `RuneSchemaRegistryJson` or `RegistryJson` uses this
shape. `Entries` may be empty when the asset is used only for native registry
declarations:

```json
{
  "SchemaVersion": 1,
  "Entries": [],
  "NativeRegistries": {
    "Items": [],
    "Recipes": [],
    "CombatSpells": [
      "/AdditionalWeapons/Magic/SPELL_SpearWard.SPELL_SpearWard"
    ],
    "UtilitySpells": [],
    "EquipmentEffects": [],
    "MeleeAttackClasses": [
      "/AdditionalWeapons/Attacks/BP_Spear_Attack1.BP_Spear_Attack1_C",
      "/AdditionalWeapons/Attacks/BP_Spear_Attack2.BP_Spear_Attack2_C"
    ],
    "RangedAttackClasses": []
  }
}
```

The cooked asset name must begin with `RSREG_` or
`DA_RuneSchemaRegistry`. Set its separate `RegistryOwner` string property to a
stable mod identifier. Assets mounted under `/Game` require that explicit
owner. `NativeRegistries` is deliberately rejected in loose `/registry` JSON;
the declaration must come from the PAK that owns the content.

Each lane accepts at most 4,096 unique cooked object paths, with a combined
limit of 16,384 paths per manifest. Unknown lanes, duplicate paths, invalid
mount paths, wrong object classes, and incomplete ordered collections are
rejected. The current accepted fields are exactly:

- `Items`
- `Recipes`
- `Quests`
- `CombatSpells`
- `UtilitySpells`
- `EquipmentEffects`
- `MeleeAttackClasses`
- `RangedAttackClasses`

`RangedAttackClasses` is retained for schema compatibility but is deferred at
runtime. Dragonwilds does not expose a global player-ranged class registry.
Ranged weapons must be registered as items and carry their own cooked
`HeldEquipmentData.RangedAttackCollection`. RuneSchema resolves that
collection's class default object after the item registry settles and admits
only a complete `QuickAttackData`/`FullAttackData` class pair. It never appends
the lower-level shot or action implementation classes.

Do not put magic spell classes in either attack-class lane. Magic uses the
`CombatSpells` and `UtilitySpells` identity registries. A weapon's cooked
`HeldEquipmentData.MagicAttackCollection` then maps its equipped magic ammo to
those registered starter spells.

## PAK author responsibilities

The PAK author must:

- use a permanent and unique content root;
- cook every declared asset into the shipped container;
- preserve released object paths and persistence identities;
- use the correct Dragonwilds data or class type for each lane;
- bake normal item-to-recipe, recipe-to-station, and unlock relationships into
  the PAK when practical;
- list order-sensitive attack classes in their required replicated order;
- install the same gameplay PAK and manifest on the server and participating
  clients;
- update the manifest deliberately when content is added or removed.

RuneSchema should remain useful for focused integration work that should not be
baked into the large content catalogue. One example is a small `/raw` patch
that adds consumable packs to an enemy loot DataTable.

## Discovery boundary

RuneSchema should discover manifests only from enabled PAK mods.

Discovery must not:

- scan every UObject in the process;
- treat every object under `/Game` as mod-owned;
- infer gameplay ownership from a filename alone;
- load content from a mod marked `0` in `runeschema.txt`;
- let a disabled duplicate folder re-enable the same mod under different case.

The enabled mod order in `runeschema.txt` remains authoritative. Manifests are
processed in that order, with stable lexical ordering used only inside a mod
when more than one manifest is intentionally supported.

## Validation and atomic commit

Registration uses a two-stage transaction.

## Native-first reconciliation

The manifest is an **ensure-present declaration**, not proof that RuneSchema
owns an entry. Dragonwilds' live registry remains authoritative.

RuneSchema must wait for the applicable native startup lane to settle and then
classify every manifest entry as one of these outcomes:

| Live result | RuneSchema action |
| --- | --- |
| The identity or class is already present and points to the same object | Accept it as `native-already-present`; do not write, root, or claim ownership |
| The identity is absent and the manifest object is fully valid | Add it through the verified lane |
| The identity exists but points to a different object | Reject the manifest lane as a conflict |
| An ordered class is already present in the expected relative order | Accept it without appending a duplicate |
| Only part of an order-sensitive collection is present | Reject that collection unless a verified native insertion rule proves the final indices are safe |
| The game registry is not yet complete | Defer without changing the registry or enabling cleanup for that category |

This prevents duplication if a future Dragonwilds build begins registering a
PAK's content automatically. RuneSchema becomes a compatibility backstop rather
than a competing registrar.

Provenance is diagnostic only. An entry that was already present remains native
or PAK-owned; RuneSchema records only entries it actually inserted. Save cleanup
continues to use the final live registry and never deletes an identity merely
because RuneSchema did not insert it.

For multiplayer, the handshake should compare the final effective identities
and order after reconciliation, not only the manifest text. That catches a case
where identical manifests produce different live indices because one platform
or game build registered content natively and another did not.

### Stage 1: resolve and validate

RuneSchema builds an in-memory plan without mutating live registries.

For every entry it must verify:

1. The object path has a valid mounted long-package form.
2. The object resolves from the enabled PAK.
3. The object has the exact class required by the declared lane.
4. Persistence-based data has a non-empty canonical persistence ID.
5. The internal name and persistence ID round-trip to that same object.
6. No identity resolves to two different objects.
7. No ordered class appears twice in the same collection.
8. Collection and entry counts remain inside documented safety bounds.
9. The complete order-sensitive collection resolves before any entry is added.
10. The target subsystem or component layout matches the expected reflected
    class and property types.

If any required validation fails, RuneSchema rejects the affected manifest or
atomic collection without changing the target registry.

### Stage 2: commit

Only a fully validated plan may mutate the game.

The commit must:

- skip entries already registered to the same object;
- reject an identity already registered to a different object;
- append ordered classes in manifest order;
- preserve native entries and their original order;
- assign or verify required network identities through the native subsystem;
- record exactly which changes RuneSchema owns for safe shutdown or rollback;
- verify the live collection after the write;
- roll back all changes from the current atomic collection if verification
  fails.

## Lifecycle rules

### Process-wide subsystem maps

Items, recipes, quests, spells, and equipment-effect data belong to process or
GameInstance subsystem maps. Their manifest registration should occur once
during the startup registration boundary after enabled PAKs are mounted and the
authoritative subsystem instances are ready.

They must not be re-registered merely because the player returns to the menu or
enters another world.

### Player combat component defaults

Melee, ranged, or other player combat components can be recreated with a new
world or player pawn. RuneSchema installs an already validated ordered class
collection on the compatible component's class default once per game execution.
If the first gameplay world's component was constructed before registration
settled, RuneSchema may apply that same complete plan to the first world's live
component once. New worlds must inherit the settled default collection.

This registration must be:

- idempotent;
- completed on the component class default, with an optional, disabled-by-default
  one-shot fallback for pre-existing components in the first gameplay world;
- read-only after RuneSchema observes a different gameplay world;
- free of retained raw component pointers after a component or world dies;
- based on the immutable validated manifest plan rather than a fresh global
  discovery pass.

Entering another world is not permission to mutate a combat component or
rebuild any process-wide registry.

## Object lifetime safety

The manifest asset can safely use soft object and soft class paths. RuneSchema
should resolve those paths only at the correct lifecycle boundary.

RuneSchema must not root every discovered cooked object. Native subsystem maps,
component arrays, and PAK relationships should provide normal Unreal ownership.
Explicit roots should be limited to RuneSchema-owned references that are held
only in native C++ memory and have no reflected Unreal owner. Every such root
must have a documented and symmetric release path.

Before reading or writing an object, RuneSchema must reject objects marked as:

- class default objects;
- archetypes;
- beginning destruction;
- finished destruction;
- unreachable or garbage, where that state is available.

## Multiplayer rules

The server remains authoritative for gameplay.

### Theoretical safety assessment

This design is theoretically multiplayer safe when the server and every
gameplay client have:

- the same PAK build;
- the same manifest schema and contents;
- the same enabled mod order;
- the same cooked persistence identities;
- the same ordered attack-class collections;
- compatible RuneSchema and game versions.

Simply having a similarly named PAK on both sides is not enough. Ordered combat
collections can be unsafe if one machine inserts the same classes in a different
order, because replicated indices may then identify different attacks.

Before enabling manifest-backed replicated gameplay, RuneSchema should compare
the stable manifest fingerprint during connection setup. A match establishes
that both sides intend to use the same paths and ordering. The server should
then remain authoritative for registration-dependent gameplay decisions.

A fingerprint match does not prove that a cooked Blueprint behaves correctly,
so normal host-and-client gameplay testing remains required. It does prevent the
most dangerous silent mismatch: different manifests producing different
authoritative indices or identities.

RuneSchema should calculate a stable fingerprint from:

- manifest schema version;
- owner ID;
- lane names;
- ordered normalized paths;
- persistence identities where applicable.

The host and client should compare this fingerprint before enabling replicated
manifest content. A mismatch must disable the affected custom lane or refuse
the relevant gameplay action with a clear message. RuneSchema must not silently
continue with different attack indices on different machines.

Presentation-only assets still need to be installed on every client that must
render them.

## Failure behavior

An incorrect manifest entry should not crash the game. It should produce a
bounded message and leave the original registry unchanged.

Example:

```text
[PAK-REGISTRY][REJECTED] owner=AdditionalWeapons lane=MeleeAttackClasses
entry=4 reason="object did not resolve to the required player attack class"
result="no melee collection was modified"
```

A PAK can still crash before RuneSchema registration if it is corrupt, cooked
for an incompatible engine build, replaces an incompatible native asset, or
contains broken Blueprint bytecode. The manifest cannot make a fundamentally
invalid package safe. It can make missing paths, wrong classes, duplicate IDs,
and incomplete collections fail cleanly.

## Logging

Logging should be concise and lane based.

Recommended messages:

```text
[PAK-REGISTRY][DISCOVERED] owner=AdditionalWeapons manifests=1
[PAK-REGISTRY][VALIDATED] owner=AdditionalWeapons melee=16 ranged=0 combat_spells=2
[PAK-REGISTRY][COMMITTED] owner=AdditionalWeapons items=0 recipes=0 spells=2 effects=0
[PAK-REGISTRY][COMPONENT-ATTACHED] owner=AdditionalWeapons lane=melee component=PlayerMeleeAttackComponent added=16
[PAK-REGISTRY][ALREADY-PRESENT] owner=AdditionalWeapons lane=melee count=16
```

Detailed per-entry output should be limited to a small diagnostic sample, with
aggregate totals for the rest.

## Relationship to save pruning

The manifest is not a save-cleanup authority.

After all valid content has loaded and registration is sealed, RuneSchema takes
its cleanup view from the applicable live Dragonwilds registries. A saved
persistence ID is removable only when it is absent from the complete matching
live registry.

If a required registry is incomplete or a manifest failed before the registry
became trustworthy, cleanup for that category must remain disabled for that
launch.

## Relationship to ordinary RuneSchema loaders

Ordinary mods may continue using `/assets`, `/recipes`, `/raw`, `/buildings`,
and the other loaders with full cooked mod paths.

The manifest is intended for authoritative registration, especially for large
or order-sensitive cooked content. It does not prevent a later compatibility
mod from applying a focused and validated patch through normal RuneSchema load
order.

When RuneSchema receives more than 1,000 authored recipe definitions, it emits
one advisory warning and continues loading without a limit. The warning suggests
that the native PAK and manifest route may be easier to maintain; it does not
gatekeep the mod.

## Implementation phases

### Phase 1: generalize the known melee lane

- Replace the hardcoded AdditionalWeapons class array with a cooked manifest.
- Preserve the currently verified `PlayerMeleeAttackComponent` and
  `AttackDataCollection` behavior.
- Require complete resolution before attachment.
- Add stable fingerprints, concise logs, and re-entry tests.

### Phase 2: map-based gameplay data

- Add manifest lanes for combat spells, utility spells, and held-equipment
  effects.
- Reuse the existing subsystem map validation and network identity logic.
- Confirm host/client parity and safe duplicate handling.

### Phase 3: ranged combat

- Steam mapping confirms `PlayerRangedAttackComponent` inherits the shared
  `PlayerAttackComponent.AttackDataCollection` array, but inheritance alone
  does not make that array a global ranged registry.
- The authoritative player-ranged path is the registered
  `HeldEquipmentData.RangedAttackCollection`, which supplies quick/full attack
  data and is cached by the live ranged component when the item is equipped.
- RuneSchema validates this path after ItemData registration and admits only
  complete quick/full attack-data pairs to the live ranged component. Shot and
  action implementation classes are never inserted.
- Verify the same reflected layout on Game Pass before changing this model.

### Phase 4: AI attacks and abilities

- Determine whether each AI family uses a global registry, archetype-owned
  arrays, behavior assets, or class-default references.
- Avoid forcing dissimilar AI systems into one false universal registry.
- Add separate typed lanes only for verified layouts.

### Phase 5: authoring and tooling

- Publish the manifest JSON schema.
- Add an Unreal authoring helper for the cooked registry DataAsset.
- Add a pre-package validator that resolves paths and checks duplicates.
- Add Helpy diagnostics that show manifest, registry, and multiplayer status
  without allowing unsafe runtime mutation.

## Required tests

At minimum, test:

- a complete melee manifest;
- a complete ranged manifest;
- one missing melee class with zero partial insertion;
- one missing ranged class with zero partial insertion;
- duplicate attack paths;
- wrong-class entries;
- already-present native entries;
- two enabled mods contributing distinct ordered collections;
- deterministic load order from `runeschema.txt`;
- a mod marked `0` contributing nothing;
- host and client fingerprint equality;
- host and client fingerprint mismatch;
- first world entry;
- menu return and world re-entry;
- a second player component created in the same world;
- shutdown and restart;
- Steam and Game Pass layouts;
- save cleanup after a manifest-backed mod is removed;
- cleanup disabled when the applicable live registry is incomplete.

## Open questions

1. Does the current Game Pass build retain the Steam item-owned
   `HeldEquipmentData.RangedAttackCollection` layout?
2. Which AI families share the verified `AIActionAttackRegistry`, and at what
   lifecycle point is its `UpdateCache` safe to invoke?
3. Can the existing cooked registry DataAsset class carry typed soft arrays, or
   should version 1 retain a strictly validated embedded JSON string?
4. Which native subsystem function should assign network IDs for each
   map-based lane?
5. Should a multiplayer manifest mismatch disable only the affected lane or
   prevent the player from joining?

These questions must be answered with live reflection and multiplayer testing,
not assumptions from filenames or mappings alone.

Return to [Create new PAK content](PAK-CONTENT-GUIDE.md).
