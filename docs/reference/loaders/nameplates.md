# Nameplates loader

## Step-by-step workflow

Use `RuneSchema/mods/MyMod/nameplates/` for this example. JSONC blocks are
**mock-ups**, not an automatically installed feature: replace placeholder paths,
IDs, coordinates and reflected fields with values from your content. New art or
classes must be cooked/mounted separately. Keep published identities stable.

1. Prepare a mounted icon and decide which players should see the badge.
2. Create `nameplates/10-camp-badge.jsonc` using the reusable definition example.
3. Replace ID, icon and distance; add only confirmed event/function filters if dynamic states are needed.
4. Reference the definition from players, restart and verify self/remote visibility, distance, death and respawn.

**Tie-ins:** [Players](players.md) selects recipients; [PAK content](../../PAK-CONTENT-GUIDE.md) supplies new artwork.


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
