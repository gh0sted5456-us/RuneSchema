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

## Working examples

- [Great Tree: resource-style world actor](../examples/GreatTree/npc/85-GreatTree.json)

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
