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

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
