# NPC loader

**Folder:** `RuneSchema/mods/<ModName>/npc/`

Creates persistent interactable AI, human, resource-style actors, and merchants.

`Location.Z: "$"` prefers landscape beneath overhead roofs or bridges. To place
an NPC on a particular stair landing, bridge deck or upper floor, add an
approximate `GroundReferenceZ` alongside a `$` location. The shared floor query
allows a 45 cm step upward, rejects wall/underside hits and initial overlaps,
and requires real blocking collision before applying `$+offset`/`$-offset`.
A numeric NPC Z remains an exact authored placement. See the
[stacked-surface grounding rules](spawns.md) for terrain and streaming limits.

[← Loader reference](../../LOADER-WALKTHROUGHS.md)

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
- The 7.8 alpha's `JournalEntries` list requests native player discovery on interaction without replacing the NPC's shop or dialogue. Use local journal/lore IDs, `Mod:Id`, or full cooked journal paths; see the [journal loader](journal.md). `NoInteract` suppresses these bindings.
- `HideWeapon` suppresses inherited weapon presentation on supported human NPCs.
- Human `Equipment` accepts `Necklace` for items whose native slot strategy is
  `Necklace`, alongside the existing `Trinket` slot. `Trinket2` is an alias for
  `Necklace`; do not specify both. Slot validation uses the live item's native
  strategy, so a necklace cannot be assigned to `Trinket`.
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

- [Great Tree](../../examples/GreatTree/npc/85-GreatTree.json)

---

[← All loaders](../../LOADER-WALKTHROUGHS.md) · [Authoring guide](../../AUTHORING-GUIDE.md)
