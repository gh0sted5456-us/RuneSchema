# NPC loader

**Folder:** `RuneSchema/mods/<ModName>/npc/`

Creates persistent interactable AI, human, resource-style actors, and merchants.

[← Loader reference](../LOADER-WALKTHROUGHS.md)

```jsonc
{
  "Id": "merchant",
  "Type": "AI",
  "DisplayName": "Merchant",
  "VendorID": "shop",
  "DialogueID": "hello",
  "VisualSource": "/Game/Gameplay/NPCs/BP_BaseInteractableNPC.BP_BaseInteractableNPC_C",
  "Location": [1000, 2000, "$+10"]
}
```

NPC definitions use one network-neutral format. Do not add a `Multiplayer`
field. In standalone RuneSchema runs the definition locally; in multiplayer the
server owns creation and clients prepare the replicated presentation.

## Rules

- Choose a compatible actor role and visual source.
- Bind `VendorID`, `DialogueID`, `QuestID`, or lore only when needed.
- `HideWeapon` suppresses inherited weapon presentation on supported human NPCs.
- Use `/buildings` for buildable architecture and `/spawns` for general world
  placement rather than using NPCs as generic props.

## FAQ

### Can an NPC act as a merchant?

Yes. Bind a RuneSchema store with `VendorID`. `DialogueID` is optional.

### Do I need separate single-player and multiplayer NPC files?

No. Use one definition.

### Can I hide an inherited weapon?

Yes, on supported human NPCs, with `HideWeapon`.

## Example

- [Great Tree](../examples/GreatTree/npc/85-GreatTree.json)

---

[← All loaders](../LOADER-WALKTHROUGHS.md) · [Authoring guide](../AUTHORING-GUIDE.md)
