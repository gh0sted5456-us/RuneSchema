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

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
