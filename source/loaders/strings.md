# Strings loader

**Folder:** `RuneSchema/mods/<ModName>/strings/`

Source-text replacement within global or named scopes.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

Strings replace matching source text globally or within a named table.

```json
{
  "DT_SomeTextTable": {
    "Old text": "New text"
  }
}
```

A replacement may also be a list where the loader supports multiple results.
Exact source text matters. Later loaded replacements win within the same scope.
This loader does not patch string-table keys.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
