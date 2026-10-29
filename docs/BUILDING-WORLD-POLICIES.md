# Cooked world policies

This page describes the maintained source candidate after 0.8.0.3. It is not a claim that an older installed DLL supports these additions.

A cooked Blueprint actor can provide world policies to RuneSchema. Keep the mod's definitions, range calculations, artwork and interface in its PAK. RuneSchema validates the callback signatures and connects them to the game's native behavior. These are optional callbacks on a live actor in the affected world; they do not need a separate mod DLL or a shipped Lua poller.

## Provider contract

Implement `RuneSchemaBuildingPolicyVersion() -> int32` and return `1`. Existing building support, placement, damage, destruction and overlay callbacks keep their contracts.

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

## Player effects

Provide both callbacks together:

- `RuneSchemaPlayerEffectCount() -> int32`: number of effect slots, from `0` to `64`.
- `RuneSchemaPlayerEffectData(Position: Vector, Index: int32) -> DominionGameplayEffectData class`: the cooked native gameplay-effect class for that slot, or null when inactive/outside range.

Cook capacity, maxima, recovery and other effect rules into the returned native effect classes. Configure their native stacking rules deliberately; the scalar strongest-value rule does not replace native gameplay-effect stacking behavior. Effects are tracked by provider and player and removed when that relationship ends, including exit, death and provider/world cleanup. Ending a vitality effect clamps excess special charge to the player's normal maximum. Well Rested decay is restored when its policy ends.

## Native behavior and work limits

Farming protection uses private transient plant/stage copies rather than editing the source assets. Harvest parameters are restored after the borrowed native call. Resource durations are restored when their policy ends. Bonus enemies use transient, non-persistent actors and a recursion guard.

Farm and resource detours install only in authoritative worlds. They validate the reflected layout and decoded executable path before binding; unsupported executables retain native handling and report the original failure. This does not certify Game Pass or a later game update without a corresponding runtime test.

Provider discovery uses BeginPlay events and a one-time catch-up scan. Unrelated ProcessEvent calls pass a cached function-name filter before path checks. Player effects are sampled at most every half second, and farm maintenance at most once per second when its native adapter is installed. Resource enumeration is skipped when no matching policy or outstanding restoration exists. These are work limits, not measured FPS or latency guarantees.

## Multiplayer building identity

Buildings receive compact native network IDs. Sorting the added definitions by PersistenceID is deterministic only when both sides contain the same definitions. Extra entries can shift later IDs onto completely different buildings.

The candidate exchanges a bounded building-registry fingerprint through the existing owned RuneSchema compatibility RPC. Placement checks use the exact ordered identities and native prefix. Matching registries can place their registered pieces. A verified matching native prefix permits native pieces even when the modded suffix differs. Unverified or different modded IDs are blocked before native spawning, with [RS-NET-003](ERROR-CODES.md#rs-net-003).

This guard does not copy a server's building list, remap IDs, remove client-only mods or reject connection entry. Required building definitions still need to match for those modded placements. Enable the current RuneSchema networking plugin on both client and server. A client/server without the new identity exchange cannot certify placement; retry after the handshake or align the required packages.

For a policy validation/hook failure, keep the complete [RS-WLD-001](ERROR-CODES.md#rs-wld-001) message and the executable/framework version. Do not bypass signature checks or distribute private test drivers with a mod.
