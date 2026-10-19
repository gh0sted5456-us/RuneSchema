# Players loader

## Step-by-step workflow

Use `RuneSchema/mods/MyMod/players/` for this example. JSONC blocks are
**mock-ups**, not an automatically installed feature: replace placeholder paths,
IDs, coordinates and reflected fields with values from your content. New art or
classes must be cooked/mounted separately. Keep published identities stable.

1. Choose a name, GUID or wildcard selector; use a GUID where duplicate names matter.
2. Create `players/10-camp-profile.jsonc` from the mock-up and replace the selector/definition references.
3. Add only supported attributes/presentation. Choose one carry-capacity mode and inspect competing matching rules.
4. Restart and verify the selected players only, host/client presentation and recreated pawns after death/reconnect.

**Tie-ins:** [Nameplates](nameplates.md), [effects](effects.md), [Niagara](niagara.md) and [equipment](equipment.md).


**Folder:** `RuneSchema/mods/<ModName>/players/`

Player selectors, rules, attributes, appearance, effects, and presentation.

[← Loader reference](../../LOADER-WALKTHROUGHS.md)

Player rules select by name, GUID, or wildcard and apply supported attributes,
appearance, effects, archetypes, map icons, and nameplates.

```jsonc
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

Carry capacity uses `CarryWeightMultiplier` (relative to the pawn's captured
baseline) **or** `MaxCarryWeight` (an absolute capacity), not both in one rule.
These change maximum capacity, not the weight of individual inventory items.
If several matching rules set capacity, the last successfully applied rule wins;
multipliers do not stack. For example, a later player profile with `1.4` overrides
an earlier inventory mod with `3.0`. Remove the competing field or change load
order deliberately; a restart does not change that precedence.

Carry diagnostics include the captured baseline, requested capacity and value
read back after the native attribute notification. Conflicting matching rules
produce a deduplicated `[PLAYERS][CARRY-OVERRIDE]` warning. A rejected or overwritten
native value is not reported as successfully applied.

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

---

[← All loaders](../../LOADER-WALKTHROUGHS.md) · [Authoring guide](../../AUTHORING-GUIDE.md)
