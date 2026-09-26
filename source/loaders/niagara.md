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

## Simple rules

- A Niagara definition wraps an already cooked Niagara system and its attachment settings.
- Only `User.*` parameters are accepted.
- Supported parameter values are Boolean, number, vector, and color; verify the effect on every client.

## FAQ

### FAQ-NIAGARA-001 — Can the Niagara loader create a Niagara system? {#faq-niagara-001}

No. The Niagara system must already be cooked and mounted; the loader defines
how RuneSchema references and attaches it.

### FAQ-NIAGARA-002 — Can I set arbitrary Niagara parameters? {#faq-niagara-002}

No. Only `User.*` parameters are accepted by this loader.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
