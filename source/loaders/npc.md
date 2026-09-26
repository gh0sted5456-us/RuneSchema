# NPC loader

**Folder:** `RuneSchema/mods/<ModName>/npc/`

Persistent interactable AI, human, and resource-style actors.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

`/npc` creates persistent AI, human, or resource-style actors.

```jsonc
{
  "Id": "merchant",
  "Type": "AI",
  "Multiplayer": true,
  "DisplayName": "Merchant",
  "VendorID": "shop",
  "DialogueID": "hello",
  "VisualSource": "/Game/Gameplay/NPCs/BP_BaseInteractableNPC.BP_BaseInteractableNPC_C",
  "Location": [1000, 2000, "$+10"],
  "PowerLevel": 20
}
```

Choose one actor role, provide a compatible visual source, and bind dialogue or
vendor IDs. `HideWeapon` can suppress inherited weapon presentation. Prefer
`/buildings` or `/spawns` for static architecture rather than using an NPC as a
prop.

## Simple rules

- `/npc` creates persistent AI, human, or resource-style actors.
- Choose one actor role, use a compatible visual source, and bind dialogue or vendor IDs as needed.
- Prefer `/buildings` or `/spawns` for static architecture.

## FAQ

### FAQ-NPC-001 — Can an NPC definition act as a merchant? {#faq-npc-001}

Yes. Bind a RuneSchema vendor with `VendorID`, and optionally bind dialogue
with `DialogueID`.

### FAQ-NPC-002 — Should I use NPC for a static prop or building? {#faq-npc-002}

Normally no. Use `/buildings` for buildable content or `/spawns` for
supported world placement.

### FAQ-NPC-003 — Can I hide a weapon inherited from the visual source? {#faq-npc-003}

Yes. `HideWeapon` can suppress inherited weapon presentation.

### FAQ-NPC-004 — Can /npc create resource-style world actors? {#faq-npc-004}

Yes. The NPC loader supports persistent AI, human, and resource-style actors.

### FAQ-NPC-005 — Can I give an NPC both dialogue and a vendor? {#faq-npc-005}

Yes. Bind `DialogueID` and `VendorID` on the same supported NPC definition.

### FAQ-NPC-006 — Can NPC PowerLevel be authored? {#faq-npc-006}

Yes. `PowerLevel` is part of the documented NPC definition shape.

## Working examples

- [Great Tree: resource-style world actor](../examples/GreatTree/npc/85-GreatTree.json)

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
