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

## Working examples

- [Great Tree: event-only goblin templates](../examples/GreatTree/spawns/87-GreatTree-TribeTrial.json)

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
