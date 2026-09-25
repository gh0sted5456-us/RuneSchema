# Niagara loader

**Folder:** `RuneSchema/mods/<ModName>/niagara/`

Cooked Niagara systems, attachment settings, and user parameters.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

Niagara definitions wrap a cooked system and attachment settings.

```json
{
  "NightAura": {
    "System": "/Game/MyMod/VFX/NS_NightAura.NS_NightAura",
    "Socket": "root",
    "AutoActivate": true,
    "LocationOffset": {"X":0,"Y":0,"Z":0},
    "Parameters": {"User.Intensity":1.0}
  }
}
```

Only `User.*` parameters are accepted. Supported values are Boolean, number,
vector, and color. Reference the definition from a consumer's visual-effect
block and verify it on every client.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
