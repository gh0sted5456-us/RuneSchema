# Enums loader

**Folder:** `RuneSchema/mods/<ModName>/enums/`

Extensions to supported loaded enum name sets.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

Use `/enums` to add names to a supported loaded enum.

```json
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

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
