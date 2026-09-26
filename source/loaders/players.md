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

## Simple rules

- Player rules may select by player name, GUID, or wildcard.
- Use GUID selectors when player names are not unique.
- Appearance rows and cooked presentation assets must be installed on every client; test respawn because player objects can be recreated.

## FAQ

### FAQ-PLAYERS-001 — How do I target every player? {#faq-players-001}

Use the wildcard player selector, for example `"PlayerName": "*"`.

### FAQ-PLAYERS-002 — Should I target a player by name or GUID? {#faq-players-002}

Use a GUID when names may not be unique. Name and wildcard selectors are useful
when that distinction is acceptable.

### FAQ-PLAYERS-003 — Why should I test player rules after respawn? {#faq-players-003}

Player pawns and components can be recreated, so a rule that looks correct on
initial spawn also needs a respawn test.

### FAQ-PLAYERS-004 — Can player appearance assets exist only on the server? {#faq-players-004}

No. Cooked appearance assets and referenced customization rows must be
available on every client that renders them.

### FAQ-PLAYERS-005 — Can a player rule reference a reusable nameplate definition? {#faq-players-005}

Yes. Reference the definition created under `/nameplates`.

### FAQ-PLAYERS-006 — Can one rule select by wildcard instead of a specific player? {#faq-players-006}

Yes. The documented wildcard selector is `"PlayerName": "*"`.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
