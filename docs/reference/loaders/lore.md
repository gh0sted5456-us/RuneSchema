# Lore loader

## Step-by-step workflow

Use `RuneSchema/mods/MyMod/lore/` for this example. JSONC blocks are
**mock-ups**, not an automatically installed feature: replace placeholder paths,
IDs, coordinates and reflected fields with values from your content. New art or
classes must be cooked/mounted separately. Keep published identities stable.

1. Write the pages and choose a stable mod-owned lore key, or resolve an existing cooked lore asset.
2. Create `lore/10-camp-book.jsonc`; keep Type as Lore and replace the mock-up page text.
3. Choose normal discovery or a supported dialogue/NPC lore binding. Use a compatible book prop if you want read-state presentation.
4. Restart and verify discovery, actual viewing, open/non-glowing read presentation and saved progress separately.

**Tie-ins:** [Journal](journal.md) explains native placement/discovery; [NPCs](npc.md) and [dialogue](dialogue.md) provide supported interactions.


**Folder:** `RuneSchema/mods/<ModName>/lore/`

Readable lore entries and pages using the journal registry.

[← Loader reference](../../LOADER-WALKTHROUGHS.md)

`/lore` uses the journal registry but fixes the entry type to lore.

```jsonc
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
Cooked entries are registered from their live persistent identities; no ownership declaration is required.

Lore uses the same persistence setting and missing-mod cleanup as journal
entries on Steam/Epic and Game Pass.

## Overworld book helpers

A `/npc` `Type: "Prop"` with `LoreID`/`LoreEntry` and the native
`SM_Lore_Book_01` or `SM_Lore_Book_02` mesh automatically follows the local
player's journal read state. Unread books use the closed mesh and native
`NS_LoreItem_Book` glow; read books use the open mesh without that glow.
Unlocking an entry alone does not count as reading it. The visual check reads
the native unlocked/unread arrays and never changes another player's progress.
Opening the helper unlocks its entry through the normal journal contract and
then requests the native lore popup; merely requesting the popup is not a read
receipt. Unknown/not-ready state keeps the existing appearance.

Use `"LorePresentation":{"Enabled":false}` to preserve a fixed prop, or provide
`Enabled: true`, `UnreadMesh`, `ReadMesh`, and optional `GlowSystem` for custom
mounted art. An empty `GlowSystem` disables this helper's unread glow. Custom
meshes do not automatically opt in. This controls the overworld helper, not
Helpy's icon cache. Native component/signature checks remain active. A live
read/re-entry test is still required on each runtime lane.

## Simple rules

- `/lore` uses the journal registry but fixes the entry type to lore.
- Lore may be unlocked normally or opened through a supported dialogue lore action.
- Cooked entries require no ownership declaration.

## FAQ

### FAQ-LORE-001 — Does lore use a separate registry from journal entries? {#faq-lore-001}

No. Lore uses the journal registry with the entry type fixed to lore.

### FAQ-LORE-002 — Can dialogue open a lore entry? {#faq-lore-002}

Yes. Use a supported dialogue lore action, or unlock the lore entry normally.

---

[← All loaders](../../LOADER-WALKTHROUGHS.md) · [Authoring guide](../../AUTHORING-GUIDE.md)
