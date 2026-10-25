# Effects loader

## Step-by-step workflow

Examples with placeholder paths/IDs are templates. Use inspected values and the
[shared testing checklist](../../modders/testing-and-compatibility.md) before sharing.

1. Cook and mount a GameplayEffect class with the native behavior your feature requires.
2. Create `effects/10-camp-effects.jsonc`; replace the mock-up's class path with that mounted generated class.
3. Give the class a stable reusable effect alias; reference it only from a consumer that supports effect IDs.
4. Restart and verify application/removal through the consumer. An alias does not create a class or edit its defaults.

**Tie-ins:** [Equipment](equipment.md) can consume granted effects; [players](players.md) provides supported player effect rules.


**Folder:** `RuneSchema/mods/<ModName>/effects/`

Reusable IDs for cooked GameplayEffect classes.

[← Loader reference](../../LOADER-WALKTHROUGHS.md)

`/effects` assigns virtual IDs to cooked GameplayEffect classes.

```jsonc
{
  "Movement/Dash": {
    "Class": "/Game/MyMod/Effects/GE_Dash.GE_Dash_C"
  }
}
```

Reference the definition as `MyMod:Effects/Movement/Dash` or use the full
class path where a consumer permits it. This loader does not clone or patch
effect class defaults.

## Simple rules

- `/effects` gives a reusable RuneSchema ID to an already cooked GameplayEffect class.
- Consumers may reference `ModName:EffectId` or a full class path where that consumer permits it.
- This loader does not clone or edit GameplayEffect class defaults.

## FAQ

### FAQ-EFFECTS-001 — Can the Effects loader create a GameplayEffect class? {#faq-effects-001}

No. The GameplayEffect class must already be cooked and mounted. The loader
creates a reusable reference ID for it.

### FAQ-EFFECTS-002 — Can I patch GameplayEffect class defaults here? {#faq-effects-002}

No. The Effects loader is a catalogue/alias layer; it does not clone or patch
the effect class defaults.

---

[← All loaders](../../LOADER-WALKTHROUGHS.md) · [Authoring guide](../../AUTHORING-GUIDE.md)
