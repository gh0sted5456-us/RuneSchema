# Equipment behavior and asset ownership

Native effect grants, equipment properties and item perk icons belong in `/assets`. Use existing `GrantedEffects` and `BuffDatas` fields. Native equipment owns applying/removing its effects. `/equipment` only configures validated runtime behavior that ordinary data edits cannot express; it neither grants effects nor patches assets. No new loader or cross-loader callback is introduced.

## Configurable Shadowveil actions

```json
{
  "ShadowveilWearables": {
    "/Game/RuneSchema/ShadowPaladinSet/Items/rs_shadow_paladin_cloak.rs_shadow_paladin_cloak": {
      "PreserveOn": ["MeleeAttack", "RangedAttack", "MagicAttack", "UtilityCast", "Evade"]
    }
  }
}
```

Choose any subset. Each list replaces the previous list for that item; `false` or an empty list disables protection. Other removal triggers, including damage/interactions, retain native behavior. Unequipping must still remove the effect. Restart after changing rules and re-equip when testing.

Existing `SurgeEvadeLegs` maps remain unchanged. Legacy `ShadowveilAttackEvadeWearables` boolean maps retain exactly melee/ranged/evade protection. Use only one Shadowveil format per document. Later files in existing mod order override earlier entries; AA_ mods precede normal mods and ZZ_ patches follow them. Unknown fields/actions, duplicates, invalid paths and excess capacity reject the whole document. Each behavior permits 64 exact asset paths.

The handler matches the native effect class, GE data class, wearable source and reflected source item data. It only skips selected delegate registrations. It does not suppress general removal, invoke ProcessEvent, retain UObject pointers, search all objects, poll, or reapply effects. Five binding/resume contracts are checked before selected hooks are installed; failure rolls back that handler. Unselected sites are not hooked.

Source matching and melee/ranged/evade were gameplay-confirmed earlier. Combat magic and utility suppression are new candidate behavior requiring gameplay validation.

## Cloak bonus and skill icon

`ZZ_ShadowveilActionProtection/assets/shadowcloak-bonus.json` appends native Ulv's Longbow `DamageModifierPlayerAttackingUnalertedAI` to the existing mantle. Exported data specifies an infinite 1.15 multiplier: 15% more damage against unalerted enemies. Remaining invisible does not necessarily make a previously alerted enemy unalerted.

The patch appends a Veiled Ambush item perk description using the game's Attack skill icon through `BuffDatas.BuffIcon`. This is the item perk display, not a new HUD status icon. It does not change shared GE presentation/stat defaults.

Use the same /assets pattern for other verified native equipment effects with compatible attributes. `$Append` preserves existing grants. The `$Patch` target is the clone's original authoring identity; /equipment uses its resulting runtime asset path. These identifiers differ intentionally.

The existing ShadowPaladinSet Blueprint patch that makes Shadowveil infinite is separate and global; this package does not change it. Normal cast Shadowveil is not protected because its source is not a configured wearable.

## Load order

PostEngineInit visits the cached ordered mod list. Equipment collects rules, then finalization installs selected hooks after all rule files merge. Assets queues ordinary edits/clones and deferred patches. At GameInstanceInit assets processes ordinary edits first, then deferred patches; successful pending edits are consumed. Equipment hooks do not need the cloak asset at installation: they match the source when the game later grants its effect.

Keep Equipment enabled for Surge/action protection and Assets enabled for the bonus. Missing assets do not prompt equipment to create them. Changes require restart; diagnostics remain manual.
