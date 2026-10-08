# Optional combat fallback settings

RuneSchema 0.7.7.4m can turn its optional weapon-registration helpers on or off
without changing item, recipe, quest, spell-persistence, or save-cleanup
registries.

Edit `settings/settings.jsonc` with the game closed, or use **Settings > Runtime
> Optional combat fallback**, then restart.

```json
"combatFallback": {
  "enabled": false,
  "additionalWeapons": true,
  "manifestMelee": true,
  "rangedEquipment": true,
  "initialWorldMutation": false
}
```

- `enabled` is the master switch for all optional combat fallback helpers. It
  is disabled by default because normal item, recipe, spell, and save support
  does not require component mutation.
- `additionalWeapons` controls RuneSchema's built-in ordered spear and hoplite
  attack bridge.
- `manifestMelee` controls melee attack collections declared by cooked registry
  bridge assets.
- `rangedEquipment` controls complete quick/full ranged attack pairs discovered
  through registered item equipment data.
- `initialWorldMutation` controls only the compatibility path that may update
  combat components which already exist in the first gameplay world. When
  enabled, RuneSchema makes one guarded attempt and never retries on later
  callbacks or worlds.

Leave both safety switches `false` for normal play. Mod authors testing the
bridge can enable the master switch while leaving `initialWorldMutation` off;
that permits startup component-default registration without changing live
components. Enable live mutation only for a controlled compatibility test.

These settings do not change mounted-PAK discovery, item registration,
PersistenceID validation, or orphan cleanup. RuneSchema does not currently use
a live-component fallback for magic; combat and utility spell data continue
through their established native registries.

The startup log prints `[COMBAT-REGISTRY][FALLBACK-SETTINGS]` with the active
choices.
