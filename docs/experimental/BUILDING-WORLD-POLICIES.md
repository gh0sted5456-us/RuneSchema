---
channel: experimental
promote_to: BUILDING-WORLD-POLICIES.md
---

!!! info "Experimental · 0.8.1.0"
    This guide describes the experimental branch. For the current public package, see
    [stable downloads](../CURRENT-RELEASE.md). [Experimental overview](index.md).

# Cooked world policies

This page describes the maintained source candidate after 0.8.0.3. It is not a claim that an older installed DLL supports these additions.

A cooked Blueprint actor can provide world policies to RuneSchema. Keep the mod's definitions, range calculations, artwork and interface in its PAK. RuneSchema validates the callback signatures and connects them to the game's native behavior. These are optional callbacks on a live actor in the affected world; they do not need a separate mod DLL or a shipped Lua poller.

## Provider contract

Implement `RuneSchemaBuildingPolicyVersion() -> int32` and return `1`. Existing building support, placement, damage, destruction and overlay callbacks keep their contracts.

These optional callbacks accept `Position: Vector` and return a Blueprint boolean:

| Callback | When true |
| --- | --- |
| `RuneSchemaPlayerPhasing` | Living authoritative or locally controlled players in range can pass through geometry using native flying movement. Space rises and Left Control descends. Leaving the area, disabling the policy, death and cleanup restore the previous capsule collision and movement mode. |
| `RuneSchemaFreeBuildingMaterials` | Temporarily waives recipe requirements during native preview, selection and construction calls. Requirements are restored after each synchronous call. Native spatial checks and multiplayer building identity checks remain active. |
| `RuneSchemaFreeCraftingMaterials` | Temporarily waives crafting materials for the actual crafter in range. Recipe unlocks, skills, station requirements, outputs and XP retain native handling. |

Free crafting wraps native `InventoryController:CraftRecipe` and
`Server_CraftRecipe`, plus `CanCraftRecipe`,
`GetMaxAmountCraftableOfRecipe` and `HasRequiredConsumedItemsForRecipe` ingredient
checks. It uses the inventory controller's verified owning player, the explicit
crafter, or a unique owner proven by the supplied personal inventory. Shared
storage alone and ambiguous player inventories cannot grant the waiver. Client
UI checks apply only to the owning player; authority checks use the server's
current pawn position.

The candidate also covers direct native C++ menu checks, which do not pass through
Blueprint hooks. Building slots calculate their normal buildable appearance with
the material waiver in scope. Crafting uses the native eligibility, quantity and
ingredient checks; it does not replace their unlock or station decisions.

Recipe arrays are borrowed only during synchronous game-thread calls and restored
afterward. For AnyOf recipes, `bConsumeItemsIndividually` is scoped and restored
with the ingredients so an empty temporary array does not incorrectly mean no
crafting choice is available. Nested calls from an ineligible player temporarily
see the original costs and flag, even if another player has a waiver for that
same recipe. Passive processors, repair
requirements and fuel remain outside this callback. The native crafting and
owning-client tests must pass before a combined content release uses the candidate.

An open local menu refreshes when its material policy changes. Crafting keeps the
selected recipe and quantity; building updates are spread across frames rather
than refreshing the whole catalogue in one frame. These checks use the existing
slow update only while the relevant native menu is active. Closing the menu stops
its refresh work. No global difficulty or free-crafting setting is changed.

An individual building actor can additionally opt in with
`RuneSchemaCanDismantleOwnBuilding(Player: Pawn) -> bool`. The argument is the
requesting controller's current pawn. Keep distance, preview and controlled-pawn
checks in that cooked callback. This changes only that actor's dismantle
permission; it does not grant global demolition rights.

Each scalar callback below accepts `Position` as an Unreal `Vector` and returns `ReturnValue` as a Blueprint double. Positions use native world coordinates in centimetres. Return the neutral value outside the affected area or when the provider is disabled.

