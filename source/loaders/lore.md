# Lore loader

**Folder:** `RuneSchema/mods/<ModName>/lore/`

Readable lore entries and pages using the journal registry.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

`/lore` uses the journal registry but fixes the entry type to lore.

```json
{
  "RS_MyBook": {
    "Type": "Lore",
    "DisplayName": "My Book",
    "PageDescriptions": [
      {"Description":"Page one."},
      {"Description":"Page two."}
    ],
    "Unlock": false
  }
}
```

Open the entry from a supported dialogue lore action or unlock it normally.
Cooked entries may use `$declaration` for ownership tracking.

## Simple rules

- `/lore` uses the journal registry but fixes the entry type to lore.
- Lore may be unlocked normally or opened through a supported dialogue lore action.
- Cooked entries may use `$declaration` for ownership tracking.

## FAQ

### FAQ-LORE-001 — Does lore use a separate registry from journal entries? {#faq-lore-001}

No. Lore uses the journal registry with the entry type fixed to lore.

### FAQ-LORE-002 — Can dialogue open a lore entry? {#faq-lore-002}

Yes. Use a supported dialogue lore action, or unlock the lore entry normally.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
