# Enums loader

## Step-by-step workflow

Use `RuneSchema/mods/MyMod/enums/` for this example. JSONC blocks are
**mock-ups**, not an automatically installed feature: replace placeholder paths,
IDs, coordinates and reflected fields with values from your content. New art or
classes must be cooked/mounted separately. Keep published identities stable.

1. Identify a supported loaded enum and confirm the consumer already handles the proposed values.
2. Create `enums/10-camp-values.jsonc` using the mock-up below.
3. Replace the enum name and supply unqualified value names; do not include EnumName:: in each value.
4. Restart and inspect enum resolution and the consuming feature. Added names do not implement new game logic.

**Tie-ins:** [Raw](raw.md) and [assets](assets.md) may reference supported enum values; use cooked code/Blueprint content for new behavior.


**Folder:** `RuneSchema/mods/<ModName>/enums/`

Extensions to supported loaded enum name sets.

[← Loader reference](../../LOADER-WALKTHROUGHS.md)

Use `/enums` to add names to a supported loaded enum.

```jsonc
{
  "EMyLoadedEnum": ["NewValue", "AnotherValue"]
}
```

Write values without the `EnumName::` prefix. This changes the loaded enum
name set; it does not add native code or Blueprint logic for a new value.

## Simple rules

- Use `/enums` only for supported loaded enum name sets.
- Write new values without the `EnumName::` prefix.
- Adding a name does not add native or Blueprint behavior for that value.

## FAQ

### FAQ-ENUMS-001 — Should enum values include the EnumName:: prefix? {#faq-enums-001}

No. Supply only the value name.

### FAQ-ENUMS-002 — Does adding an enum name create logic for the new value? {#faq-enums-002}

No. The loader extends the loaded name set only. Code or Blueprint behavior
must already know how to handle that value.

---

[← All loaders](../../LOADER-WALKTHROUGHS.md) · [Authoring guide](../../AUTHORING-GUIDE.md)
