# Nameplates loader

## Step-by-step workflow

Start with the example closest to your idea. Replace its sample paths and IDs
with values from your game, then use the
[testing checklist](../../modders/testing-and-compatibility.md) to try it out before sharing.

1. Prepare a mounted icon and decide which players should see the badge.
2. Create `nameplates/10-camp-badge.jsonc` using the reusable definition example.
3. Replace ID, icon and distance; add only confirmed event/function filters if dynamic states are needed.
4. Reference the definition from players, restart and verify self/remote visibility, distance, death and respawn.

**Related guides:** [Players](players.md) selects recipients; [PAK content](../../PAK-CONTENT-GUIDE.md) supplies new artwork.


**Folder:** `RuneSchema/mods/<ModName>/nameplates/`

Reusable player nameplate and activity-badge presentation.

[← Loader reference](../../LOADER-WALKTHROUGHS.md)

Nameplates are reusable definitions consumed by `/players`.

```jsonc
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

---

[← All loaders](../../LOADER-WALKTHROUGHS.md) · [Authoring guide](../../AUTHORING-GUIDE.md)
