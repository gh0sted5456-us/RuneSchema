# Niagara loader

## Step-by-step workflow

Start with the example closest to your idea. Replace its sample paths and IDs
with values from your game, then use the
[testing checklist](../../modders/testing-and-compatibility.md) to try it out before sharing.

1. Cook/mount the system and inspect the exact User.* parameters it exposes.
2. Create `niagara/10-camp-aura.jsonc`; replace the mock-up system path and attachment socket.
3. Set supported Boolean/number/vector/color parameters with the correct types; tune offsets on the real skeleton.
4. Reference the definition through a supported visual-effect consumer and verify attachment/removal on each client.

**Related guides:** [Equipment](equipment.md), [NPCs](npc.md) and [players](players.md) supply supported consumers; this loader does not create a system.


**Folder:** `RuneSchema/mods/<ModName>/niagara/`

Cooked Niagara systems, attachment settings, and user parameters.

[← Loader reference](../../LOADER-WALKTHROUGHS.md)

Niagara definitions wrap a cooked system and attachment settings.

```jsonc
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

[← All loaders](../../LOADER-WALKTHROUGHS.md) · [Authoring guide](../../AUTHORING-GUIDE.md)
