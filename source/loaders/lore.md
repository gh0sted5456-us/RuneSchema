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

### FAQ-LORE-003 — Can a lore entry stay locked until dialogue opens it? {#faq-lore-003}

Yes. Keep `Unlock:false` and open it through a supported dialogue lore action.

### FAQ-LORE-004 — Can cooked lore content use $declaration? {#faq-lore-004}

Yes. Cooked entries may use `$declaration` for RuneSchema ownership tracking.

### FAQ-LORE-005 — Can lore entries have multiple pages? {#faq-lore-005}

Yes. Lore uses the journal registry and supports multiple page descriptions.

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