| Callback | Neutral | Meaning |
| --- | --- | --- |
| `RuneSchemaFarmProtection` | `0` | A positive value enables healthy, watered native plots in that area. |
| `RuneSchemaFarmHarvestMultiplier` | `1` | Multiplies the native harvest RPC's yield multiplier. |
| `RuneSchemaResourceYieldMultiplier` | `1` | Multiplies final native resource quantities after native rolls and bonuses. |
| `RuneSchemaResourceRespawnMultiplier` | `1` | Divides resource regeneration duration; for example, `4` gives one-quarter duration. Values below `1` are unsupported here. |
| `RuneSchemaPlayerCarryWeight` | `0` | Optional scalar metadata for cooked player effects; it does not itself set capacity. |
| `RuneSchemaPlayerVitality` | `0` | A positive value enables native needs refill and Well Rested maintenance for living players in range. |
| `RuneSchemaEnemySpawnMultiplier` | `1` | `0` suppresses newly spawned hostile AI; `2` requests one additional eligible enemy. Existing enemies remain. |

Scalar values must be finite and between `0` and `100000`; individual adapters apply narrower limits where needed. Matching scalar bonuses use the strongest value rather than multiplying together. A zero enemy-spawn result takes precedence over a doubled-spawn result. Native quantities and harvest multipliers have overflow checks.

## Spatial AI temperament — private candidate

This addition is included in the [complete candidate](PRE-RELEASE.md).
Implement the optional `RuneSchemaAIPassivity(Position: Vector) -> int32` callback
on the same version-1 provider actor. `Position` is the authoritative AI actor's
location in centimetres, including when native code supplies its controller or
component as context. The callback has exactly one input and one return value.

| Return | Policy |
| --- | --- |
| `0` | Inherit the current native world and AI behavior. |
| `1` | Friendly: request nonaggression and add RuneSchema's own native attack lock. |
| `2` | Angry: request aggression toward players while retaining native abilities. |

Friendly wins over Angry across overlapping providers, regardless of registration
order. Invalid values are reported for that provider; other valid providers still
compose. Return `0` outside the zone or while disabled. Native attackless wildlife
receives no invented attacks, equipment, abilities or combat tables.

The candidate wraps the native `DisableAggressiveAI` difficulty queries, the
AI-to-player attitude query and reflected `IsAiPassive` answers on the game
thread. Direct native calls that bypass reflection still require behavior
verification. It changes their answers only for
an identified authoritative AI in a world with this optional policy. It does not
change global difficulty, factions, AI data assets or class defaults. Every query
starts with the current native answer, so leaving a zone does not restore an old
world-setting snapshot. AI-to-AI attitudes and unrelated difficulty settings keep
their native results.

Friendly uses `DominionAICharacter.SetAttacksDisabled` with a distinct transient
RuneSchema-owned locker. Only that locker is released on exit, disable, provider
removal or cleanup; unrelated native/mod locks remain. Existing actors are found
once per participating world, and new/streamed actors enter through the existing
BeginPlay bridge. Transitions are checked on the bridge's existing 0.1-second
update. Actor transitions are summarized in the RuneSchema log.

An isolated headless Steam follow-up passed 16 actor-query and owned-lock checks
on real wolf/deer actors, including radius exit/reentry and unchanged native
AI data. Content registration and cooked provider callbacks passed separately.
These checks do not certify native targeting or queued-strike cancellation. Private acceptance must verify native binding/call contexts on Steam,
Game Pass and dedicated authority, normal/creative transitions, spawn/streaming,
world changes, other lockers, and an enemy with an existing target and queued
strike. In particular, the setter's declaration alone does not prove that an
already queued strike is cancelled. Unknown native bindings disable this adapter
and report `AI_PASSIVITY_BIND_FAILED`; other world policies remain available.

## Player effects

Provide both callbacks together:

- `RuneSchemaPlayerEffectCount() -> int32`: number of effect slots, from `0` to `64`.
- `RuneSchemaPlayerEffectData(Position: Vector, Index: int32) -> DominionGameplayEffectData class`: the cooked native gameplay-effect class for that slot, or null when inactive/outside range.

Cook capacity, maxima, recovery and other effect rules into the returned native effect classes. Configure their native stacking rules deliberately; the scalar strongest-value rule does not replace native gameplay-effect stacking behavior. Effects are tracked by provider and player and removed when that relationship ends, including exit, death and provider/world cleanup. Ending a vitality effect clamps excess special charge to the player's normal maximum. Well Rested decay is restored when its policy ends.

Server player effects run independently of the global building manager. Missing
building managers and failed damage, support or phasing adapters do not suppress
unrelated player buffs. Profile modifiers still compose using the game's native
effect rules; a cooked effect does not promise an exact final value after other
mods apply their modifiers.

