# Spawns loader

**Folder:** `RuneSchema/mods/<ModName>/spawns/`

AI, actors, resources, building props, and event-only templates.

[← Loader reference](../../LOADER-WALKTHROUGHS.md)

Spawns place AI, actors, resources, and supported building props.

```json
[
  {
    "Id": "night_guard",
    "Type": "AISpawnPoint",
    "AIClass": "/Game/Gameplay/AI/BP_Guard.BP_Guard_C",
    "Location": [1000, 2000, "$+10"],
    "PowerLevel": 30,
    "TimeOfDay": "Night",
    "SpawnRadiusMeters": 100
  }
]
```

Use `GroundOffset:10` or `$+10` for ten centimeters above traced ground, not
both. `AdditionalDrops` supports an item, minimum, maximum, and chance. A
quest-completed condition can latch with `PersistAfterCondition`. `EventOnly`
templates are definitions for `/events` and do not place permanent actors.

## Temporary ore and tree nodes

Use an ordinary `Actor` resource entry with **one** lifecycle condition:
`QuestActive` or `EventActive`. These fields name a RuneSchema quest or event
ID in the same mod; use `OtherMod:Id` for a cross-mod reference. The resource
class must be a cooked subclass of the game's ore-node or fellable-tree base,
including a mod-owned subclass, with `SpudGuid` and
`bSkipSpudStore` fields. It is spawned only by the world authority, excluded
from the world save, and removed when the condition stops being active.

```json
[
  {
    "Id": "trial_iron",
    "Type": "Actor",
    "Class": "/Game/Gameplay/World/Mining/Ores/BP_OreNode_Iron.BP_OreNode_Iron_C",
    "Location": [1000, 2000, "$+10"],
    "QuestActive": "iron_trial"
  }
]
```

For an event, replace `QuestActive` with `EventActive` and name the event ID.
RuneSchema does not recreate a harvested node during that same event or quest;
any respawn behavior authored inside the cooked resource Blueprint remains its
own responsibility. A later event run can create a fresh node. Temporary resource
entries cannot also use `QuestCompleted`, `TimeOfDay`, or `UseNativeRespawn`.
They are not persistent world placements; do not use them for a resource you
want to remain after the event or quest.

## Simple rules

- `/spawns` places supported AI, actors, resources, and building props.
- Use either `GroundOffset` or a ground-relative Z value such as `$+10`, not both.
- `EventOnly:true` creates an event template rather than a permanent placement.

## FAQ

### FAQ-SPAWNS-001 — Does EventOnly place an actor in the world immediately? {#faq-spawns-001}

No. It defines a spawn template for `/events`; it does not create a permanent
world placement by itself.

### FAQ-SPAWNS-002 — Can I use GroundOffset and $+10 together? {#faq-spawns-002}

No. Pick one ground-offset form for the placement.

### FAQ-SPAWNS-003 — Can a spawn add extra item drops? {#faq-spawns-003}

Yes. `AdditionalDrops` supports an item plus minimum, maximum, and chance.

## Working examples

- [Great Tree: event-only goblin templates](../../examples/GreatTree/spawns/87-GreatTree-TribeTrial.json)

---

[← All loaders](../../LOADER-WALKTHROUGHS.md) · [Authoring guide](../../AUTHORING-GUIDE.md)
