# Equipment loader

**Folder:** `RuneSchema/mods/<ModName>/equipment/`

Wear-triggered effects and supported equipment behavior.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

Use `/equipment` to bind supported behavior to worn item paths.

```json
{
  "SurgeEvadeLegs": {
    "/Game/MyMod/Items/ITEM_DashLegs.ITEM_DashLegs": true
  },
  "GrantedEffects": {
    "/Game/MyMod/Items/ITEM_DashLegs.ITEM_DashLegs": {
      "Mode": "Append",
      "Effects": ["MyMod:Effects/Movement/Dash"]
    }
  }
}
```

`GrantedEffects` supports `Replace`, `Append`, and `Clear`. Use item data paths,
not executable addresses. Validate equip, unequip, death, respawn, reconnect,
and client presentation.

The loader also accepts supported item fields for native actions, skills,
projectiles, block effects, tags, durability behavior, and other fields that
exist on the selected equipment item. Surge-style movement and
Shadowveil-style presentation use storefront-specific game support; if that
support is unavailable, RuneSchema disables only that behavior and continues
loading the remaining equipment changes.

## Simple rules

- Bind supported equipment behavior to worn item data paths.
- `GrantedEffects` supports `Replace`, `Append`, and `Clear`.
- Use item data paths, not executable addresses, and test the full equip/respawn/reconnect cycle.

## FAQ

### FAQ-EQUIPMENT-001 — Can I add an effect without replacing existing granted effects? {#faq-equipment-001}

Yes. Use `GrantedEffects` with `Mode: "Append"`.

### FAQ-EQUIPMENT-002 — What path should an equipment rule target? {#faq-equipment-002}

Target the worn item's data path. Do not use executable addresses.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
