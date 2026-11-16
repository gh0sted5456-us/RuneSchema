---
channel: experimental
promote_to: COMPANION-SAVES.md
---

!!! info "Experimental investigation"
    This is a proposed character-save API and an isolated policy prototype.
    It is not installed runtime functionality. Automatic item restoration and a
    companion-data page in Helpy are not enabled. [Experimental overview](index.md).

# Character companion saves

A RuneSchema companion save could retain mod-owned character state separately
from the game's active save. This would give mods a shared place for morph values,
flags, journal progress, recipe unlocks and quest state. It could also hold complete
modded item records before missing definitions are removed from the native save.

The first scope is **characters**. World files, placed buildings and shared
containers need separate binary persistence and ownership adapters and are deferred.
The companion stores data and mod identities; it does not contain mod binaries or
replace the need to reinstall a required mod.

The intended end state is a native save containing vanilla state and a companion
containing mod-owned state. RuneSchema still prepares and relaxes the native load.
After the base character loads, it applies available companion state through
verified game adapters. Modded items still use the game's real inventory during
play; the separation is a persistence boundary, not a second live inventory.

## What the current implementation establishes

| Existing contract | What it supports | What it does not establish |
| --- | --- | --- |
| Selected-character JSON interception | Access to the original character payload before cleanup | A successful native character-save completion receipt |
| Inventory cleanup | Removal of individual invalid records and their affected loadout references while preserving valid entries | Exact reconstruction of a full native item instance |
| Quest and journal ownership | Exact mod ownership declarations and scoped progress handling | Ownership of every historical item in every save |
| Journal persistence bridge | A verified native write adapter that appends journal ownership after the game's writer | Arbitrary new root fields surviving all character-save writers |
| Native inventory inspection | Runtime item GUIDs, slot contents and quantities | GUID stability across reload, splitting, merging and cloud rollback |
| Save Recovery storage | Character identity checks, verified publication and restoration safeguards | An atomic transaction spanning both the game's save and a new companion file |

Relevant source areas are `Core/SaveCleanup.h`, `Core/PersistencePruner.cpp`,
`Loader/JournalPersistence.inl`, `Loader/QuestSaveOwnership.h`,
`SDK/Helper/EquipmentDelivery.h` and `shared/SaveBackupStore.h`.

### Cooked Blueprint API boundary

The 0.8.1.0 candidate implements opt-in `RuneSchemaReadProfile` and
`RuneSchemaWriteProfile` on a registered mod-owned component. See the
[local mod profile contract](LOCAL-MOD-PROFILES.md) for exact Blueprint signatures,
AppData storage and revision checks. These local functions are separate from
the cooked networking protocols and the proposed native inventory companion
engine. `ImportLegacyProfile` is not implemented; the mod owns migration.
A native Blueprint SaveGame node alone does not establish the required AppData
location, namespace ownership, revision checks or migration behavior.

| Requested capability | Current source contract | Remaining work |
| --- | --- | --- |
| Local mod-owned profiles | A cooked mod-owned SaveGame passes an isolated Steam shipping round trip. The candidate also implements registered JSON profiles with exact controller/GUID binding, atomic publication and expected revisions | Shipping Blueprint dispatch, target-platform storage and frontend identity still need verification. Remote messages must never dispatch local file operations |
| General mod-data receive | Server-side C++ `ModDataReceived` receives assembled messages; the Lua helper assembles client notifications | A native client sender/assembler and a registered cooked consumer callback; the existing presentation consumer is a separate protocol |
| Cosmetic presentation | A declared cooked ActorComponent receives `OnRuneSchemaPresentation(PluginId, Connection, EntityId, Payload, Revision)` on the client GameState | An authoritative publisher and a mod-owned actor identity mapping; no arbitrary asset paths or local profile writes |
| Character appearance | The local [appearance transport](CHARACTER-APPEARANCE.md) registers per-mod routes, derives the actual subject/GUID on the server and delivers committed snapshots through the cooked client RPC | Rendered multiplayer acceptance with a real morph provider; the provider owns strict body/mesh/control validation and never writes profiles from remote messages |
| Selected or newly created frontend character | Current local GUID lookup calls `DominionPlayerControllerBase:GetCharacterGuid` on a verified local controller | A verified selection/creation identity handoff; this controller query does not prove which preview actor or creation draft owns a GUID |

The mod-data sender is an authenticated, opaque session peer. It is not a
character GUID or a client-resolvable Actor identifier. A cosmetic consumer
needs a server-verified mapping before applying another player's profile; it
must not trust a claimed GUID in the uploaded payload. Payload schema versions
are also separate from the transport packet format.

