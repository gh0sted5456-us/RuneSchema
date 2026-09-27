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

## Simple rules

- `/strings` replaces matching source text globally or within a named scope.
- Source text matching is exact.
- Later replacements win within the same scope; this loader does not patch string-table keys.

## FAQ

### FAQ-STRINGS-001 — Does the Strings loader edit string-table keys? {#faq-strings-001}

No. It replaces matching source text; it does not patch string-table keys.

### FAQ-STRINGS-002 — Does the original text have to match exactly? {#faq-strings-002}

Yes. Exact source text matters for the replacement to match.

### FAQ-STRINGS-003 — Which replacement wins if more than one matches the same scope? {#faq-strings-003}

The later loaded replacement wins within that scope.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
