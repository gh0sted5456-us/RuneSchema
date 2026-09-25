# Startup performance

RuneSchema separates its own loader work from UE4SS and game lifecycle waits.
This distinction matters when reading timestamps: a long gap between two
RuneSchema messages does not necessarily mean RuneSchema was executing during
that interval.

## September 25, 2026 baseline

The 0.7.5.27 universal runtime was profiled on both Steam/GOG and Game
Pass/WinGDK with the same active RuneSchema mod set. These measurements are a
diagnostic baseline, not guaranteed startup times for every computer.

| Stage | Steam/GOG | Game Pass/WinGDK | Owner |
| --- | ---: | ---: | --- |
| Journal finalization after optimization | about 306 ms | about 410 ms | RuneSchema |
| Recipe reference preparation | about 6 ms | about 2.91 s | RuneSchema |
| UE4SS object construction | about 1.4–3.7 s observed | about 1.4–3.7 s observed | UE4SS/game |
| Hash-table and garbage-collection readiness | about 2.8–3.0 s observed | about 2.8–3.0 s observed | UE4SS/game |
| Store-specific signature scan | not material in this run | about 0.9 s | RuneSchema native-binding lane |
| Front-end viewport readiness before Helpy attaches | about 3.5–4.9 s observed | about 3.5–4.9 s observed | Game lifecycle |

Plugin discovery, storefront selection, settings loading, USMAP fingerprinting,
and ownership-ledger initialization were not meaningful startup bottlenecks in
this profile. A USMAP is fingerprinted at startup but is parsed only when a
mapping-service query explicitly needs it.

## Journal optimization

Journal finalization previously performed repeated full Unreal object scans
for every journal entry. On Game Pass this took approximately 8.65 seconds.
RuneSchema now builds one per-class name index for recipe, item, and DataTable
soft references, reuses resolved objects, and caches the journal subsystem and
subcategory objects during the finalization pass.

The optimized run completed journal finalization in approximately 410 ms on
Game Pass and 306 ms on Steam while retaining all 148 valid journal entries and
148 placements. The same three invalid Currency references remained isolated
and reported; unrelated entries continued loading.

## Remaining RuneSchema hotspot

Recipe reference preparation remains disproportionately expensive on Game
Pass. The current compatibility path resolves approximately 258 recipe
definitions through repeated Unreal class-object lookups. Steam's hash-backed
lane makes the same work effectively negligible, while the measured Game Pass
run spent about 2.91 seconds there.

An initial shared name-index experiment was not retained because its runtime
build did not pass the required Game Pass startup stability check. The verified
journal optimization remains in place. Future recipe optimization must preserve
the two storefront lanes, late object availability, automatic reload behavior,
and per-entry failure isolation, then pass clean-start and world-entry tests on
both storefronts before release.

## Reading apparent pauses

These normal waits can appear between RuneSchema messages:

- UE4SS constructs the UObject array and validates native reflection access.
- Unreal creates hash tables and completes early garbage collection.
- The game creates or replaces the local player controller.
- The front-end viewport becomes available for Helpy's renderer.
- Game Pass resolves its lane-specific native bindings.

RuneSchema must wait for the corresponding Unreal object or lifecycle event
before touching it. Removing those guards can trade a few seconds for startup
crashes, invalid pointers, or loader work applied to objects that the game later
replaces.

## Profiling world entry and SafeClean

Main-menu profiling does not prove the cost of world-specific SafeClean work.
For that test, use the same save and mod set on both storefronts, enable
advanced diagnostics, enter the same world, and compare loader phase markers
rather than total wall-clock load time. Record separately:

1. ownership snapshot read and comparison;
2. hydrated save cleanup and readback;
3. world-loader finalization;
4. the game's own map load and replication waits.

SafeClean remains owned-content-only. Performance work must not broaden it into
a whole-save scan or infer that unknown vanilla or third-party identities are
safe to remove.
