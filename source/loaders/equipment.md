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

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
