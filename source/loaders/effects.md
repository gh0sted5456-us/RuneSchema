# Effects loader

**Folder:** `RuneSchema/mods/<ModName>/effects/`

Reusable IDs for cooked GameplayEffect classes.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

`/effects` assigns virtual IDs to cooked GameplayEffect classes.

```json
{
  "Movement/Dash": {
    "Class": "/Game/MyMod/Effects/GE_Dash.GE_Dash_C"
  }
}
```

Reference the definition as `MyMod:Effects/Movement/Dash` or use the full
class path where a consumer permits it. This loader does not clone or patch
effect class defaults.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
