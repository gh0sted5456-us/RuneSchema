# Full `/players` authoring guide

The active `players/profiles.json` demonstrates four connection-order roles. The
two `.disabled` files document arbitrary attribute and appearance syntax without
loading placeholder FModel row/attribute names into a real game.

## Selectors

| Field | Type | Meaning |
| --- | --- | --- |
| `PlayerName` | string | One name, `*`, or `*N`. |
| `PlayerNames` | string array | Any listed name/slot selector. |
| `PlayerGuid` | string | One stable character GUID. |
| `PlayerGuids` | string array | Any listed GUID. |

`*` selects everyone. `*1`, `*2`, etc. select stable first-seen connection slots
for the current world session. GUID selectors are authoritative when present;
names alongside GUIDs are descriptive rather than an additional OR match.

## Complete dedicated field reference

| Field | Range | Effect |
| --- | ---: | --- |
| `Scale` | 0.25–3 | Multiplies original pawn scale. |
| `HealthMultiplier` | 0.1–100 | Multiplies baseline maximum health. |
| `MaxHealth` | 1–1,000,000 | Sets absolute maximum health. |
| `BaseHealth` | 1–1,000,000 | Alias for `MaxHealth`. |
| `DefenseMultiplier` | 0.1–100 | General defense multiplier. |
| `DamageMultiplier` | 0.1–100 | General outgoing damage multiplier. |
| `StaminaMultiplier` | 0.1–100 | Multiplies baseline maximum stamina. |
| `MaxStamina` | 1–1,000,000 | Sets absolute maximum stamina. |
| `WalkSpeedMultiplier` | 0.1–10 | Walking speed multiplier. |
| `RunSpeedMultiplier` | 0.1–10 | Running/sprint speed multiplier. |
| `CarryWeightMultiplier` | 0.1–100 | Multiplies baseline carry capacity. |
| `MaxCarryWeight` | 1–1,000,000 | Sets absolute carry capacity. |
| `PoisonResistanceMultiplier` | 0–100 | Poison-resistance multiplier. |
| `StaminaRecoveryMultiplier` | 0–100 | Stamina regeneration multiplier. |
| `PhysicalAttackMultiplier` | 0–100 | Physical attack multiplier. |
| `MagicalAttackMultiplier` | 0–100 | Magical attack multiplier. |
| `MagicAttackMultiplier` | 0–100 | Alias for `MagicalAttackMultiplier`. |
| `RangedAttackMultiplier` | 0–100 | Ranged attack multiplier. |
| `RangeAttackMultiplier` | 0–100 | Alias for `RangedAttackMultiplier`. |
| `PhysicalDefenseMultiplier` | 0–100 | Physical defense multiplier. |
| `MagicalDefenseMultiplier` | 0–100 | Magical defense multiplier. |
| `MagicDefenseMultiplier` | 0–100 | Alias for `MagicalDefenseMultiplier`. |
| `RangedDefenseMultiplier` | 0–100 | Ranged defense multiplier. |
| `RangeDefenseMultiplier` | 0–100 | Alias for `RangedDefenseMultiplier`. |

Do not combine `HealthMultiplier` with `MaxHealth`/`BaseHealth`,
`StaminaMultiplier` with `MaxStamina`, or `CarryWeightMultiplier` with
`MaxCarryWeight` in the same rule. Separate matching rules may be deliberately
layered in deterministic mod/file/array order.

## Arbitrary attributes

`AttributeMultipliers` maps an FModel/SDK attribute asset or runtime class name to
a multiplier from 0–100:

```json
{"AttributeMultipliers":{"DA_Attribute_ExactName":1.25}}
```

`Attributes` maps an exact attribute identifier to one operation:

```json
{
  "Attributes": {
    "DA_Attribute_A": {"Set":100},
    "DA_Attribute_B": {"Add":20},
    "DA_Attribute_C": {"Multiply":1.5}
  }
}
```

Each entry must contain exactly one of `Set`, `Add`, or `Multiply`. `Multiply` is
0–100; `Set` and `Add` are -1,000,000–1,000,000. Identifiers are normalized for
paths, quotes, `DA_Attribute_`, `_C`, `U...Attribute`, and `MaxCarryWeight` naming.
Use FModel to replace the placeholders in `advanced-attributes.json.disabled`.

## Appearance

Supported fields and aliases:

| Canonical field | Accepted aliases | Vanilla DataTable |
| --- | --- | --- |
| `BodyType` | — | `DT_Customization_BodyType` |
| `FaceType` | `Head` | `DT_Customization_FaceType` |
| `HairPreset` | `HairStyle` | `DT_Customization_HairPresets` |
| `FacialHairPreset` | `BeardStyle` | `DT_Customization_FacialHairPresets` |
| `SkinTone` | `SkinColor` | `DT_Customization_SkinTone` |
| `HairColor` | — | `DT_Customization_HairColor` |
| `EyeColor` | — | `DT_Customization_EyeColor` |
| `EyebrowColor` | — | `DT_Customization_EyebrowColor` |

A value may be a vanilla row-name string:

```json
{"Appearance":{"HairStyle":"ExactHairPresetRow"}}
```

or an object with `Name`, `Row`, or `RowName`, plus optional `DataTable`:

```json
{"Appearance":{"HairColor":{"DataTable":"/Game/Path/DT.DT","RowName":"Blue"}}}
```

Appearance packs may publish `appearance/manifest.json` with `Tables` and optional
`FallbackRows`. A selection can then use `Source` and must have a safe vanilla
fallback either in the manifest or inline:

```json
{"Appearance":{"HairStyle":{"Source":"EnabledAppearancePack","Row":"CustomHair","Fallback":"VanillaHair"}}}
```

Fallback provenance is stored in
`RuneSchema/player-data/appearance-fallbacks.json`, allowing RuneSchema to restore
a vanilla row if the owning appearance mod is later disabled. Sources from all
enabled mods are registered before `/players` parsing, independent of load order.

## Use cases

- Party roles via `*1`–`*4` (the active example).
- Server-wide accessibility or difficulty rules via `*`.
- Persistent character-specific profiles via GUID.
- Temporary named-player event buffs.
- Class-like builds combining movement, carrying, recovery, attack, and defense.
- Total conversions using exact attribute operations and appearance source packs.

Rules re-evaluate once per second and apply from captured baselines, preventing
multiplier drift. New joiners and replacement pawns after respawn are handled.
`mods.txt`, sorted filenames, and array order make overlapping rules deterministic.

## Testing

Verify each field independently before combining it; then test join order,
disconnect/reconnect, death/respawn, UI refresh, save/reload, appearance fallback,
and multiple matching mods. Failed targets are logged without crashing the loader.

Credits: Snorkles and Jonesing4Space.