### Player profiles and native bonuses — local follow-up

This follow-up is included in the [complete candidate](PRE-RELEASE.md); the checks below describe its coverage.
Health, stamina and carry-capacity player rules use their own native Add effects.
They leave equipment and world-policy effect handles alone. An unchanged profile
refresh keeps its existing handle; a changed rule replaces its own modifier.
Respawn and possession changes retire the previous pawn's owned state.

The baseline comes from the player's resolved default attribute table and the
world's native difficulty scale. It is captured once for that pawn, rather than
reading a maximum that may already include a totem or equipment bonus. The
current adapter accepts finite numeric `StartingValue` strings. An unfamiliar
expression or changed native contract is reported as unsupported instead of
being treated as an unbuffed value.

For example, with a base capacity of `400`, a profile capacity of `900` adds a
native `500` modifier. A cooked Override effect can still set capacity to `4000`.
Changing the profile to `1200` inside that zone leaves the native Override in
control; leaving restores the new profile capacity, plus any native equipment
bonuses. These interactions follow native stacking rules. Other player-rule
attribute families keep their existing implementation.

Creating or changing a profile examines only that player's bounded native effect
arrays. This adds no periodic search or global effect scan. Normal authority,
replication and world-policy checks still apply; use matching framework builds
on server and clients when testing the follow-up.

## Mod-owned native AI effects — private contract v1

This contract is in local test builds only. It extends the earlier spatial
adapter: native effect instances own the behavior, and a world provider only
selects which native effects to apply. It contains no mod-specific asset names.

| Owner | Member | Contract |
| --- | --- | --- |
| Effect data BP | `RuneSchemaAIPassivityMode()` | One `int32` return: `0` inherit, `1` Friendly, `2` Angry. No inputs or side effects. |
| Effect instance BP | `RuneSchemaAIPassivityApplied` | Instance bool, default `false`. The data BP's native added/removed events set it on the actual instance. Never store applied state on the data CDO. |
| Version-1 provider | `RuneSchemaAIEffectCount()` | One `int32` return, `0..64` stable slots. |
| Provider | `RuneSchemaAIEffectData(Position, Index)` | Native GE class or `None`; `Position` is the AI's `Vector`, `Index` is `int32`. |
| Provider | `RuneSchemaAIEffectInstigator(Position, Index)` | Actual live actor in the AI's world, or `None`. |
| Provider | `RuneSchemaAIEffectSource(Position, Index)` | Actual source object or `None`; a world-bound source must belong to the AI's world. |

The four provider callbacks form one optional contract. An environmental
coordinator may explicitly return itself as caster/source. RuneSchema preserves
null values and never substitutes the target AI. Standalone native applications
keep the caster/source supplied by the spell or mod.

Only authoritative, living AI are affected. A recognized instance must belong
to that AI's native component and have a valid native handle. Never-applied,
blocked and removed instances contribute no mode. The applied marker must also
default to false on the instance CDO. This first version supports immediate main
effect activation with no `ActivationCurrentValueAttribute`; conditional main
activation is reported as unsupported. Friendly wins across active instances.

Spatially selected GEs must be removable and non-single-instance. RuneSchema
uses native forced-new handles and removes only the handles it created for that
provider, AI and slot. Class/caster/source changes retire that slot's old handle.
Direct spell handles remain owned by their original caller. Player effects retain
their existing arguments, vitality, charge and rest handling.

Native data add/remove events request deferred standalone discovery without a
spatial provider. A one-time scan catches pre-existing instances; BeginPlay and
provider discovery cover later actors. Event hints are coalesced on the existing
policy tick. Selection runs at most twice per second. Mode callbacks and native
authority/health checks are sampled on that tick, outside native getter and GE
lifecycle callbacks. Native queries read sampled mode values with fresh native
membership, applied, blocked and removed guards. New activation can wait for the
next policy tick; disabled or removed instances cannot retain a cached mode.
Mode publication follows native attack-lock reconciliation and readback. Each
active Friendly instance owns its native attack lock. Replacement owners acquire
their locks before retired owners release theirs; blocking, removal and shutdown
release only those recorded owners. Other callers' lockers remain untouched.

New GE-owning providers should remove the legacy `RuneSchemaAIPassivity` callback
or return `0` from it, so the old spatial answer cannot outlive the GE. Existing
legacy providers remain compatible. The mod owns its data/instance BPs and their
native lifecycle; RuneSchema supplies the missing generic native-query bridge.

