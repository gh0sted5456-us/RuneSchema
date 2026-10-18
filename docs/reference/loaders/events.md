# Events loader

**Folder:** `RuneSchema/mods/<ModName>/events/`

Server-owned timed encounters and waves driven by spawn templates.

[← Loader reference](../../LOADER-WALKTHROUGHS.md)

Events run server-owned waves from `/spawns` entries marked `EventOnly:true`.

```json
[
  {
    "Id": "night_attack",
    "TimeOfDay": "Night",
    "TimeoutSeconds": 900,
    "Waves": [[
      {"SpawnID":"night_raider","Location":[1000,2000,"$+10"]}
    ]]
  }
]
```

Create the spawn template first, then start or cancel the event from dialogue.
`$`, `$+offset`, and `$-offset` resolve Z against blocking ground. Event IDs can
scope quest kill credit.

## Boss proof items and scaled grounding (alpha)

An `EventOnly:true`, `Type:"AISpawnPoint"` template accepts the same
`AdditionalDrops` array as ordinary AI spawns. A guaranteed shared proof item is:

```json
"AdditionalDrops": [
  {"Item":"/YourMod/Items/ITEM_Proof.ITEM_Proof","Min":1,"Max":1,"ChancePercent":100}
]
```

This appends an instance-owned bonus row to the enemy's native loot selection;
it does not replace vanilla drops, manually grant on damage, or award one item
per player. Only authority configures loot. Event actors require an owned native
AI LootDrop component; the destruction-resource fallback is forbidden. Failure
to bind/read back the bonus row fails the spawn rather than silently losing the
proof. Cancellation, cleanup and non-kill despawn restore the original loot handle
before retirement. Native death-loot behavior and pickup still require an in-game
test; parser tests alone cannot prove a physical drop occurred.

Grounded encounter waves preflight the actual root capsule's scaled radius and
half-height against five ground samples before constructing any wave actors.
The existing 10 cm foot clearance and author GroundOffset are retained. The
capsule, not the mesh pivot, determines actor-origin Z. Missing collision waits;
cliffs or changed construction-time capsule dimensions fail safely. Ground misses
cannot retry an already partially constructed wave.

For proof-item quests, use a mandatory Collect/TurnIn objective and normal
inventory-validated NPC turn-in, not a preceding sequence of kill objectives.
A changed active definition does not inherit fabricated kills or proof items:
the existing authoritative reset is scoped to that quest and requires re-accept.
Completed receipts/cooldowns are preserved, and unresolved acceptance/turn-in
transactions block migration rather than being cleared or paid again. No broad
character-quest reset is needed or permitted by this feature.

## Quest-bound bounties and survival encounters (development)

This optional contract requires a framework build containing quest-bound
encounters; the installed r6 binary does not gain it from content JSON alone.
Existing events without `Contract` keep their current behavior.

```json
{
  "Id": "milestone_trial",
  "Scope": "Participant",
  "TimeoutSeconds": 900,
  "Area": {"Location":[10000,20000,300],"RadiusMeters":65,"Visibility":"Auto"},
  "Contract": {
    "QuestID": "hold_milestone",
    "SpawnOrigin": "Participant",
    "ActivationRadiusMeters": 15,
    "ExitGraceSeconds": 10
  },
  "Waves": [[{"SpawnID":"final_champion","Location":[2200,0,"$"]}]]
}
```

`QuestID` resolves within the supplying mod, or accepts a qualified `Mod:Id`.
It must identify a single-objective `Kill` quest with `Count:1`, this `EventID`,
and a `SpawnID` appearing exactly once in the final wave. One quest belongs to
one contract event. Staged quests are not supported by this contract mode.

Only authority can activate the event. The participant must have accepted its
quest and must be within the activation radius of the authored area center.
Acceptance has a monotonic attempt identity; abandoning and re-accepting cannot
adopt an encounter from the previous acceptance, even when its run number repeats.

`SpawnOrigin` defaults to `World`. `Participant` interprets member X/Y values as
centimeter offsets from the participant at activation, held fixed for all waves.
These offsets must be 1–100 meters away and use grounded `$` Z forms. Every
resolved member must remain within the authored area. Normal collision and
capsule-grounding preflight still applies; this is not a navigation guarantee.

The activation radius is 1–100 meters, defaults to 15, and must fit inside `Area`.
The exit grace is 0–60 seconds, defaults to 10, and resets when the participant
returns to the area. The area uses a three-dimensional distance check. A zero
grace fails on the first boundary check outside. Contract checks run at the
encounter's existing quarter-second cadence.

Individual fatal hits do not advance a contract quest. Only the complete set of
waves, a still-living participant and the same accepted attempt can record its
single victory objective. Rewards continue through ordinary quest transactions;
`Completion:"Automatic"` uses that existing reward path. Death, abandonment,
timeout, cancellation, missing actors or leaving beyond the grace record no
victory. A failed attempt can be retried at its activation interaction with fresh
actors and no partial victory counters. A completed victory persists with normal
quest progress and cannot start another encounter before the reward is claimed.

The mode currently supports `Scope:"Participant"` and cannot use
`DespawnTimeOfDay`. This credits the player who activated the encounter; the
scope does not create a private instance or grant every assisting player credit.
The existing one-active-encounter runtime limit remains in effect. The contract
fields participate in server/client manifest matching.

Map-derived coordinates are candidate placements. Verify terrain, navigation,
existing player buildings, native enemies, death on the final kill, cancellation,
reconnect/reload and remote-client rewards in game before release.

## Simple rules

- Events run server-owned waves from `/spawns` definitions marked `EventOnly:true`.
- Create the spawn template before the event that uses it.
- Start or cancel events through supported dialogue actions; ground-relative Z forms resolve against blocking ground.

## FAQ

### FAQ-EVENTS-001 — Do EventOnly spawn templates place permanent actors by themselves? {#faq-events-001}

No. `EventOnly:true` marks a spawn definition as a template for `/events`;
it is not a permanent world placement.

### FAQ-EVENTS-002 — Can an event be used to scope quest kill credit? {#faq-events-002}

Yes. Event IDs can be used to scope kill credit for quest objectives.

## Working examples

- [Great Tree: five-wave tribe trial](../../examples/GreatTree/events/85-GreatTree-TribeTrial.json)

---

[← All loaders](../../LOADER-WALKTHROUGHS.md) · [Authoring guide](../../AUTHORING-GUIDE.md)