Do not route local profile operations through `ServerRequestRuneSchemaAction`
or extend a world PIN into filesystem authority. Profile previews and commits
need explicit local character identity, while received appearance data changes
only the corresponding cosmetic actor state. Missing frontend identity must
leave a draft unbound rather than choosing a display name or last-used profile.

### Shipping mod-owned SaveGame probe

The Steam shipping executable `++dominion+hotfix-CL-254097` successfully loaded
a mod-owned cooked SaveGame class from a verified IoStore package in an isolated
headless test. Native `CreateSaveGameObject`, `SaveGameToSlot`,
`LoadGameFromSlot` and `DeleteGameInSlot` completed successfully. The test
verified all five profile fields, fifteen nondefault float values and a 64-bit
revision of `4294967313`. The serialized file was a 2,158-byte Unreal GVAS image.
The unique disposable slot was deleted through the native API, and the process
exited normally without loading a character or gameplay world.

This proves a cooked Blueprint can own its profile schema and use the native
save APIs in that shipping Windows test. It does not require a RuneSchema-owned
profile format. The mod owns its schema, migration, UI and explicit Apply/Cancel
behavior; RuneSchema supplies mounting, registration and any separately verified
identity or platform adapter it actually needs. Networked appearance updates
change cosmetic state only; the local player alone writes their profile.

The probe used `-nullrhi`, `-NoSteam` and an isolated `-UserDir`. Its actual native
Saved directory was queried before writing, and the file appeared under that
directory's `SaveGames` folder. This is not proof of a normal account's default
path, Game Pass/WGS behavior, Wine compatibility, frontend GUID handoff or
successful editor UI/consumption delivery. Those remain separate acceptance
checks; profile Apply must not guess an identity while they are unresolved.

The Lua test harness also exposed an important distinction: UE4SS's global
`LoadAsset` looks up asset-registry records, which this minimal fixture did not
contain. The successful test used the engine's typed soft-reference conversion
before `LoadAsset_Blocking`. Passing a string directly as a soft-object reference
is not a valid substitute. Native Blueprint soft-reference nodes avoid that Lua
argument mismatch.

### Existing recovery helper is not the companion engine

`Core/SaveRecovery.h` is an older test-only recovery planner. The native
quest-ownership contract explicitly keeps its calls out of the runtime load,
registration and pruning paths. Its passing tests do not mean companion saves
are already installed.

It captures differences between original and cleaned JSON and retains exact
historical registry bindings. Those are useful investigation inputs, but its
restoration policy does not implement the proposed paired-save lifecycle:

- It marks an offered record `ClaimedForNativeLoad` before a verified native
  completion receipt. This prevents blind replay but can leave a failed load's
  record held indefinitely.
- It attempts the original inventory slot. An occupied original slot is blocked
  even when a different valid slot is available; native capacity is not checked.
- Its existing-item guard compares `ItemData`, which identifies a definition,
  rather than the native instance `GUID`. Multiple legitimate instances can
  share that definition.
- Its archive key includes the slot and serialized payload. Moving an item or
  changing its quantity can therefore produce another historical record instead
  of updating one canonical instance.

Do not wire this planner into character loading as a shortcut. Reuse verified
ownership, native record validation and durable publication patterns while
replacing those lifecycle rules. Whole-save recovery remains a separate feature.

