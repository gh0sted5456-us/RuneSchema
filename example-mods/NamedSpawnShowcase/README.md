# Complete `/spawns` field guide

The included `AISpawnPoint` uses the native spectral-zombie class and the custom
per-instance `DisplayName` `The White-Road Revenant`. It does not mutate the shared
AI class default.

## Shared fields

| Field | Meaning |
| --- | --- |
| `Type` | Required: `AISpawnPoint`, `Actor`, or `RemoveActor`. |
| `Location` | Required `{X,Y,Z}` world position. |
| `Rotation` | Optional `{Pitch,Yaw,Roll}`. |
| `Scale` | Positive number or `{X,Y,Z}`. |
| `DisplayName` | 1–128 character per-instance AI name. |
| `LootRow` | Native `DT_EnemyLootDropTable` row selected for this AI instance. |
| `DropIncreasePercent` | Nonnegative opt-in drop-row percentage. |
| `HealthMultiplier` | `0.1`–`100`; scales initialized maximum and current health per spawned AI. |
| `DamageMultiplier` | `0.1`–`100`; scales supported reflected attack/damage component multipliers. |

## `AISpawnPoint`

`AIClass` is required. `PowerLevel` is the native spawn-point level field (the
same property name used by `/assets`). Supported convenience fields are `PowerLevel`, `Mandatory`,
`Respawn`, `RespawnDuration`, `AmbientBehaviour`, `DespawnBehaviour`,
`RequiresActivation`, `IgnoreNavmeshRequirement`, `RoamRadius`,
`RoamMaxZTolerance`, `RoamGoalQueryType`, `SentryRadius`, `HearingRange`,
`HearingZRange`, `IdleAnimTag`, `Tags`, `MinSpawnDistance`, `MaxSpawnDistance`,
`Variants`, and advanced `Properties`.

Three reflected-property scopes are available:

- `Properties` writes exact fields on the `AISpawnPoint` itself.
- `CharacterProperties` writes exact fields on the newly spawned AI character.
- `ComponentProperties` maps a reflected character component name to an object of
  exact fields on that component. This is the intended bridge for curve, health,
  attack, attribute, and other cooked fields discovered in FModel.

For example, a verified FModel export can be translated as:

```json
"CharacterProperties": {
  "VerifiedCharacterField": 1.25
},
"ComponentProperties": {
  "VerifiedComponentPropertyName": {
    "VerifiedCurveOrCombatField": "/Game/Verified/Curve.Curve"
  }
}
```

Names and JSON shapes must match the cooked class exactly. RuneSchema logs missing
properties/components instead of silently redirecting them. Prefer the stable
`HealthMultiplier` and `DamageMultiplier` fields for ordinary combat scaling; use
the reflected maps when a particular AI class exposes a more specific FModel field.

Each variant requires `AIClass`. Progress gating requires both `WorldProgressTag`
and `WorldProgressValue`. `Properties` maps exact reflected spawn-point property
names to correctly shaped JSON values.

## `Actor`

Requires unique `Id` and full `Class`; accepts shared transform/drop fields and
advanced `Properties`. The mod identity plus `Id` produces a stable actor GUID so
world streaming finds the managed actor rather than duplicating it.

## `RemoveActor`

Requires `Class`; accepts a positive `Radius` (default 500) around `Location`.
Keep removal classes and radii narrow.

## Use cases

- Named miniboss: `DisplayName`, scale, mandatory spawn, respawn timing.
- Named character with additional drops: author matching `/raw` rows in
  `DT_LootDropTable` and `DT_EnemyLootDropTable`, then select them with `LootRow`.
- Ambient population: distance gates, roam/perception fields, tags, variants.
- Event loot target: explicit drop percentage with the config switch enabled.
- Stable prop placement: `Actor` plus unique ID and reflected properties.
- Scoped world cleanup: `RemoveActor` around verified coordinates.

Replace the demonstration coordinates with a safe location. Test streaming,
respawn, overhead name refresh, duplicate prevention, navigation, and save reload.

The included `RS_WhiteRoadRevenant` loot route retains a guaranteed native zombie
pack and adds a 35% chance for one or two Goblin Packs for players who inflicted
damage. `LootRow` selects native DataTable content; it does not maintain a private
RuneSchema drop list. This keeps loot editable and composable through `/raw`.

Credits: Snorkles and Jonesing4Space.
