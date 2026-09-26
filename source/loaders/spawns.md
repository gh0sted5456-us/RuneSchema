# Spawns loader

**Folder:** `RuneSchema/mods/<ModName>/spawns/`

AI, actors, resources, building props, and event-only templates.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

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

### FAQ-SPAWNS-004 — Can a spawn be limited by time of day? {#faq-spawns-004}

Yes. `TimeOfDay` is part of the documented spawn definition shape.

### FAQ-SPAWNS-005 — Can a quest-completed condition stay latched? {#faq-spawns-005}

Yes. Use `PersistAfterCondition` where the documented quest-completed
condition should remain active after it first becomes true.

### FAQ-SPAWNS-006 — Can I use a fixed Z coordinate instead of ground-relative Z? {#faq-spawns-006}

Yes. A normal numeric Z value is allowed. Use `# Spawns loader

**Folder:** `RuneSchema/mods/<ModName>/spawns/`

AI, actors, resources, building props, and event-only templates.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

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

, `$+offset`, or
`$-offset` only when you want the loader to resolve height from blocking
ground.

## Working examples

- [Great Tree: event-only goblin templates](../examples/GreatTree/spawns/87-GreatTree-TribeTrial.json)

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