Unreal supports custom save objects and multiple save files, but that general
capability does not prove that Dragonwilds preserves arbitrary extra fields or
automatically synchronizes companion files through its platform save system.
See [Epic's saving and loading documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/saving-and-loading-your-game-in-unreal-engine?application_version=5.6).

## Native contract discovery

A bounded headless probe on November 2, 2026 inspected **633 reflected records**
on Steam's `++dominion+hotfix-CL-254097` executable. Its executable SHA-256 matched
the installed Steam copy. All inspected property lists completed without errors.
The probe ran with a fresh native Saved directory, only its inspection mod
enabled, and no character or world loaded; it exited normally. It used
[UE4SS's property/function inspection API](https://docs.ue4ss.com/dev/lua-api/classes/ustruct.html).

These are discovered contracts, not proof of a successful save transaction:

| Native contract | Observed reflected fields | Use and remaining verification |
| --- | --- | --- |
| `PersistenceSubsystem:OnPersistentStoreSavePlayerResult` | `Handle`, `bSuccess`, `CharacterSaveSlot` | Candidate completion receipt. Must observe a real write and prove which staged character revision its handle acknowledges. |
| `PersistenceSubsystem:OnPersistentStoreLoadPlayerResult` | `Handle`, `bSuccess`, `CharacterSaveSlot`, `PlayerState` string | Character payload boundary with identity/result context. Existing selected-load safeguards still distinguish entry from preview. |
| `DominionPlayerControllerBase:Client_SaveToDisk` | One `DomClientDiskSave` struct | Server-to-client disk-save route. Its struct exposes no reflected member fields, so this is not a validated editable JSON parameter. |
| `DominionPlayerControllerBase:Server_ClientSaveToDiskResult` | `Handle`, `InCharacterSaveSlot`, signed-byte `Result` | Acknowledgement route from client to server. The byte's meaning and its correlation with the completion callback remain unverified. |
| `InventoryComponent` | `OnInventoryLoadedFromSave`, `bHasLoadedFromSave`, `OnInventoryChanged`, `MaxSlotCount`, `ItemSlots`, `JsonInventory` | Candidate readiness/change signals and capacity source. The JSON-named field is not yet a verified item serializer or safe import API. |
| `InventoryComponent:AddItemToSlot` | Existing `Item` object, `SlotIndex`, boolean return | Candidate insertion of a reconstructed instance. Full payload reconstruction and native invariants need disposable round trips. |
| `InventoryComponent:AddItemByData` | `ItemData`, `Count`, `DurabilityPercentage`, `GameplayTags`, boolean return | Normal granting path. It does not expose an original instance GUID or every serialized field, so it cannot be assumed to restore an exact saved instance. |

The completion callback closes an important discovery gap: an engine-managed
receipt is a concrete candidate, rather than a timer or file timestamp heuristic.
Its existence alone does not prove callback ordering, successful cloud/provider
durability or handle uniqueness after restart. A reflected save-start call such
as `SavePlayerState(DomPC)` is not a completion acknowledgement.

The client/server route also means a dedicated server cannot simply claim that
writing its own companion file completed a player's local save. Character
identity, owner connection and the client's persistence receipt must travel
together through the authenticated RuneSchema bridge. A world PIN authorizes
configured self-service actions; it is not permission to replace arbitrary save
namespaces or trust a client-supplied inventory count.

The first inventory adapter should use the native inventory-loaded signal as an
admission candidate, then verify the selected character GUID and actual
registrations before applying companion state. It must not reload the whole
character payload afterward, which could overwrite already-loaded vanilla state.

The inspected inventory exposes `IsValidSlot`, `GetNumEmptySlots`,
`GetSpaceAvailableForItemByData` and `CanAddItem`. These are candidates for
capacity/admission checks, not proof that arbitrary restored instances are safe.
The saved `MaxSlotIndex` is not a verified capacity limit. Revalidate the target
immediately before insertion and verify the exact instance and quantity afterward.
The existing equipment-delivery helper demonstrates scoped reflected-call
ownership, current-controller checks and GUID readback; it does not reconstruct
a saved item or serialize every unknown field.

Game Pass's save-provider path and these callback correlations still require
their own disposable native test. No raw Xbox WGS container edits are proposed.

### Follow-up scan: actual item and equipment shape

A second isolated Steam frontend scan inspected **704 reflected records** with
zero property-inspection errors on the same executable. It exited normally and
its temporary loader configuration was restored with an exact hash check. No
character or gameplay world was loaded.

The reflected `Item` class exposes `Guid`, `Count`, `Durability`, `GameplayTags`,
`ItemDataPersistenceId`, `ItemData`, `NumMasterworkUpgradesApplied` and
`AddedToInventoryTimestamp`. The character-save result enum declares
`NotStarted=0`, `InProgress=1`, `Success=2`, `InvalidJSON=3` and `MiscError=4`.
This does **not** yet prove that the client disk-save acknowledgement's signed
byte uses that enum. Do not interpret zero as success or infer the mapping from
the enum name alone.

A separate read-only structural scan of four existing Steam character files
examined **232 bag/personal-storage items**. Every item had a unique serialized
`GUID`: a canonical 22-character URL-safe Base64 encoding of 16 bytes. The
character IDs in these files were 32-character strings. Keep those identity
representations distinct; the mapping to native FGuid word order still needs a
verified round trip. The saved item fields included `ItemData`, `GUID`, `Count`,
`Durability`, `NumUpgradesApplied` and `AddedToInventoryTimestamp`. Reflection
names and serialized field names are not interchangeable, and this sample does
not establish the complete schema for every derived item class. All source
file hashes were unchanged after inspection.

The same files contained ten equipped records with their own `ItemData`,
`GUID`, durability and upgrade fields. None of those GUIDs appeared in the
bag/personal-storage records. Each `PlayerInventoryItemIndex` pointed at a bag
slot containing a different definition and instance. The index alone therefore
cannot establish the identity or lifetime of the equipped record.

An isolated synthetic reproduction against the current `SaveCleanup::Plan` and
`PlanAvailable` confirmed a related defect: pruning an unrelated bag slot can
also remove a registered, independently identified equipped record whose saved
index matches. The local corrective build now distinguishes complete equipped
records from known single-field legacy references across cleanup, baseline
merge, offline bag-item removal and NPC export. Synthetic regression checks
cover that distinction; this is not yet a native equipment-restoration adapter.
Companion capture must preserve complete equipped instances, not only bag
records and index links.

The inspected `PlayerCharacterInfo` struct exposes only `DisplaySlotName` through
reflection. A display name is not a verified character GUID or durable save
generation. The client disk-save struct still exposes no reflected member
fields, and the inspected item/inventory function set exposes no verified full
JSON serialization/import pair. These remain native adapter discovery work,
rather than permission to construct layouts or call guessed serializer methods.

## Proposed mod API

Each mod receives a namespace bound to its registered RuneSchema identity and the
selected character's exact GUID. A mod can read and update its own JSON-compatible
state and request a durable write. It cannot choose another mod's namespace or
use a generic data write to grant server-owned inventory.

Version labels are informational. Optional author migrations may transform known
data, but an unknown version or failed migration keeps the original payload
available for recovery rather than deleting it.

The existing native plugin host already has JSON service registration and calls.
The first implementation can use that router rather than adding another IPC
process or allowing mods to choose filesystem paths. Bind each request to the
registered mod, current selected-character identity and expected revision.
Arbitrary caller-supplied character GUIDs or namespace names must not decide
where state is written. Availability should distinguish no active character,
loading, ready and a specific held/recovery condition.

| Data | Companion role | Native integration still needed |
| --- | --- | --- |
| Morphs and custom flags | Author-owned values, defaults and optional migrations | The mod applies its values when its character component is ready |
| Journals, recipes and quests | Mod-owned progression, unlocks and references | Domain adapters reconcile that state with the game's active progression |
| Items | Full held instance records and restoration receipts | Verified item serialization, identity, capacity and commit handling |

Assets and item/recipe definitions remain in their mods and registries. The
companion does not turn an absent definition into a usable gameplay item.

Files would use RuneSchema's managed AppData storage, separated by platform and
character identity. This is active saved data, independent of optional backup
snapshots. It requires its own paired export/restore support; platform cloud saves
must not be assumed to carry it automatically.

## Native save first, companion second

The proposed load sequence is:

1. Identify the selected character and matching save generation. If migrating an
   older mixed save, capture its mod-owned data before cleanup can remove it.
2. Let RuneSchema prepare/relax the native load, then let the game load the base
   character using its normal format and native lifecycle.
3. Once the actual registries and character components are ready, apply the
   enabled mods' companion state into those native components.
4. Leave unavailable or incompatible records held in the companion. A full
   inventory produces a partial result and an explicit pending remainder.

The matching write adapter is essential. It must capture the current mod state,
including items consumed, transferred or removed, before preparing a vanilla-only
projection for the native writer. The live inventory must remain intact while
that projection is serialized. Both sides need a verified save-generation
association and commit recovery; simply writing the companion after loading is
insufficient.

In this fully separated design, successfully applying an item does **not** delete
its canonical companion record. That record remains its saved representation
until a subsequent committed inventory change updates or removes it. Restoration
receipts prevent replay within the same load/transaction. They cannot replace the
canonical saved state. Otherwise the next vanilla-only load would lose the item.

This complete read/write separation has not been verified. The current prototype
tests the narrower recovery/escrow policy, where a confirmed native save can
still contain the restored instance. Its completion rule must be adapted to the
paired-save manifest before enabling a vanilla-only native writer.

## Mixed inventory handling

1. Read the selected character's original payload before pruning or native cleanup.
2. Capture the full unavailable modded records, including quantities, custom
   fields, inventory kind and original slots. Record exact ownership when proven.
   Do not guess ownership from a display name or item prefix.
3. Durably publish those records before claiming they are recoverable. Let the
   existing selected-character cleaner remove invalid active references. Valid
   vanilla records remain untouched.
4. When the owning mod and its actual registry definitions are ready again,
   reconcile the held records against the current native inventory.
5. Restore records into valid free space, preserving occupied vanilla slots.
   Keep anything that cannot fit pending and report the result through
   RuneSchema's log and Helpy.
6. Mark recovery complete only after the matching persistence transaction commits.
   Keep the canonical companion item state for the vanilla-only design and record
   a receipt so reopening Helpy cannot grant it again within that transaction.

The initial policy restores complete records/stacks. It does not auto-equip,
split or merge customized stacks without verified native rules. A changed stack
limit or incompatible instance stays pending. Supporting partial quantities
within one stack requires that additional adapter; restoring some complete
records while retaining the rest is already covered by the prototype.

An item cache is presentation data, not evidence of registry readiness. Likewise,
an enabled folder is not proof that its PAK and registry successfully loaded.
Unknown ownership or an ambiguous item identity needs review instead of an
automatic grant. A server's missing cosmetic mod must not erase client-owned
cosmetic state.

## Crash and rollback reconciliation

The companion and native save are separate files. Writing one successfully does
not mean that the other committed. A save/restore transaction therefore needs a
durable intent, a verified native save-generation association and a completion
receipt. Reconciliation must distinguish an already restored item from a pending
one, and recognize cloud replacement, rollback or a different save branch.

Stage the complete companion revision durably before removing mod-owned data
from the native projection. If staging fails, preserve the original native path;
never strip native state and then discover that its companion was not saved.
A hash of the vanilla projection alone cannot identify a transaction: mod-only
changes can leave that projection unchanged. The native receipt must correlate
the selected character, staged companion revision and current persistence
operation, and reject stale callbacks from an earlier connection or load.

Fully automatic rollback matching also needs a durable pairing identity exposed
by the native save/provider or a verified harmless native metadata field. A
process-local callback handle cannot provide that identity after restart. If
two companion revisions have identical vanilla projections, their hashes cannot
tell which one a cloud-restored native file belongs to. Until a persistent
anchor is proven, paired export/restore and an explicit ambiguous-branch hold
are required; do not promise transparent cross-machine cloud synchronization.

The recovery prototype models these tokens explicitly. The actual Dragonwilds commit
contract and persistence of item instance GUIDs are still unverified. When a
previous restoration might have occurred but the current inventory cannot prove
it, the prototype holds that record for review and never blindly grants it again.

Native inventory remains authoritative during play. A stale inventory snapshot
cannot be treated as a second independently writable inventory: items may have
been traded, consumed, dropped or moved to a chest since it was captured.
Only a complete authoritative inventory observation can retire an active item
record. An absent owner, unloaded inventory or failed observation must retain
held records rather than treating missing data as a confirmed deletion.

## Helpy management and performance

The proposed companion-data tab belongs in [Save Recovery](SAVE-RECOVERY.md).
It would show character and mod namespaces, pending records, restored receipts
and specific reasons for a hold. Actions would include retrying restoration into
available space and exporting/importing the matching character and companion
data. Inventory actions must follow the existing host/server authority checks.

Capture and flush work should follow selected-character load, committed
inventory changes and native save completion. No per-frame inventory enumeration
or continuous disk polling is required for this design.

## Verification and next gate

The smallest useful implementation order is:

1. A mod-owned data API for flags and morph values, with atomic AppData writes,
   revisions, readiness events and offline inspection/export. This can be
   verified without changing the native inventory writer.
2. Native save/load observation on a disposable character. Correlate the actual
   character identity, staged revision, save receipt and reconnect/load lifetime.
3. One full item-instance round trip with occupied slots and missing-mod tests,
   followed by repeated vanilla-only save/companion-load cycles. Retain unknown
   fields and prove consumption, movement and failure recovery.
4. Domain adapters for journals, recipes and quests, plus Helpy management.

The first step must not advertise inventory preservation or vanilla-only native
saves before the later native adapters pass. The current journal writer shows
how to extend one verified persistence domain; it does not establish an
inventory serializer or whole-character save completion.

The isolated prototype passed **26 synthetic tests**: namespace isolation,
atomic publication and stale-write rejection, mixed inventories, loadout links,
separate personal storage, full and partially available inventories, restart
persistence, registry readiness, incompatible payloads, duplicate prevention and
ambiguous crash/rollback handling. Six existing native cleanup/ownership tests
also passed. The prototype uses synthetic instance identifiers and save commit
tokens; it never opens or changes real game saves.

Before runtime integration, a disposable character test must verify full native
instance round trips, persisted GUID behavior, actual capacity/stack rules, a
successful save-completion contract and the complete vanilla-first/companion-second
cycle across repeated saves. Steam and Game Pass each need their own
native-boundary check, followed by authority, transfer, crash and rollback tests.
World and chest support remains a later investigation.
