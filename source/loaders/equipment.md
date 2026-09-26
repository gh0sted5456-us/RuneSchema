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

## Simple rules

- Bind supported equipment behavior to worn item data paths.
- `GrantedEffects` supports `Replace`, `Append`, and `Clear`.
- Use item data paths, not executable addresses, and test the full equip/respawn/reconnect cycle.

## FAQ

### FAQ-EQUIPMENT-001 — Can I add an effect without replacing existing granted effects? {#faq-equipment-001}

Yes. Use `GrantedEffects` with `Mode: "Append"`.

### FAQ-EQUIPMENT-002 — What path should an equipment rule target? {#faq-equipment-002}

Target the worn item's data path. Do not use executable addresses.

### FAQ-EQUIPMENT-003 — What GrantedEffects modes are supported? {#faq-equipment-003}

`Replace`, `Append`, and `Clear`.

### FAQ-EQUIPMENT-004 — Can equipment reference a RuneSchema effect alias? {#faq-equipment-004}

Yes. The documented example uses a RuneSchema effect definition from
`/effects`.

### FAQ-EQUIPMENT-005 — What lifecycle cases should I test? {#faq-equipment-005}

Test equip, unequip, death, respawn, reconnect, and remote-client presentation.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
