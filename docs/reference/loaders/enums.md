# Enums loader

## Step-by-step workflow

Examples with placeholder paths/IDs are templates. Use inspected values and the
[shared testing checklist](../../modders/testing-and-compatibility.md) before sharing.

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

Native `/Script/Dominion.ESkill` extension is now explicitly blocked. Its
compiled MAX checks, switches, arrays and network encoding are not verified
for added values; its menu ordering also uses an explicit native enum list.
Do not reuse Magic, deprecated slots, or a guessed next byte for a custom skill.

For independent mod enums, cook a mod-owned `UserDefinedEnum` and use that
exact enum type in mod-owned cooked properties. Declare its full path through
`NativeRegistries.Enums` in a registry document to verify its existing symbols
and include its owner-scoped fingerprint in network agreement. This is not an
append operation or an arbitrary property/memory patch. The values are fixed
by cooking and are not reassigned by mod load order. Keep their mapping stable
when publishing updates; changed tables produce changed fingerprints.

## FAQ

### FAQ-ENUMS-001 — Should enum values include the EnumName:: prefix? {#faq-enums-001}

No. Supply only the value name.

### FAQ-ENUMS-002 — Does adding an enum name create logic for the new value? {#faq-enums-002}

No. The loader extends the loaded name set only. Code or Blueprint behavior
must already know how to handle that value.

---

[← All loaders](../../LOADER-WALKTHROUGHS.md) · [Authoring guide](../../AUTHORING-GUIDE.md)
