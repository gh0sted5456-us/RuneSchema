# Registry loader and network bridge

`/registry` gives a stable mod-owned key to cooked presentations and optional
server-authoritative actions. The server remains authoritative; registry JSON
does not make an arbitrary client asset executable.

## Compact author format

The file may be a direct map from local ID to entry. `SchemaVersion`, `Entries`
and repeated `Id` fields are not required:

```jsonc
{
  "windstep_cast": {
    "Kind": "UtilitySpellPresentation",
    "Spell": "/Game/Gameplay/UtilityMagic/PerkSpells/Windstep/USD_Windstep.USD_Windstep",
    "Presentation": [{
      "Phase": "Cast",
      "Class": "/MyMod/FX/BP_WindstepCast.BP_WindstepCast_C",
      "Classification": "PureVFX"
    }]
  }
}
```

The mod folder name supplies ownership. The normalized network key becomes
`ModFolder:windstep_cast`. Existing single-entry, bare-array and
`{SchemaVersion, Entries}` documents remain accepted.

Supported descriptive kinds are `SpellPresentation`,
`UtilitySpellPresentation`, `SkillPresentation`,
`GameplayEffectPresentation`, `EquipmentPresentation`, `PersistentEffect`,
`WeatherPresentation`, `WorldPresentation`, `AudioPresentation`, and
`CosmeticWrapper`.

## Replication rules

- The authority publishes a bounded registry fingerprint and revision.
- A mismatch degrades individual actions; it does not unload RuneSchema.
- Clients resolve presentation keys against their own mounted content.
- Gameplay requests are rate-limited and revalidated on the authority.
- Cosmetic data never acquires gameplay authority from JSON.
- Missing cooked bridge assets disable networking only; local loaders remain
  independent.

Cooked discovery recognizes `DA_RuneSchemaRegistry*` and `RSREG_*` assets with
a bounded `RuneSchemaRegistryJson`/`RegistryJson` string and an explicit owner
when the package mount cannot provide one.

