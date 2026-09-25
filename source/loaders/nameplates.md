# Nameplates loader

**Folder:** `RuneSchema/mods/<ModName>/nameplates/`

Reusable player nameplate and activity-badge presentation.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

Nameplates are reusable definitions consumed by `/players`.

```json
[
  {
    "Id": "ActivityBadge",
    "Nameplate": {
      "Mode": "Icon",
      "Icon": "/Game/MyMod/UI/T_Badge.T_Badge",
      "Distance": 2500,
      "Client": "Yes",
      "Server": "Yes"
    }
  }
]
```

States and observed function events can change or pulse a badge. Use exact
function paths and narrow parameter conditions. Test self, host, remote client,
distance, inactivity timeout, death, and respawn.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
