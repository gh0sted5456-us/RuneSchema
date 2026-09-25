# Players loader

**Folder:** `RuneSchema/mods/<ModName>/players/`

Player selectors, rules, attributes, appearance, effects, and presentation.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

Player rules select by name, GUID, or wildcard and apply supported attributes,
appearance, effects, archetypes, map icons, and nameplates.

```json
[
  {
    "Id": "all-players",
    "PlayerName": "*",
    "Scale": 1.0,
    "Nameplate": {"Definition":"ActivityBadge","ShowSelf":true}
  }
]
```

Use GUID selectors when names are not unique. Keep appearance rows and cooked
assets installed on every client. Test respawn because player pawns and
components can be recreated.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
