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
