---
channel: experimental
promote_to: COOKED-NATIVE-DATA-FAMILIES.md
---

!!! info "Experimental · 0.8.1.0"
    This guide describes the experimental branch. For the current public package, see
    [stable downloads](../CURRENT-RELEASE.md). [Experimental overview](index.md).

# Cooked native data registration

These experimental, opt-in registration routes help connect additional cooked
data families. Automated checks cover their definitions; live gameplay, networking
and save/reload tests for these new families are still pending. Try them in a
disposable test setup before including them in a release.

## How the cooked assets connect {#boundary}

The PAK owns assets, references and behavior. RuneSchema enrolls explicitly
declared, already-cooked native objects at its existing startup boundary. It
does not invent properties, grow crops/trees, invoke hooks, dispatch FTUE, grant
entitlements or change difficulty. No new global ProcessEvent callback, polling
loop, native enum extension or per-world registry rebuild is introduced.

| NativeRegistries lane | Native object class | Native subsystem |
|---|---|---|
| ContainerContents | ContainerContentDataAsset | ContainerContentDataSubsystem |
| FarmPlants | FarmPlantDataAsset | FarmPlantDataSubsystem |
| Trees | TreeData | TreeDataSubsystem |
| TreeGrowth | TreeGrowStagesDataAsset | TreeGrowingDataSubsystem |
| FixedTextNotifications | FixedTextNotificationData | FixedTextNotificationDataSubsystem |
| FTUENotifications | FTUENotificationData | FTUENotificationDataSubsystem |
| PlayerHooks | PlayerHook (cooked concrete subclass) | PlayerHooksSubsystem |
| WorldHooks | WorldHook (cooked concrete subclass) | WorldHooksSubsystem |

Classes are in /Script/Dominion. Both current platform captures show these
subsystems inheriting the same five registry containers without additional
reflected subsystem fields. Next, test the feature that uses each registry in-game to check its initialization
timing before shipping a mod with that lane.

## Declaration

Use an enabled mod's loose `registry/*.jsonc` sidecar or cooked registry
discovery (`RegistryJson` / RSREG shards):

```json
{
  "SchemaVersion": 1,
  "Entries": [],
  "NativeRegistries": {
    "FarmPlants": ["/MyPlugin/Farming/PLANT_Example.PLANT_Example"],
    "TreeGrowth": ["/MyPlugin/Trees/GROW_Example.GROW_Example"]
  }
}
```

The logical mod folder/owner need not equal the cooked plugin mount. Paths must
be exact full object paths, not directories, wildcards, Script/Engine references
or {Path, Source: Schema} declarations. Duplicate paths are rejected ignoring
case. Limits: 4096 references per lane, 16384 total references per document,
plus the existing cooked document/shard byte limits. Use the existing shard
mechanism rather than increasing limits for a large mod.

## What registration handles {#registration-guarantees-and-limits}

Only declared objects are resolved. The runtime checks exact loaded paths and
serialized-object provenance; soft-deleted/migrating assets are rejected so the
game's retirement/migration policy is not bypassed. Enrollment follows the
established item/recipe/spell passes. It checks
native types, bounded PersistenceID/InternalName/asset-name aliases, duplicate
claims and deterministic PersistenceID/path ordering before mutation. Every
ready subsystem must belong to a ready native GameInstance on the game thread.
Unexpected reflected subsystem fields or derived subsystem classes are rejected
because they may introduce caches/lifecycle requirements not covered here.

Each asset reuses the existing staged registrar: PersistenceIDToDataMap,
InternalNameToDataMap (both authored name and persistence alias), inherited
AssetNameToDataMap, NetIdToData and DataToNetIdMap. Object-reference and reflected
uint16-wrapper layouts are checked. Changes are staged before publication;
existing IDs remain the prefix and forward/reverse identities must round-trip.
All objects in a declaration are preflighted against the owning instance before
the first commits. Cross-asset aliases are rejected in advance. Per-asset commits
are atomic; there is no claim of a single cross-instance/lane transaction.

The second existing startup pass is idempotent. Native network fingerprints are
published through the existing compatibility mechanism; client/authority must
use matching cooked content and IDs. A missing or not-ready startup subsystem
is an explicit rejection, not a late-world mutation or retry loop. Errors identify
owner/lane and do not block independent registries or save-cleanup categories.
These families are not added to character quest/item pruning authority.

## Other discovered families

- Existing item, recipe, quest, journal/lore, building, spell and equipment-effect
  adapters remain in their established routes.
- Skills/SkillPerks declarations remain blocked pending compiled ESkill consumer,
  startup, UI, unlock and save/network verification. Mod-owned cooked Enums use
  read-only verification, not native ESkill append/reindex.
  The [skill registry investigation](SKILL-REGISTRY.md)
  records the live reflected contracts and the remaining adapter tests.
- Difficulty needs a dedicated policy/cache adapter. Entitlements are platform/
  backend-owned; no generic unlock or bypass is provided.
- CacheAssets declares artwork, not gameplay identities. It is now represented
  in the generated schema as well as the existing parser.

The implementation was compared against captured native metadata on both client
platforms; this is structural evidence, not a completed gameplay fixture. Before release, use isolated
cooked fixtures on both platforms and a real client/server pair; verify native
lookups, early consumers, transitions and save/reload for each relevant family.