Core compilation and activation/overlap model checks passed. Native lifecycle
checks of the cooked assets are separate from acceptance of this new adapter.
Combat, queued-strike cancellation, creative-mode and cross-platform/dedicated
behavior remain unverified. No production installation is implied.

## Native behavior and work limits

The native crafting resolver handles Game Pass thunks whose prologue, body and
epilogue occupy separate chained Windows unwind records. It follows only
adjacent ranges with the exact same primary unwind identity, within fixed
byte/range limits. Steam's complete function ranges retain their boundaries.
Unknown metadata still disables only the affected adapter and reports
`CRAFTING_MENU_BIND_FAILED` with its function name. Fresh isolated Steam checks
passed all eight native menu groups on this follow-up; a live Game Pass menu
check after restarting with the rebuilt DLL remains necessary.

The follow-up also accepts indirect bytecode/parameter helper calls inside the
WinGDK exec wrapper. Those calls cannot become detour targets: the resolver still
requires a direct call whose unchanged AL/EAX result is stored through the exec
result pointer, followed by an exact executable/unwind-boundary check. Unknown,
rewritten or ambiguous result producers remain unsupported. The full captured
Game Pass CanCraftRecipe, maximum-quantity and required-items wrappers pass the
decoder and native-target boundary checks. Live acceptance after restart is
still required. Configure `RUNESCHEMA_ZYDIS_SOURCE_DIR` with the project's pinned
Zydis checkout to include the native decoder regression in standalone core tests;
its compiler output remains in the selected CMake binary directory.

Farming protection uses private transient plant/stage copies rather than editing the source assets. Harvest parameters are restored after the borrowed native call. Resource durations are restored when their policy ends. Bonus enemies use transient, non-persistent actors and a recursion guard.

Farm and resource detours install only in authoritative worlds. They validate the reflected layout and decoded executable path before binding; unsupported executables retain native handling and report the original failure. This does not certify Game Pass or a later game update without a corresponding runtime test.

The October 29 WinGDK capture identified a stability worker reached by a tail
jump, a farm-update call six bytes beyond the previous 768-byte search, and a
different subsystem register in the watering message. The maintained resolver
accepts those verified variants within fixed search limits, retaining signature,
layout, ambiguity and output bounds checks. Captured-code checks and a successful
build establish resolution compatibility; they do not replace a live Game Pass
world test of farming and stability effects.

An unsupported support adapter is suspended for that building manager; it does
not repeatedly scan native code or abort the remaining policy tick. A new
manager retries normally. Failed farm-protection binding is attempted once per
world even if several providers request it. `SUPPORT_UPDATE_FAILED` and
`FARM_BIND_FAILED` retain the original reason and identify the unavailable
override. Native stability and farming continue; these messages do not mean
that the optional override has been made compatible with that executable.

Provider discovery uses BeginPlay events and a one-time catch-up scan. Unrelated ProcessEvent calls pass a cached function-name filter before path checks. Player effects are sampled at most every half second, and farm maintenance at most once per second when its native adapter is installed. Resource enumeration is skipped when no matching policy or outstanding restoration exists. These are work limits, not measured FPS or latency guarantees.

## Multiplayer building identity

Buildings receive compact native network IDs. Sorting the added definitions by PersistenceID is deterministic only when both sides contain the same definitions. Extra entries can shift later IDs onto completely different buildings.

The candidate exchanges a bounded building-registry fingerprint through the existing owned RuneSchema compatibility RPC. Placement checks use the exact ordered identities and native prefix. Matching registries can place their registered pieces. A verified matching native prefix permits native pieces even when the modded suffix differs. Unverified or different modded IDs are blocked before native spawning, with [RS-NET-003](../ERROR-CODES.md#rs-net-003).

This guard does not copy a server's building list, remap IDs, remove client-only mods or reject connection entry. Required building definitions still need to match for those modded placements. Enable the current RuneSchema networking plugin on both client and server. A client/server without the new identity exchange cannot certify placement; retry after the handshake or align the required packages.

For a policy validation/hook failure, keep the complete [RS-WLD-001](../ERROR-CODES.md#rs-wld-001) message and the executable/framework version. Do not bypass signature checks or distribute private test drivers with a mod.
