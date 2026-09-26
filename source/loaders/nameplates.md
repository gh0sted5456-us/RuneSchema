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

## Simple rules

- Nameplates are reusable presentation definitions consumed by `/players`.
- Use exact function paths and narrow parameter conditions for observed events.
- Test self, host, remote client, distance, inactivity timeout, death, and respawn.

## FAQ

### FAQ-NAMEPLATES-001 — Does a nameplate definition apply to players by itself? {#faq-nameplates-001}

No. Define it in `/nameplates`, then reference that definition from a
`/players` rule.

### FAQ-NAMEPLATES-002 — Can observed function events change a badge? {#faq-nameplates-002}

Yes. States and observed function events can change or pulse a badge when their
function paths and parameter conditions match.

### FAQ-NAMEPLATES-003 — Can I match a function by a loose or partial name? {#faq-nameplates-003}

Use the exact function path documented by the runtime and keep parameter
conditions narrow.

### FAQ-NAMEPLATES-004 — Can a nameplate react to player activity? {#faq-nameplates-004}

Yes. States and observed function events can change or pulse a badge.

### FAQ-NAMEPLATES-005 — What multiplayer cases should I test? {#faq-nameplates-005}

Test self, host, remote client, distance limits, inactivity timeout, death, and
respawn.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
